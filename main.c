// main.c
// ******************************
//
// podcastgen
// by Trygve Bertelsen Wiig, 2014
//
// This program detects speech and music
// using the algorithm given in
// http://www.speech.kth.se/prod/publications/files/3437.pdf.
//
// It then removes the music from the input file, and
// fades the speech sections into each other.

#include "main.h"

SNDFILE *source_file;
SNDFILE *dest_file;
SF_INFO source_info;
SF_INFO dest_info;

const int RMS_FRAME_DURATION = 20; // Length of the RMS calculation frames in milliseconds
const int LONG_FRAME_DURATION = 1000; // Length of the long (averaging) frames in milliseconds
int FRAMES_IN_RMS_FRAME;
int FRAMES_IN_LONG_FRAME;
int RMS_FRAME_COUNT;
int LONG_FRAME_COUNT;
int RMS_FRAMES_IN_LONG_FRAME;

float LOW_ENERGY_COEFFICIENT = 0.15; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679
float UPPER_MUSIC_THRESHOLD = 0.01; // MLER below this value => 1 second frame classified as music

char *input_path;
char *output_path;
char *file_folders;
char *filename;

bool verbose = false;
bool very_verbose = false;
bool has_intro = true;

int signum(float n) {
	return (n > 0) - (n < 0);
}

void split_path(char *full_path, char **folders, char **file) {
	char *slash = full_path;
	char *next;
	while (next = strpbrk(slash + 1, "\\/")) {
		slash = next;
	}
	if (full_path != slash) {
		slash++;
	}
	*folders = strndup(full_path, slash - full_path);
	*file = strdup(slash);
}

char *prettify_seconds(int start, int delta) {
	char *interval_string = malloc(50);

	int s_min = start/60;
	int s_sec = start - s_min*60;

	if (0 != delta) {
		int e_min = (start + delta)/60;
		int e_sec = (start + delta) - e_min*60;

		sprintf(interval_string, "%02d:%02d to %02d:%02d", s_min, s_sec, e_min, e_sec);
	} else {
		sprintf(interval_string, "%02d:%02d", s_min, s_sec);
	}

	return interval_string;
}

void open_source_file() {
	if (!(source_file = sf_open(input_path, SFM_READ, &source_info))) {
		printf("An error occured while opening the source file.\n");
		printf("%s\n", sf_strerror(NULL));
		exit(1);
	}
}

void open_dest_file() {
	output_path = malloc(200);

	strcpy(output_path, file_folders);
	strcat(output_path, "podcast_");
	strcat(output_path, filename);

	if (!(dest_file = sf_open(output_path, SFM_WRITE, &dest_info))) {
		printf("An error occured while opening the destination file.\n");
		printf("%s\n", sf_strerror(NULL));
		exit(1);
	}
}

char *interpret_args(int argc, char *argv[]) {
	const char *HELP_TEXT =
		"Syntax: podcastgen <options> <input file>\n"
		"\n"
		"	-h, --help\t\tDisplay this help text\n"
		"	-v\t\t\tVerbose output\n"
		"	-C\t\t\tSet the Low Energy Coefficient\n"
		"	-T\t\t\tSet the Upper Music Threshold\n"
		"	--very-verbose\t\tVERY verbose output\n"
		"	--no-intro\t\tRemove music at start of file\n\n";

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"very-verbose", no_argument, (int*) &very_verbose, true},
		{"no-intro", no_argument, (int*) &has_intro, false}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "vhC:T:", long_options, NULL)) != -1) {
		switch (opt) {
			case 'v':
				verbose = true;
				break;
			case 'h':
				printf(HELP_TEXT);
				exit(0);
			case 'C':
				LOW_ENERGY_COEFFICIENT = atof(optarg);
				break;
			case 'T':
				UPPER_MUSIC_THRESHOLD = atof(optarg);
				break;
			case '?':
				fprintf(stderr, "Unknown argument \'-%c\'.\n", optopt);
				exit(1);
		}
	}

	if (very_verbose) {
		verbose = true;
	}

	if (argc-optind == 1) {
		input_path = malloc(100);
		file_folders = malloc(100);
		filename = malloc(100);

		strcpy(input_path, argv[optind]);
		split_path(input_path, &file_folders, &filename);
	} else {
		printf("Please supply a source file.\n");
		exit(1);
	}
}

