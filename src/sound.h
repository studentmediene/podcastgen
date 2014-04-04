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

extern float LOW_ENERGY_COEFFICIENT;
extern float UPPER_MUSIC_THRESHOLD;

float *calculate_rms(float *rms);
float *calculate_features(float *rms, float *mean_rms, float *variance_rms, float *norm_variance_rms, float *mler);
void classify_segments(bool *is_music, float *mler);
void classify_segments2(bool* transition, float *mean_rms, float *variance_rms, float *norm_variance_rms, float *mler);
void average_musicness(bool *is_music);
int merge_segments(bool *is_music, segment *merged_segments);

#endif
