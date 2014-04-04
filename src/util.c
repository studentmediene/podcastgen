// util.c
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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "util.h"
#include "main.h"

void _logger(severity_t severity, const char *message, ...) {
	// TODO: Actually log to file
	va_list arg;

	if (severity == ERROR) {
		va_start(arg, message);
		vfprintf(stderr, message, arg);
		va_end(arg);
	} else if (very_verbose) {
		va_start(arg, message);
		vfprintf(stdout, message, arg);
		va_end(arg);
	} else if (verbose) {
		if (severity == NOTICE || severity == WARNING) {
			va_start(arg, message);
			vfprintf(stdout, message, arg);
			va_end(arg);
		}
	} else {
		if (severity == WARNING) {
			va_start(arg, message);
			vfprintf(stdout, message, arg);
			va_end(arg);
		}
	}
}

int signum(float n) {
	return (n > 0) - (n < 0);
}

void split_path(char *full_path, char **folders, char **file) {
	char *slash = full_path;
	char *next;
	while (next = strpbrk(slash + 1, "\\/")) {
		slash = next;
	}
	if (full_path != slash) {
		slash++;
	}
	*folders = strndup(full_path, slash - full_path);
	*file = strdup(slash);
}

char *prettify_seconds(int start, int delta) {
	char *interval_string = malloc(50);

	int s_min = start/60;
	int s_sec = start - s_min*60;

	if (0 != delta) {
		int e_min = (start + delta)/60;
		int e_sec = (start + delta) - e_min*60;

		sprintf(interval_string, "%02d:%02d to %02d:%02d", s_min, s_sec, e_min, e_sec);
	} else {
		sprintf(interval_string, "%02d:%02d", s_min, s_sec);
	}

	return interval_string;
}
