podcastgen
=============

podcastgen is a podcast generator written for [Radio Revolt](http://dusken.no/radio).

It takes a WAV file as input, and outputs a 16 bit PCM WAV file in which most of the
music has been removed.

Usage
-------------
```
Syntax: podcastgen <options> <input file>

    -v                      Verbose output
    -h, --help              Show this help text
    --ignore-start-music    Do not remove music at start of file
```

Installation
-------------

```
cd build
cmake ..
make
make install
```
