// main.h
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

typedef enum { false, true } bool;

extern SNDFILE *source_file;
extern SNDFILE *dest_file;
extern SF_INFO source_info;
extern SF_INFO dest_info;

extern const int RMS_FRAME_DURATION; // Length of the RMS calculation frames in milliseconds
extern const int LONG_FRAME_DURATION; // Length of the long (averaging) frames in milliseconds
extern int FRAMES_IN_RMS_FRAME;
extern int FRAMES_IN_LONG_FRAME;
extern int RMS_FRAME_COUNT;
extern int LONG_FRAME_COUNT;
extern int RMS_FRAMES_IN_LONG_FRAME;

extern float LOW_ENERGY_COEFFICIENT; // see http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=1292679
extern float UPPER_MUSIC_THRESHOLD; // MLER below this value => 1 second frame classified as music

extern char *input_path;
extern char *output_path;
extern char *file_folders;
extern char *filename;

extern bool verbose;
extern bool very_verbose;
extern bool has_intro;

int signum(float n);
void split_path(char *full_path, char **folders, char **file);
char *prettify_seconds(int start, int delta);

void open_source_file();
void open_dest_file();
char *interpret_args(int argc, char *argv[]);

float *calculate_rms(float *rms);
float *calculate_features(float *rms, float *mean_rms, float *variance_rms, float *norm_variance_rms, float *mler);
