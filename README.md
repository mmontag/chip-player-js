# libADLMIDI
libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation

Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>

ADLMIDI Library API:   Copyright (c) 2015 Vitaly Novichkov <admin@wohlnet.ru>

Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:

[http://iki.fi/bisqwit/source/adlmidi.html](http://iki.fi/bisqwit/source/adlmidi.html)

# Differences with original tool
* Reverb code has been removed.
* Doesn't contains platform specific code. Library can be used with various purposes include making of a custom music decoders for a media players and usage with a games

# Key features
* OPL3 emulation with four-operator mode support
* FM patches from a number of known PC games, copied from files typical to AIL = Miles Sound System / DMX / HMI = Human Machine Interfaces / Creative IBK.
* Stereo sound
* Number of simulated soundcards can be specified as 1-100 (maximum channels 1800!)
* Pan (binary panning, i.e. left/right side on/off)
* Pitch-bender with adjustable range
* Vibrato that responds to RPN/NRPN parameters
* Sustain enable/disable
* MIDI and RMI file support
* loopStart / loopEnd tag support (Final Fantasy VII)
* Use automatic arpeggio with chords to relieve channel pressure
* Support for multiple concurrent MIDI synthesizers (per-track device/port select FF 09 message), can be used to overcome 16 channel limit

# How to build
You can build shared version and additional tools on the Linux when you will run a "make" command and you will have libadlmidi.so and additional tools in the "bin" directory.

You also can build library manually:
You need to make in the any IDE a library project and put into it next files
(or include those files into subfolder of your exist project instead if you want to use it statically):

* adlmidi.h    - Library API, use it to communicate with library

* dbopl.h		 - DOSBOX OPL Emulation header
* fraction.h	 - Fraction number handling
* adldata.hh   - bank structures definition

* dbopl.cpp    - DOSBOX OPL Emulation code
* adlmidi.cpp  - code of library
*adldata.cpp	 - Automatically generated dump of FM banks from "fm_banks" directory
               via "gen_adldata" tool

# Example
In the src/midiplay you will found alone CPP file which an example of library usage.
That example is a simple audio player based on SDL Audio usage.

To build that example you will need to have installed SDL2 library.

# Todo
* Fix infinite loop causes swapped loopEnd and loopStart (when loopEnd goes before loopStart)
* Time based Seek/Tell support
* Support of title and other meta-tags retrieving
* Support of real-time listening of incoming MIDI-commands.
  That will allow to use library as software MIDI Output device
  to play any MIDI via this library.

# Changelog	
## 1.0.0	2015-10-10
 * First release of library


