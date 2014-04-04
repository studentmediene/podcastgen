// util.h
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

#include "main.h"

typedef enum {
	INFO, NOTICE, WARNING, ERROR
} severity_t;

#define logger(severity, message, ...) _logger(severity, message"\n", ##__VA_ARGS__)

void _logger(severity_t severity, const char *message, ...)
	    __attribute__((format (printf, 2, 3)));
int signum(float n);
void split_path(char *full_path, char **folders, char **file);
char *prettify_seconds(int start, int delta);
double manymax(double args, ...);
