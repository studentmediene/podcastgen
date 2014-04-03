// podcastgen
// by Trygve Bertelsen Wiig, 2014
//
// This program detects speech and music
// using the algorithm given in
// http://www.speech.kth.se/prod/publications/files/3437.pdf.
//
// It then removes the music from the input file, and
// fades the speech sections into each other.

#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef enum { false, true } bool;

int signum(float n) {
	return (n > 0) - (n < 0);
}

int main(int argc, char *argv[]) {
	// GET FILE TO OPEN FROM ARGUMENT
	const char* filename;
	if (argc > 1) {
		filename = argv[1];
	} else {
		printf("Please supply a source file.\n");
		return 1;
	}

	// SOURCE FILE
	SNDFILE *source_file;
	SF_INFO source_info;

	if (!(source_file = sf_open(filename, SFM_READ, &source_info))) {
		printf("An error occured while opening the source file.\n");
		printf("%s\n", sf_strerror(NULL));
		return 1;
	}

	// DESTINATION FILE
	SNDFILE *dest_file;
	SF_INFO dest_info;
	dest_info.samplerate = 44100;
	dest_info.channels = 2;
	dest_info.format = SF_FORMAT_WAV ^ SF_FORMAT_PCM_16 ^ SF_ENDIAN_FILE;

	if (!(dest_file = sf_open("testsound_out.wav", SFM_WRITE, &dest_info))) {
		printf("An error occured while opening the destination file.\n");
		printf("%s\n", sf_strerror(NULL));
		return 1;
	}

	// SOURCE FILE INFO
	printf("Sample rate: %d\n", source_info.samplerate);
	printf("Frames: %d\n", (int) source_info.frames);
	printf("Channels: %d\n", source_info.channels);
	printf("%f minutes long.\n", (source_info.frames/(double) source_info.samplerate)/60.0);

	// CALCULATE RMS

	int RMS_FRAME_LENGTH = 20; // milliseconds
	int FRAMES_IN_RMS_FRAME = (source_info.samplerate*RMS_FRAME_LENGTH)/1000; // (44100/1000)*20
	int RMS_FRAME_COUNT = ceil(source_info.frames/(float) FRAMES_IN_RMS_FRAME);
	
	float *rms = malloc(2*RMS_FRAME_COUNT*sizeof(float));
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
		local_rms = sqrt(local_rms/RMS_FRAME_LENGTH);

		rms[rms_frame] = local_rms;
	}

	puts("Calculated RMS...");

	// CALCULATE FEATURES OF LONG FRAMES

	const int LONG_FRAME_LENGTH = 1000; // milliseconds
	const int FRAMES_IN_LONG_FRAME = (source_info.samplerate*LONG_FRAME_LENGTH)/1000; // (44100/1000)*20
	const int LONG_FRAME_COUNT = ceil(source_info.frames/(float) FRAMES_IN_LONG_FRAME);

	int RMS_FRAMES_IN_LONG_FRAME = LONG_FRAME_LENGTH/RMS_FRAME_LENGTH;

	float *mean_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *norm_variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *mler = malloc(2*LONG_FRAME_COUNT*sizeof(float));

	int start_rms_frame = 0;
	int current_rms_frame = 0;

	float rms_sum;
	float variance_difference_sum; // sum of (x_i - mu)^2
	float lowthres; // used to compute the MLER value

	const float LOW_ENERGY_COEFFICIENT = 0.15; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679

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

		/*printf("Seconds: %d\n", long_frame);
		printf("Mean: %f\n", mean_rms[long_frame]);
		printf("Variance: %f\n", variance_rms[long_frame]);
		printf("Normalized variance: %f\n", norm_variance_rms[long_frame]);
		printf("MLER: %f\n\n", mler[long_frame]);*/

		start_rms_frame += RMS_FRAMES_IN_LONG_FRAME;
	}

	// CLASSIFY
	bool *music = malloc(2*LONG_FRAME_COUNT*sizeof(bool));
	bool *music_second_pass = malloc(2*LONG_FRAME_COUNT*sizeof(bool));

	// First pass: decide whether a given second segment is music or speech
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		if (mler[long_frame] < 0.01) {
			music[long_frame] = true;
		} else {
			music[long_frame] = false;
		}
	}

	// Second pass: check if there are other speech segments within the next 10 frames
	music_second_pass[0] = true;
	music_second_pass[1] = true;
	for (int long_frame = 2; long_frame < LONG_FRAME_COUNT-2; long_frame++) {
		music_second_pass[long_frame] = rint((music[long_frame-2]+music[long_frame-1]+music[long_frame]+music[long_frame+1]+music[long_frame+2])/5.0);
	}

	/*for (int long_frame = 0; long_frame < LONG_FRAME_COUNT-10; long_frame++) {
		if (music_second_pass[long_frame]) {
			puts("MUSIC");
		} else {
			puts("-----");
		}
	}*/
	free(music);

	// Third pass: merge smaller segments with larger segments

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
		} else if (music_second_pass[long_frame] == segments[current_segment].is_music) {
			segments[current_segment].endframe++;
		} else {
			current_segment++;
			segments[current_segment].startframe = long_frame;
			segments[current_segment].endframe = long_frame;
			segments[current_segment].is_music = music_second_pass[long_frame];
		}
	}

	segment *merged_segments = malloc(LONG_FRAME_COUNT*sizeof(segment));
	int current_merged_segment = 0;
	for (int seg = 0; seg < current_segment; seg++) {
		if (seg == 0) {
			merged_segments[0].startframe = segments[0].startframe;
			merged_segments[0].endframe = segments[0].endframe;
			merged_segments[0].is_music = false;
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

	for (int s = 0; s < current_merged_segment; s++) {
		printf("%d to %d: music? %d\n", merged_segments[s].startframe, merged_segments[s].endframe, merged_segments[s].is_music);
	}

	// Finally, we write only the speech sections to the destination file
	float *cache = malloc(1);
	int frames_to_read;
	sf_count_t current_read_location;
	sf_count_t current_write_location;
	puts("Writing...");
	for (int s = 0; s < current_merged_segment; s++) {
		if (merged_segments[s].is_music) {
			continue;
		} else {
			puts("Seeking...");
			if (s == 0) {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+5)*FRAMES_IN_LONG_FRAME;
			} else {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe-5)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+10)*FRAMES_IN_LONG_FRAME;
			}
			puts("Allocating...");
			cache = realloc(cache, frames_to_read*2*sizeof(float));
			puts("Reading...");
			current_read_location = sf_readf_float(source_file, cache, frames_to_read);
			puts("Writing...");
			current_write_location = sf_writef_float(dest_file, cache, frames_to_read);
			puts("Syncing...");
			sf_write_sync(dest_file);
			puts("Written!");
		}
	}

	// CLEANUP
	int source_file_status = sf_close(source_file);
	sf_write_sync(dest_file);
	int dest_file_status = sf_close(dest_file);

	free(rms);
	free(read_cache);

	return source_file_status && dest_file_status;
}
