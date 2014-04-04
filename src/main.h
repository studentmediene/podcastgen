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

#ifndef MAIN_H
#define MAIN_H

#include <sndfile.h>

typedef enum { false, true } bool;

extern bool verbose;
extern bool very_verbose;
extern bool has_intro;

void open_source_file();
void open_dest_file();
char *interpret_args(int argc, char *argv[]);

#endif
