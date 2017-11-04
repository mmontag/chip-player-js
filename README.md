# libADLMIDI
libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation

Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>

ADLMIDI Library API:   Copyright (c) 2015-2017 Vitaly Novichkov <admin@wohlnet.ru>

Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:

[http://iki.fi/bisqwit/source/adlmidi.html](http://iki.fi/bisqwit/source/adlmidi.html)

# Differences with original tool
* Reverb code has been removed.
* Doesn't contains platform specific code. Library can be used with various purposes include making of a custom music decoders for a media players and usage with a games.
* Supports custom non-hardcoded WOPL banks and ability to build without of embedded banks

# Tested on platforms
* Linux GCC 4.8, 4.9, 5.4 / CLang
* Mac OS X CLang (Xcode 7.x)
* Windows MinGW 4.9.x, 5.2
* Android NDK 12b/13

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
* 111-th controller based loop start (RPG-Maker)
* Use automatic arpeggio with chords to relieve channel pressure
* Support for multiple concurrent MIDI synthesizers (per-track device/port select FF 09 message), can be used to overcome 16 channel limit
* Support for playing Id-software Music File format (IMF)
* Support for custom banks of [WOPL format](https://github.com/Wohlstand/OPL3BankEditor/blob/master/Specifications/WOPL-and-OPLI-Specification.txt)

# How to build
To build libADLMIDI you need to use CMake:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```
## Available CMake options
* **CMAKE_PREFIX_PATH** - destinition folder where libADLMIDI will be installed. On Linux it is /usr/local/ by default.
* **CMAKE_BUILD_TYPE** - Build types: **Debug** or **Release**
* **WITH_MIDIPLAY** - (ON/OFF, default OFF) Build demo MIDI player (Requires SDL2 and also pthread on Windows with MinGW)
* **WITH_OLD_UTILS** - (ON/OFF, default OFF) Build old utilities to dump some bank formats, made by original creator of ADLMIDI
* **WITH_EMBEDDED_BANKS** - (ON/OFF, default ON) Enable or disable embedded banks (Original ADLMIDI and older versions of libADLMIDI are had embedded-only banks with no ability to load custom banks in runtime).
* **WITH_GENADLDATA**  - (ON/OFF, default OFF) Build and execute the utility which will rebuild the embedded banks database (which is an adldata.cpp file).
* **USE_DOSBOX_EMULATOR** - (ON/OFF, default OFF) Use DosBox emulator instead of current Nuked OPL3. This emulator has less accuracy, but it is well optimized for a work on slow devices such as older computers, embedded devices, or mobile device.

* **libADLMIDI_STATIC** - (ON/OFF, default ON) Build static library
* **libADLMIDI_SHARED** - (ON/OFF, default OFF) Build shared library


## You also can build library manually:
You need to make in the any IDE a library project and put into it next files
(or include those files into subfolder of your exist project instead if you want to use it statically):

### Public header (include)
* adlmidi.h     - Library Public API header, use it to control library

### Internal code (src)
* adldata.hh    - bank structures definition
* adlmidi_mus2mid.h - MUS2MID converter header
* adlmidi_private.hpp - header of internal private APIs
* adlmidi_xmi2mid.h - XMI2MID converter header
* dbopl.h       - DOSBOX OPL Emulation header
* fraction.h    - Fraction number handling
* nukedopl3.h   - Nuked OPL3 Emulation header

* adldata.cpp	 - Automatically generated database of FM banks from "fm_banks" directory via "gen_adldata" tool. **Don't build it if you have defined `DISABLE_EMBEDDED_BANKS` macro!**
* adlmidi.cpp   - code of library
* adlmidi_load.cpp	- Source of file loading and parsing processing
* adlmidi_midiplay.cpp	- MIDI event sequencer
* adlmidi_mus2mid.c	- MUS2MID converter source
* adlmidi_opl3.cpp	- OPL3 chips manager
* adlmidi_private.cpp	- some internal functions sources
* adlmidi_xmi2mid.c	- XMI2MID converter source
* dbopl.cpp     - DOSBOX OPL Emulation code (used when `ADLMIDI_USE_DOSBOX_OPL` macro is defined)
* nukedopl3.c   - Nuked OPL3 Emulation code (used by default, disabled when `ADLMIDI_USE_DOSBOX_OPL` macro is defined)

**Important**: Please use DosBox emulator on mobile devices because it requires small CPU power. Nuked OPL synthesizer is very accurate (compared to real OPL3 chip), but it requires much more power device and is high probability your device will lag and playback will be choppy.

**Tip 1**: If you want to work with custom WOPL banks without using of embedded banks, you can create them by using [OPL3 Bank Editor](https://github.com/Wohlstand/OPL3BankEditor) where also included some WOPL examples, or you are able to save any other bank as WOPL.

**Tip 2**: To compile libADLMIDI without embedded banks, define the `DISABLE_EMBEDDED_BANKS` macro and remove building of the `adldata.cpp` file in your project.

# Example
In the utils/midiplay you will found an example project which uses this library.
That example is a simple audio player based on SDL Audio usage.

To build that example you will need to have installed SDL2 library.

# Working demos

* [PGE MusPlay for Win32](http://wohlsoft.ru/docs/_laboratory/_Builds/win32/bin-w32/_packed/pge-musplay-dev-win32.zip) (also available for other platforms as part of [PGE Project](https://github.com/WohlSoft/PGE-Project)) - a little music player which uses SDL Mixer X library (fork of the SDL Mixer 2.0) which has embedded libADLMIDI to play MIDI files independently from operating system's settings and drivers. <br>(source code of player can be find [here](https://github.com/WohlSoft/PGE-Project/tree/master/MusicPlayer) and source code of SDL Mixer X [here](https://github.com/WohlSoft/PGE-Project/tree/master/_Libs/SDL2_mixer_modified))
* [ADLMIDI Player for Android](https://github.com/Wohlstand/ADLMIDI-Player-Java/releases) - a little MIDI-player for Android which uses libADLMIDI to play MIDI files and provides flexible GUI with ability to change bank, flags, number of emulated chips, etc.

# Todo
* Reimplement original ADLMIDI with using of new hooks of libADLMIDI
* Implement WOPL Version 3 which will contain pre-calculated `ms_sound_kon` and `ms_sound_koff` values per every instrument.
* Implement multi-bank to support GS or XG standards.
* Add support of MIDI Format 2 files (FL Studio made MIDI-files are wired and opening of those files making lossy of tempo and some meta-information events)
* Support of real-time listening of incoming MIDI-commands.
  That will allow to use library as software MIDI Output device
  to play any MIDI via this library.

# Changelog
## 1.3.0   2017-10-17 -WIP-
 * "gen_adldata" tool now supports WOPL banks format which supports a full set of libADLMIDI features
 * Added support for custom banks are loadable in runtime without rebuilding of "adldata.cpp" banks database
 * Smooth finalizing of song when loop is disabled (old ugly hack has been removed :wink:)
 * Added an ability to reset song position to begin (very helpful when song reaches the end)
 * Avoided possible crashes on attempt to fetch sample data without opening of MIDI file
 * Demo tool now can correctly record WAVs and now can correctly deal with CTRL+C termination
 * When loop is disabled, loop points will be ignored
 * Loop now is disabled by default
 * Reworked internal storage of MIDI events to easier pre-process them and retrieve any useful information before play them
 * Added ability to get seconds time position and song length
 * Added seekability support
 * Fixed IMF playing when passing file as path nor as memory block
 * Added ability to get time position of every loop point if presented
 * Added ability to change playing tempo by giving multiplier (how faster or slower than original)
 * Added support for meta-tags getting: title, copyright, track titles, and markers with time and ticks positions
 * Added hooks to increase advandate of The Library: MIDI-event, Note, and Debug-Message hooks!
 * Fixed the ability to merge two equal pseudo-4-operator voices as one voice without damaging the result!
 * Added auto-increasing of percussion note lengths when there are too short and playing an incorrect sound on various banks
 * ...

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