float *calculate_rms(float *rms) {
	float *read_cache = malloc(2*FRAMES_IN_RMS_FRAME*sizeof(float));
	sf_count_t frames_read = 0;

	int frame; // dummy variable for inner loop
	double local_rms; // temporary variable for inner loop

	for (int rms_frame = 0; rms_frame < RMS_FRAME_COUNT; rms_frame++) {
		frames_read = sf_readf_float(source_file, read_cache, FRAMES_IN_RMS_FRAME);
		local_rms = 0;

		for (frame = 0; frame < frames_read; frame++) {
			local_rms += pow(read_cache[frame], 2);
		}

		local_rms = sqrt(local_rms/RMS_FRAME_DURATION);
		rms[rms_frame] = local_rms;
	}

	free(read_cache);
	return rms;
}

float *calculate_features(float *rms, float *mean_rms, float *variance_rms, float *norm_variance_rms, float *mler) {

	int start_rms_frame = 0;
	int current_rms_frame = 0;

	float rms_sum;
	float variance_difference_sum; // sum of (x_i - mu)^2
	float lowthres; // used to compute the MLER value

	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		// We calculate four features for the 1 second interval:
		// - mean RMS
		// - variance of the RMS values
		// - normalized variance of the RMS values (i.e. variance divided by mean RMS)
		// - Modified Low Energy Ratio (MLER)
		rms_sum = 0;
		variance_difference_sum = 0;
		lowthres = 0;
	
		// Mean RMS
		for (current_rms_frame = start_rms_frame; current_rms_frame < start_rms_frame + RMS_FRAMES_IN_LONG_FRAME; current_rms_frame++) {
			rms_sum += rms[current_rms_frame];
		}
		mean_rms[long_frame] = rms_sum/RMS_FRAMES_IN_LONG_FRAME;

		// Variances and MLER
		lowthres = LOW_ENERGY_COEFFICIENT*mean_rms[long_frame];
		mler[long_frame] = 0;
		for (current_rms_frame = start_rms_frame; current_rms_frame < start_rms_frame + RMS_FRAMES_IN_LONG_FRAME; current_rms_frame++) {
			variance_difference_sum += pow(rms[current_rms_frame] - rms_sum, 2);
			mler[long_frame] += signum(lowthres-rms[current_rms_frame]) + 1;
			variance_difference_sum += pow(rms[current_rms_frame] - rms_sum, 2);
		}
		variance_rms[long_frame] = variance_difference_sum/RMS_FRAMES_IN_LONG_FRAME;
		norm_variance_rms[long_frame] = variance_rms[long_frame]/mean_rms[long_frame];
		mler[long_frame] = mler[long_frame]/(2*RMS_FRAMES_IN_LONG_FRAME);

		if (very_verbose) {
			printf("Seconds: %d\n", long_frame);
			printf("Mean: %f\n", mean_rms[long_frame]);
			printf("Variance: %f\n", variance_rms[long_frame]);
			printf("Normalized variance: %f\n", norm_variance_rms[long_frame]);
			printf("MLER: %f\n\n", mler[long_frame]);
		}

		start_rms_frame += RMS_FRAMES_IN_LONG_FRAME;
	}
}

