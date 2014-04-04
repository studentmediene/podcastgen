// files.c
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
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "sound.h"
#include "util.h"
#include "main.h"

SNDFILE *source_file;
SNDFILE *dest_file;
SF_INFO source_info;
SF_INFO dest_info;

char *input_path;
char *output_path;
char *file_folders;
char *filename;

void open_source_file() {
	if (!(source_file = sf_open(input_path, SFM_READ, &source_info))) {
		logger(ERROR, "An error occured while opening the source file: %s", sf_strerror(NULL));
		exit(1);
	}
}

void open_dest_file() {
	output_path = malloc(200);

	strcpy(output_path, file_folders);
	strcat(output_path, "podcast_");
	strcat(output_path, filename);

	if (!(dest_file = sf_open(output_path, SFM_WRITE, &dest_info))) {
		logger(ERROR, "An error occured while opening the destination file: %s", sf_strerror(NULL));
		exit(1);
	}
}

void write_speech_to_file(segment *merged_segments, int merged_segment_count) {
	float *cache = malloc(1);
	int frames_to_read;
	int voice_segment_number = 1;

	sf_count_t current_read_location;
	sf_count_t current_write_location;

	for (int s = 0; s < merged_segment_count; s++) {
		if (!merged_segments[s].is_music) {
			if (s == 0) {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+5)*FRAMES_IN_LONG_FRAME;
			} else {
				current_read_location = sf_seek(source_file, (merged_segments[s].startframe-5)*FRAMES_IN_LONG_FRAME, SEEK_SET);
				frames_to_read = (merged_segments[s].endframe-merged_segments[s].startframe+10)*FRAMES_IN_LONG_FRAME;
			}
			logger(NOTICE, "%d: %s", voice_segment_number, prettify_seconds((int) current_read_location/(float) FRAMES_IN_LONG_FRAME, (int) frames_to_read/(float) FRAMES_IN_LONG_FRAME));

			cache = realloc(cache, frames_to_read*2*sizeof(float));
			current_read_location = sf_readf_float(source_file, cache, frames_to_read);
			current_write_location = sf_writef_float(dest_file, cache, frames_to_read);

			sf_write_sync(dest_file);
			voice_segment_number++;
		}
	}
}

bool finalize_files() {
	int source_file_status = sf_close(source_file);
	sf_write_sync(dest_file);
	int dest_file_status = sf_close(dest_file);

	return source_file_status && dest_file_status;
}
