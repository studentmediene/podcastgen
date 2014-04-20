// main.c
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

#include "main.h"
#include "util.h"
#include "sound.h"
#include "files.h"

bool verbose = false;
bool very_verbose = false;
bool has_intro = true;

char *interpret_args(int argc, char *argv[]) {
	const char *HELP_TEXT =
		"Syntax: podcastgen <options> <input file>\n"
		"\n"
		"	-h, --help\t\tDisplay this help text\n"
		"	-v\t\t\tVerbose output\n"
		"	-C\t\t\tSet the Low Energy Coefficient\n"
		"	-T\t\t\tSet the Upper Music Threshold\n"
		"	--very-verbose\t\tVERY verbose output\n"
		"	--no-intro\t\tRemove music at start of file\n\n";

	static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"very-verbose", no_argument, (int*) &very_verbose, true},
		{"no-intro", no_argument, (int*) &has_intro, false}
	};

	int opt;
	while ((opt = getopt_long(argc, argv, "vhC:T:", long_options, NULL)) != -1) {
		switch (opt) {
			case 'v':
				verbose = true;
				break;
			case 'h':
				printf(HELP_TEXT);
				exit(0);
			case 'C':
				LOW_ENERGY_COEFFICIENT = atof(optarg);
				break;
			case 'T':
				UPPER_MUSIC_THRESHOLD = atof(optarg);
				break;
			case '?':
				logger(ERROR, "Unknown argument \'-%c\'.", optopt);
				exit(1);
		}
	}

	if (very_verbose) {
		verbose = true;
	}

	if (argc-optind == 1) {
		input_path = malloc(100);
		file_folders = malloc(100);
		filename = malloc(100);

		strcpy(input_path, argv[optind]);
		split_path(input_path, &file_folders, &filename);
	} else {
		logger(ERROR, "Please supply a source file.");
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	interpret_args(argc, argv);

	// Open files and set info
	open_source_file(filename);

	dest_info.samplerate = 44100;
	dest_info.channels = 2;
	dest_info.format = SF_FORMAT_WAV ^ SF_FORMAT_PCM_16 ^ SF_ENDIAN_FILE;

	open_dest_file(dest_file, &dest_info);

	if (verbose) {
		logger(NOTICE, "Sample rate: %d", source_info.samplerate);
		logger(NOTICE, "Frames: %d", (int) source_info.frames);
		logger(NOTICE, "Channels: %d", source_info.channels);
		logger(NOTICE, "%s minutes long.", prettify_seconds(source_info.frames/(double) source_info.samplerate, 0));
	}

	// Calculate Root-Mean-Square of source sound
	FRAMES_IN_RMS_FRAME = (source_info.samplerate*RMS_FRAME_DURATION)/1000; // (44100/1000)*20
	RMS_FRAME_COUNT = ceil(source_info.frames/(double) FRAMES_IN_RMS_FRAME);

	double *rms = malloc(2*RMS_FRAME_COUNT*sizeof(double));
	calculate_rms(rms);

	if (verbose) {
		logger(INFO, "Calculated RMS!");
	}

	// Calculate RMS-derived features of long (1 second) frames
	FRAMES_IN_LONG_FRAME = (source_info.samplerate*LONG_FRAME_DURATION)/1000; // (44100/1000)*20
	LONG_FRAME_COUNT = ceil(source_info.frames/(double) FRAMES_IN_LONG_FRAME);
	RMS_FRAMES_IN_LONG_FRAME = LONG_FRAME_DURATION/RMS_FRAME_DURATION;

	double *mean_rms = malloc(2*LONG_FRAME_COUNT*sizeof(double));
	double *variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(double));
	double *norm_variance_rms = malloc(2*LONG_FRAME_COUNT*sizeof(double));
	double *mler = malloc(2*LONG_FRAME_COUNT*sizeof(double));

	calculate_features(rms, mean_rms, variance_rms, norm_variance_rms, mler);

	logger(INFO, "Calculated features!");

	// CLASSIFY
	bool *is_music = malloc(2*LONG_FRAME_COUNT*sizeof(bool));

	// Decide whether a given second long segment is music or speech,
	// based on the MLER value and the Upper Music Threshold
	segment *segments = malloc(LONG_FRAME_COUNT*sizeof(segment));
	int segment_count = classify_segments2(segments, mean_rms, variance_rms, norm_variance_rms, mler);

	// Finally, we write only the speech sections to the destination file
	write_speech_to_file(segments, segment_count);

	// Close files and free memory
	bool file_close_success = finalize_files();
	logger(WARNING, "Successfully generated %s!", output_path);

	free(mean_rms);
	free(variance_rms);
	free(norm_variance_rms);
	free(mler);

	free(segments);
	free(is_music);
	free(rms);

	return !file_close_success;
}
