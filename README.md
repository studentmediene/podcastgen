podcastgen
=============

podcastgen is a podcast generator written for [Radio Revolt](http://dusken.no/radio).

It takes a WAV file as input, and outputs a 16 bit PCM WAV file in which the music has
been removed (apart from a few seconds at the beginning and end of each speech section).

The method used is based on the Modified Low Energy Ratio, described in "A Fast and
Robust Speech/Music Discrimination Approach" by Wang, Gao and Ying (2003). The goal
is to implement the full method described in "Automatic speech/music discrimination
in audio files" by Ericsson (2009), see http://www.speech.kth.se/prod/publications/files/3437.pdf.

Usage
-------------
```
Syntax: podcastgen <options> <input file>

	-h, --help				Display this help text
	-v						Verbose output
	-C <float>				Set the Low Energy Coefficient
	-T <float>				Set the Upper Music Threshold
	--very-verbose			Very(!) verbose output
	--no-intro				Remove music at start of file
```

Installation
-------------

```
cd build
cmake ..
make
make install
```
