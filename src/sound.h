// sound.h
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

#ifndef SOUND_H
#define SOUND_H

#include "main.h"

typedef struct segment {
	int startframe;
	int endframe;
	bool is_music;
} segment;

extern const int RMS_FRAME_DURATION; // Length of the RMS calculation frames in milliseconds
extern const int LONG_FRAME_DURATION; // Length of the long (averaging) frames in milliseconds
extern int FRAMES_IN_RMS_FRAME;
extern int FRAMES_IN_LONG_FRAME;
extern int RMS_FRAME_COUNT;
extern int LONG_FRAME_COUNT;
extern int RMS_FRAMES_IN_LONG_FRAME;

extern double LOW_ENERGY_COEFFICIENT;
extern double UPPER_MUSIC_THRESHOLD;

double *calculate_rms(double *rms);
double *calculate_features(double *rms, double *mean_rms, double *variance_rms, double *norm_variance_rms, double *mler);
void classify_segments(bool *is_music, double *mler);
int classify_segments2(segment *segments, double *mean_rms, double *variance_rms, double *norm_variance_rms, double *mler);
void average_musicness(bool *is_music);
int merge_segments(bool *is_music, segment *merged_segments);

#endif
