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

double LOW_ENERGY_COEFFICIENT = 0.14; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679 for definition and value
double UPPER_MUSIC_THRESHOLD = 0.0; // MLER below this value => 1 second frame classified as music
double DISSIMILARITY_THRESHOLD = 0.10; // default value of 0.26 is due to Lars Ericsson

double *calculate_rms(double *rms) {
	double *read_cache = malloc(2*FRAMES_IN_RMS_FRAME*sizeof(double));
	sf_count_t frames_read = 0;

	int frame; // dummy variable for inner loop
	double local_rms; // temporary variable for inner loop

	for (int rms_frame = 0; rms_frame < RMS_FRAME_COUNT; rms_frame++) {
		frames_read = sf_readf_double(source_file, read_cache, FRAMES_IN_RMS_FRAME);
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

double *calculate_features(double *rms, double *mean_rms, double *variance_rms, double *norm_variance_rms, double *mler) {

	int start_rms_frame = 0;
	int current_rms_frame = 0;

	double rms_sum;
	double variance_difference_sum; // sum of (x_i - mu)^2
	double lowthres; // used to compute the MLER value

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

		logger(INFO, "Seconds: %d", long_frame);
		logger(INFO, "Mean: %f", mean_rms[long_frame]);
		logger(INFO, "Variance: %f", variance_rms[long_frame]);
		logger(INFO, "Normalized variance: %f", norm_variance_rms[long_frame]);
		logger(INFO, "MLER: %f\n", mler[long_frame]);

		start_rms_frame += RMS_FRAMES_IN_LONG_FRAME;
	}
}

int classify_segments2(segment *segments, double *mean_rms, double *variance_rms, double *norm_variance_rms, double *mler) {
	double *dissim = malloc(2*LONG_FRAME_COUNT*sizeof(double));
	double *a = malloc(2*LONG_FRAME_COUNT*sizeof(double));
	double *b = malloc(2*LONG_FRAME_COUNT*sizeof(double));

	double pmp_left = 0;
	double pmp_right_top = 0;
	double pmp_right_bottom = 0;
	double pmp = 0; // p(p_i-1, p_i+1)

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

	double *dissim_norm = malloc(2*LONG_FRAME_COUNT*sizeof(double));

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

	bool *transition = malloc(2*LONG_FRAME_COUNT*sizeof(bool));

	// Find transitions based on set threshold
	for (int lf = 0; lf < LONG_FRAME_COUNT; lf++) {
		printf("%s: %f\n", prettify_seconds(lf, 0), dissim[lf]);
		if (dissim[lf] > DISSIMILARITY_THRESHOLD) {
			transition[lf] = true;
			logger(INFO, "Transition at %s.", prettify_seconds(lf, 0));
		} else {
			transition[lf] = false;
		}
	}

	free(dissim_norm);

	segment *unmerged_segments = malloc(LONG_FRAME_COUNT*sizeof(segment));

	// Merge second long frames to create unmerged_segments
	int current_segment = 0;
	unmerged_segments[0].startframe = 0;
	unmerged_segments[0].endframe = 0;
	unmerged_segments[0].is_music = false;
	for (int lf = 1; lf < LONG_FRAME_COUNT; lf++) {
		if (transition[lf]) {
			current_segment++;
			unmerged_segments[current_segment].startframe = lf;
			unmerged_segments[current_segment].endframe = lf;
			unmerged_segments[current_segment].is_music = false;
		} else {
			unmerged_segments[current_segment].endframe++;
		}
	}

	int unmerged_seg_count = current_segment+1;

	// Go through all the unmerged_segments and check whether they are speech or music
	if (has_intro) {
		unmerged_segments[0].is_music = false;
	} else {
		unmerged_segments[0].is_music = true;
	}
	double mler_sum;
	double normalized_mler;
	for (int seg = 1; seg < unmerged_seg_count; seg++) {
		mler_sum = 0;
		for (int f = unmerged_segments[seg].startframe; f <= unmerged_segments[seg].endframe; f++) {
			mler_sum += mler[f];
		}
		normalized_mler = mler_sum/(unmerged_segments[seg].endframe-unmerged_segments[seg].startframe+1);
		//printf("%f\n", normalized_mler);
		if (normalized_mler <= UPPER_MUSIC_THRESHOLD) {
			unmerged_segments[seg].is_music = true;
		} else {
			unmerged_segments[seg].is_music = false;
		}
	}

	// Merge small segments into larger segments, and segments
	// of the same type (speech/music) into each other
	current_segment = 0;
	segments[0].startframe = 0;
	segments[0].endframe = unmerged_segments[0].endframe;
	segments[0].is_music = unmerged_segments[0].is_music;
	for (int seg = 1; seg < unmerged_seg_count; seg++) {
		logger(INFO, "%s-%s: %d", prettify_seconds(unmerged_segments[seg].startframe, 0), prettify_seconds(unmerged_segments[seg].endframe, 0), unmerged_segments[seg].is_music);
		if (unmerged_segments[seg].endframe - unmerged_segments[seg].startframe < 4 && !segments[current_segment].is_music) {
			logger(INFO, "Too short -- merging");
			segments[current_segment].endframe = unmerged_segments[seg].endframe;
		} else if (unmerged_segments[seg].is_music == segments[current_segment].is_music) {
			logger(INFO, "Same type -- merging");
			segments[current_segment].endframe = unmerged_segments[seg].endframe;
		} else {
			logger(INFO, "Not merging");
			current_segment++;
			segments[current_segment].startframe = unmerged_segments[seg].startframe;
			segments[current_segment].endframe = unmerged_segments[seg].endframe;
			segments[current_segment].is_music = unmerged_segments[seg].is_music;
		}
		logger(INFO, "");
	}

	int seg_count = current_segment + 1;

	// Grow speech segments
	const int GROW_BY_BEFORE = 8; // seconds
	const int GROW_BY_AFTER = 3; // seconds
	if (has_intro) {
		segments[0].endframe += GROW_BY_AFTER;
	} else {
		segments[0].endframe -= GROW_BY_BEFORE;
	}
	for (int seg = 1; seg < seg_count; seg++) {
		if (segments[seg].is_music) {
			segments[seg].startframe += GROW_BY_BEFORE;
			segments[seg].endframe -= GROW_BY_AFTER;
		} else {
			segments[seg].startframe -= GROW_BY_BEFORE;
			segments[seg].endframe += GROW_BY_AFTER;
		}
	}

	// Finally, print all the segments
	for (int seg = 0; seg < seg_count; seg++) {
		logger(NOTICE, "%s-%s: %d", prettify_seconds(segments[seg].startframe, 0), prettify_seconds(segments[seg].endframe, 0), segments[seg].is_music);
	}
}