int main(int argc, char *argv[]) {
	interpret_args(argc, argv);

	// Open files and set info
	open_source_file(filename);

	dest_info.samplerate = 44100;
	dest_info.channels = 2;
	dest_info.format = SF_FORMAT_WAV ^ SF_FORMAT_PCM_16 ^ SF_ENDIAN_FILE;

	open_dest_file(dest_file, &dest_info);

	if (verbose) {
		printf("Sample rate: %d\n", source_info.samplerate);
		printf("Frames: %d\n", (int) source_info.frames);
		printf("Channels: %d\n", source_info.channels);
		printf("%s minutes long.\n", prettify_seconds(source_info.frames/(double) source_info.samplerate, 0));
	}

	// Calculate Root-Mean-Square of source sound

	FRAMES_IN_RMS_FRAME = (source_info.samplerate*RMS_FRAME_DURATION)/1000; // (44100/1000)*20
	RMS_FRAME_COUNT = ceil(source_info.frames/(float) FRAMES_IN_RMS_FRAME);

	float *rms = malloc(2*RMS_FRAME_COUNT*sizeof(float));
	calculate_rms(rms);

	if (verbose) {
		puts("Calculated RMS!");
	}

	// Calculate RMS-derived features of long (1 second) frames

	FRAMES_IN_LONG_FRAME = (source_info.samplerate*LONG_FRAME_DURATION)/1000; // (44100/1000)*20
	LONG_FRAME_COUNT = ceil(source_info.frames/(float) FRAMES_IN_LONG_FRAME);
	RMS_FRAMES_IN_LONG_FRAME = LONG_FRAME_DURATION/RMS_FRAME_DURATION;

	float *mean_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *norm_variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *mler = malloc(2*LONG_FRAME_COUNT*sizeof(float));

	calculate_features(rms, mean_rms, variance_rms, norm_variance_rms, mler);

	if (verbose) {
		puts("Calculated features!");
	}

	// CLASSIFY
	bool *music = malloc(2*LONG_FRAME_COUNT*sizeof(bool));

	// Decide whether a given second segment is music or speech
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		if (mler[long_frame] < UPPER_MUSIC_THRESHOLD) {
			music[long_frame] = true;
		} else {
			music[long_frame] = false;
		}
	}

	// Check if there are other speech segments within the next 10 frames
	bool *music_second_pass = malloc(2*LONG_FRAME_COUNT*sizeof(bool));
	music_second_pass[0] = true;
	music_second_pass[1] = true;
	for (int long_frame = 2; long_frame < LONG_FRAME_COUNT-2; long_frame++) {
		music_second_pass[long_frame] = rint((music[long_frame-2]+music[long_frame-1]+music[long_frame]+music[long_frame+1]+music[long_frame+2])/5.0);
		//music_second_pass[long_frame] = music[long_frame];
	}
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		music[long_frame] = music_second_pass[long_frame];
	}
	free(music_second_pass);

	// Merge smaller segments with larger segments

	typedef struct segment {
		int startframe;
		int endframe;
		bool is_music;
	} segment;

	segment *segments = malloc(LONG_FRAME_COUNT*sizeof(segment));
	int current_segment = 0;
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		if (long_frame == 0) {
			segments[current_segment].startframe = 0;
			segments[current_segment].endframe = 0;
			segments[current_segment].is_music = true;
		} else if (music[long_frame] == segments[current_segment].is_music) {
			segments[current_segment].endframe++;
		} else {
			current_segment++;
			segments[current_segment].startframe = long_frame;
			segments[current_segment].endframe = long_frame;
			segments[current_segment].is_music = music[long_frame];
		}
	}

	segment *merged_segments = malloc(LONG_FRAME_COUNT*sizeof(segment));
	int current_merged_segment = 0;
	for (int seg = 0; seg < current_segment; seg++) {
		if (seg == 0) {
			merged_segments[0].startframe = segments[0].startframe;
			merged_segments[0].endframe = segments[0].endframe;
			if (has_intro) {
				merged_segments[0].is_music = false;
			}
		} else if (segments[seg].endframe - segments[seg].startframe < 10) {
			merged_segments[current_merged_segment].endframe = segments[seg].endframe;
		} else if (segments[seg].is_music == merged_segments[current_merged_segment].is_music) {
			merged_segments[current_merged_segment].endframe = segments[seg].endframe;
		} else {
			current_merged_segment++;
			merged_segments[current_merged_segment].startframe = segments[seg].startframe;
			merged_segments[current_merged_segment].endframe = segments[seg].endframe;
			merged_segments[current_merged_segment].is_music = segments[seg].is_music;
		}
	}
	free(segments);

	if (very_verbose) {
		for (int s = 0; s < current_merged_segment; s++) {
			printf("%d to %d: music? %d\n", merged_segments[s].startframe, merged_segments[s].endframe, merged_segments[s].is_music);
		}
	}

	// Finally, we write only the speech sections to the destination file
	float *cache = malloc(1);
	int frames_to_read;
	int voice_segment_number = 1;

	sf_count_t current_read_location;
	sf_count_t current_write_location;

	for (int s = 0; s < current_merged_segment; s++) {
		if (!merged_segments[s].is_music) {
			if (s == 0) {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+5)*FRAMES_IN_LONG_FRAME;
			} else {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe-5)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+10)*FRAMES_IN_LONG_FRAME;
			}
			if (verbose) {
				printf("%d: %s\n", voice_segment_number, prettify_seconds((int) current_read_location/(float) FRAMES_IN_LONG_FRAME, (int) frames_to_read/(float) FRAMES_IN_LONG_FRAME));
			}

			cache = realloc(cache, frames_to_read*2*sizeof(float));
			current_read_location = sf_readf_float(source_file, cache, frames_to_read);
			current_write_location = sf_writef_float(dest_file, cache, frames_to_read);

			sf_write_sync(dest_file);
			voice_segment_number++;
		}
	}

	// CLEANUP
	int source_file_status = sf_close(source_file);
	sf_write_sync(dest_file);
	int dest_file_status = sf_close(dest_file);

	free(music);
	free(rms);

	printf("Successfully generated %s.\n", output_path);

	return source_file_status || dest_file_status;
}
