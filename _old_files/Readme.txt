==================================================================================
    libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
==================================================================================

Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
ADLMIDI Library API:   Copyright (c) 2015-2017 Vitaly Novichkov <admin@wohlnet.ru>

Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
=========================================
http://iki.fi/bisqwit/source/adlmidi.html
=========================================

=========================================
      Differences with original tool
=========================================
* Reverb code has been removed.
* Doesn't contains platform specific code.
 Library can be used with various purposes include making of a custom music decoders
 for a media players and usage with a games

=========================================
              Key features
=========================================

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


=========================================
              How to build
=========================================
You can build shared version and additional tools on the Linux when you will run a "make" command and you will have libadlmidi.so and additional tools in the "bin" directory.

You also can build library manually:
You need to make in the any IDE a library project and put into it next files
(or include those files into subfolder of your exist project instead if you want to use it statically):

* adlmidi.h             - Library API, use it to control library

* dbopl.h               - DOSBOX OPL Emulation header
* nukedopl3.h           - Nuked OPL3 Emulation header
* fraction.h            - Fraction number handling
* adldata.hh            - bank structures definition
* adlmidi_private.hpp   - header of internal private APIs
* adlmidi_mus2mid.h     - MUS2MID converter header
* adlmidi_xmi2mid.h     - XMI2MID converter header

* dbopl.cpp             - DOSBOX OPL Emulation code (used when ADLMIDI_USE_DOSBOX_OPL macro is defined)
* nukedopl3.c           - Nuked OPL3 Emulation code (used by default)
* adlmidi.cpp           - code of library
* adldata.cpp           - Automatically generated dump of FM banks from "fm_banks" directory via "gen_adldata" tool
* adlmidi_load.cpp      - Source of file loading and parsing processing
* adlmidi_midiplay.cpp	- MIDI event sequencer
* adlmidi_opl3.cpp      - OPL3 chips manager
* adlmidi_private.cpp   - some internal functions sources
* adlmidi_mus2mid.c	    - MUS2MID converter source
* adlmidi_xmi2mid.c	    - XMI2MID converter source

**Important**: Please use DosBox emulator on mobile devices because it requires small CPU power.
 Nuked OPL synthesizer is very accurate (compared to real OPL3 chip), but it requires much more power
 device and is high probability your device will lag and playback will be choppy.

=========================================
              Example
=========================================
In the src/midiplay you will found alone CPP file which an example of library usage.
That example is a simple audio player based on SDL Audio usage.

To build that example you will need to have installed SDL2 library.

=========================================
              Todo
=========================================
* Time based Seek/Tell support
* Support of title and other meta-tags retrieving
* Support of real-time listening of incoming MIDI-commands.
  That will allow to use library as software MIDI Output device
  to play any MIDI via this library.

=========================================
Changelog
=========================================
## 1.2.1    2017-07-30
 * Minor fixes
 * Added 72'th bank by Sneakernets
 * Updated "gen_adldata" utility to use ini file

## 1.2.0    2017-02-15
 * Fixed 12'th bank where are drums and melodic sets are was swapped
 * Fxied logariphmic volumes flag which autoreseted to false on playing begin moment
 * Added TRUE support for MUS and XMI formats (added the conversion functions to make right MIDI data to play it)

## 1.1.1	2016-12-09
 * Added a changable volume ranges models (Automatic for some banks, Generic, CMF, DMX, Apogee and 9X)

## 1.1.0	2016-12-06
 * Added Nuked OPL3 emulator which is more accurate (but requires more CPU power, therefore kept ability to use DosBox OPL3 via macro)
 * Fixed warnings of CLang code model plugin
 * Fixed bend coefficient which makes hi-hats in DOOM banks be incorrectly

## 1.0.3	2016-08-06
 * Added handling of 111'th controller as "loopStart" (which used by RPG-Maker)
 * Fixed infinite loop caused by blank MIDI-files (add extra second of waiting if over 10000 0-waiting loops are been detected)
 * Fixed damaged playing of IMF files (cased by wrong implementation of getc() function where return type must be int, not unsigned char)

## 1.0.2	2016-03-16
 * Fixed infinite loop causes swapped loopEnd and loopStart (when loopEnd goes before loopStart)
 * Fixed sielent volume tracks (when initial voule is zero, tracks wouldn't be playd even after applying fading in volume events)
 * More accurate fine tuning for second voice for DMX-based banks (playback now is more like original DOOM than before!)
 * Library's Output data now in short format (no more need to do extra conversions to make 16-bit audio stream from every 16-bit
   sample stored into each 32-bit integer)

## 1.0.0	2015-10-10
 * First release of library

