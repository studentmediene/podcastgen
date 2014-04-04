// sound.c
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

#include <sndfile.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>

#include "sound.h"
#include "main.h"
#include "util.h"
#include "files.h"

const int RMS_FRAME_DURATION = 20; // Length of the RMS calculation frames in milliseconds
const int LONG_FRAME_DURATION = 1000; // Length of the long (averaging) frames in milliseconds
int FRAMES_IN_RMS_FRAME;
int FRAMES_IN_LONG_FRAME;
int RMS_FRAME_COUNT;
int LONG_FRAME_COUNT;
int RMS_FRAMES_IN_LONG_FRAME;

float LOW_ENERGY_COEFFICIENT = 0.15; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679
float UPPER_MUSIC_THRESHOLD = 0.01; // MLER below this value => 1 second frame classified as music

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

		logger(INFO, "Seconds: %d\n", long_frame);
		logger(INFO, "Mean: %f\n", mean_rms[long_frame]);
		logger(INFO, "Variance: %f\n", variance_rms[long_frame]);
		logger(INFO, "Normalized variance: %f\n", norm_variance_rms[long_frame]);
		logger(INFO, "MLER: %f\n\n", mler[long_frame]);

		start_rms_frame += RMS_FRAMES_IN_LONG_FRAME;
	}
}

void classify_segments(bool *is_music, float *mler) {
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		if (mler[long_frame] < UPPER_MUSIC_THRESHOLD) {
			is_music[long_frame] = true;
		} else {
			is_music[long_frame] = false;
		}
	}
}

void classify_segments2(bool *transition, float *mean_rms, float *variance_rms, float *norm_variance_rms, float *mler) {
	float *dissim = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *dissim_norm = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *a = malloc(2*LONG_FRAME_COUNT*sizeof(float));
	float *b = malloc(2*LONG_FRAME_COUNT*sizeof(float));

	float pmp_left = 0;
	float pmp_right_top = 0;
	float pmp_right_bottom = 0;
	float pmp = 0; // p(p_i-1, p_i+1)

	// We calculate a and b for every frame
	for (int lf = 0; lf < LONG_FRAME_COUNT; lf++) {
		a[lf] = (pow(mean_rms[lf],2)/pow(variance_rms[lf], 2)) - 1;
		b[lf] = pow(variance_rms[lf], 2)/mean_rms[lf];
	}

	// Calculate the dissimilarity measure for every frame
	dissim[0] = 0;
	for (int lf = 1; lf < LONG_FRAME_COUNT; lf++) {
		pmp_left = tgammaf((a[lf-1]+a[lf+1])/2.0 + 1)/sqrt(tgammaf(a[lf-1]+1)*tgammaf(a[lf+1]+1));
		pmp_right_top = pow(2, (a[lf-1] + a[lf+1])/(2.0) + 1) * pow(b[lf-1], (a[lf+1]+1)/2.0) * pow(b[lf+1], (a[lf-1]+1)/2.0);
		pmp_right_bottom = pow(b[lf-1] + b[lf+1], (a[lf-1]+a[lf+1])/2.0 + 1);
		pmp = pmp_left * (pmp_right_top/pmp_right_bottom);

		dissim[lf] = 1 - pmp;
	}

	free(a);
	free(b);

	// Calculate the normalized dissimilarity measure for every frame
	int lfc = LONG_FRAME_COUNT;
	dissim_norm[0] = (dissim[0]*(dissim[0]-(dissim[0]+dissim[0+1]+dissim[0+2])/3.0))/manymax(dissim[0], dissim[0+1], dissim[0+2]);
	dissim_norm[1] = (dissim[1]*(dissim[1]-(dissim[1-1]+dissim[1]+dissim[1+1]+dissim[1+2])/4.0))/manymax(dissim[1-1], dissim[1], dissim[1+1], dissim[1+2]);
	for (int lf = 2; lf < LONG_FRAME_COUNT-2; lf++) {
		dissim_norm[lf] = (dissim[lf]*(dissim[lf]-(dissim[lf-2]+dissim[lf-1]+dissim[lf]+dissim[lf+1]+dissim[lf+2])/5.0))/manymax(dissim[lf-2], dissim[lf-1], dissim[lf], dissim[lf+1], dissim[lf+2]);
	}
	dissim_norm[LONG_FRAME_COUNT-1] = (dissim[lfc]*(dissim[lfc]-(dissim[lfc-2]+dissim[lfc-1]+dissim[lfc]+dissim[lfc+1])/4.0))/manymax(dissim[lfc-2], dissim[lfc-1], dissim[lfc], dissim[lfc+1]);
	dissim_norm[LONG_FRAME_COUNT] = (dissim[lfc]*(dissim[lfc]-(dissim[lfc-2]+dissim[lfc-1]+dissim[lfc])/3.0))/manymax(dissim[lfc-2], dissim[lfc-1], dissim[lfc]);

	free(dissim);

	// Find transitions based on set threshold
	for (int lf = 0; lf < LONG_FRAME_COUNT; lf++) {
		if (dissim[lf] > 0.95) {
			transition[lf] = true;
			logger(NOTICE, "Transition at %s.", prettify_seconds(lf, 0));
		} else {
			transition[lf] = false;
		}
	}
}

void average_musicness(bool *is_music) {
	bool *music_second_pass = malloc(2*LONG_FRAME_COUNT*sizeof(bool));
	music_second_pass[0] = true;
	music_second_pass[1] = true;
	for (int long_frame = 2; long_frame < LONG_FRAME_COUNT-2; long_frame++) {
		music_second_pass[long_frame] = rint((is_music[long_frame-2]+is_music[long_frame-1]+is_music[long_frame]+is_music[long_frame+1]+is_music[long_frame+2])/5.0);
		//music_second_pass[long_frame] = is_music[long_frame];
	}
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		is_music[long_frame] = music_second_pass[long_frame];
	}
	free(music_second_pass);
}

int merge_segments(bool *is_music, segment *merged_segments) {
	segment *segments = malloc(LONG_FRAME_COUNT*sizeof(segment));
	int current_segment = 0;
	for (int long_frame = 0; long_frame < LONG_FRAME_COUNT; long_frame++) {
		if (long_frame == 0) {
			segments[current_segment].startframe = 0;
			segments[current_segment].endframe = 0;
			segments[current_segment].is_music = true;
		} else if (is_music[long_frame] == segments[current_segment].is_music) {
			segments[current_segment].endframe++;
		} else {
			current_segment++;
			segments[current_segment].startframe = long_frame;
			segments[current_segment].endframe = long_frame;
			segments[current_segment].is_music = is_music[long_frame];
		}
	}

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
	return current_merged_segment;
}
