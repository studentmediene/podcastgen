// files.h
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

#ifndef FILES_H
#define FILES_H

#include <sndfile.h>

#include "main.h"
#include "sound.h"

extern SNDFILE *source_file;
extern SNDFILE *dest_file;
extern SF_INFO source_info;
extern SF_INFO dest_info;

extern char *input_path;
extern char *output_path;
extern char *file_folders;
extern char *filename;

void open_source_file();
void open_dest_file();
void write_speech_to_file(segment *merged_segments, int merged_segment_count);
bool finalize_files();

#endif
