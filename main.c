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

int signum(float n) {
	return (n > 0) - (n < 0);
}

int main(int argc, char *argv[]) {
	// SOURCE FILE
	SNDFILE *source_file;
	SF_INFO source_info;

	if (!(source_file = sf_open("testsound.wav", SFM_READ, &source_info))) {
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
	float *write_cache = malloc(2*sizeof(float));
	sf_count_t frames_read = 0;
	sf_count_t frames_written = 0;

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

		write_cache[0] = local_rms;
		write_cache[1] = local_rms;
		frames_written = sf_writef_float(dest_file, write_cache, 1);
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

	const float LOW_ENERGY_COEFFICIENT = 0.08; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679

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
		lowthres = LOW_ENERGY_COEFFICIENT*rms_sum;
		mler[long_frame] = 0;
		for (current_rms_frame = start_rms_frame; current_rms_frame < start_rms_frame + RMS_FRAMES_IN_LONG_FRAME; current_rms_frame++) {
			variance_difference_sum += pow(rms[current_rms_frame] - mean_rms[long_frame], 2);
			mler[long_frame] += signum(lowthres-rms[current_rms_frame]) + 1;
			variance_difference_sum += pow(rms[current_rms_frame] - mean_rms[long_frame], 2);
		}
		variance_rms[long_frame] = variance_difference_sum/RMS_FRAMES_IN_LONG_FRAME;
		norm_variance_rms[long_frame] = variance_rms[long_frame]/mean_rms[long_frame];
		mler[long_frame] = mler[long_frame]/(2*RMS_FRAMES_IN_LONG_FRAME);

		printf("Seconds: %d\n", long_frame);
		printf("Mean: %f\n", mean_rms[long_frame]);
		printf("Variance: %f\n", variance_rms[long_frame]);
		printf("Normalized variance: %f\n", norm_variance_rms[long_frame]);
		printf("MLER: %f\n\n", mler[long_frame]);

		start_rms_frame += RMS_FRAMES_IN_LONG_FRAME;
	}

	// CLEANUP
	int source_file_status = sf_close(source_file);
	sf_write_sync(dest_file);
	int dest_file_status = sf_close(dest_file);

	free(rms);
	free(read_cache);

	return source_file_status && dest_file_status;
}
