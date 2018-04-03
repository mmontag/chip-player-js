# libADLMIDI
libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation

Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>

ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>

Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:

[http://iki.fi/bisqwit/source/adlmidi.html](http://iki.fi/bisqwit/source/adlmidi.html)

[![Build Status](https://semaphoreci.com/api/v1/wohlstand/libadlmidi/branches/master/badge.svg)](https://semaphoreci.com/wohlstand/libadlmidi)

# Differences with original tool
* Reverb code has been removed.
* Doesn't contains platform specific code. Library can be used with various purposes include making of a custom music decoders for a media players and usage with a games.
* Supports custom non-hardcoded WOPL banks and ability to build without of embedded banks

# Tested on platforms
* Linux GCC 4.8, 4.9, 5.4 / CLang
* Mac OS X CLang (Xcode 7.x)
* Windows MinGW 4.9.x, 5.2
* Android NDK 12b/13
* DJGPP GCC 7.2 cross compiler from Linux to MS-DOS 32-bit

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
* Partial support for XG standard (having more instruments than in one 128:128 GM set and ability to use multiple channels for percussion purposes)

# How to build
To build libADLMIDI you need to use CMake:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```
### Notes
* To compile for DOS via DJGPP on Linux, use `cmake/djgpp/djgpp-cmake.sh` script which a wrapper over CMake to pass DJGPP's stuff required for build


## Available CMake options
* **CMAKE_PREFIX_PATH** - destinition folder where libADLMIDI will be installed. On Linux it is /usr/local/ by default.
* **CMAKE_BUILD_TYPE** - Build types: **Debug** or **Release**
* **WITH_MIDIPLAY** - (ON/OFF, default OFF) Build demo MIDI player (Requires SDL2 and also pthread on Windows with MinGW)
* **WITH_ADLMIDI2** - (ON/OFF, default OFF) Build Classic ADLMIDI MIDI player (Requires SDL2 on Linux and macOS, requires pthread on Windows with MinGW, SDL doesn't required on Windows. Also, the **WITH_CPP_EXTRAS** flag must be enabled)
* **WITH_CPP_EXTRAS** - (ON/OFF, default OFF) Build libADLMIDI with some extra public features for C++ language (for example, instrument testing API is available for C++ only).
* **WITH_OLD_UTILS** - (ON/OFF, default OFF) Build old utilities to dump some bank formats, made by original creator of ADLMIDI
* **WITH_EMBEDDED_BANKS** - (ON/OFF, default ON) Enable or disable embedded banks (Original ADLMIDI and older versions of libADLMIDI are had embedded-only banks with no ability to load custom banks in runtime).
* **WITH_GENADLDATA**  - (ON/OFF, default OFF) Build and execute the utility which will rebuild the embedded banks database (which is an adldata.cpp file).
* **USE_DOSBOX_EMULATOR** - (ON/OFF, default OFF) Use DosBox emulator instead of current Nuked OPL3. This emulator has less accuracy, but it is well optimized for a work on slow devices such as older computers, embedded devices, or mobile device.

* **libADLMIDI_STATIC** - (ON/OFF, default ON) Build static library
* **libADLMIDI_SHARED** - (ON/OFF, default OFF) Build shared library


## You also can build library manually:
You need to make in the any IDE a library project and put into it next files
(or include those files into subfolder of your exist project instead if you want to use it statically):

### Useful macros
* `ADLMIDI_DISABLE_XMI_SUPPORT` - Disables XMI to MIDI converter
* `ADLMIDI_DISABLE_MUS_SUPPORT` - Disables MUS to MIDI converter
* `ADLMIDI_DISABLE_MIDI_SEQUENCER` - Completely disables built-in MIDI sequencer.
* `ADLMIDI_USE_DOSBOX_OPL` - Enables DosBox 0.74 OPL3 emulator to be used. Nuked OPL3 is used by default if macro is not defined.
* `DISABLE_EMBEDDED_BANKS` - Disables usage of embedded banks. Use it to use custom-only banks.

### Public header (include)
* adlmidi.h     - Library Public API header, use it to control library
* adlmidi.hpp   - Public additional C++ API header, optional

### Internal code (src)
* adldata.hh    - bank structures definition
* adlmidi_private.hpp - header of internal private APIs
* dbopl.h       - DOSBOX OPL Emulation header
* fraction.hpp  - Fraction number handling
* nukedopl3.h   - Nuked OPL3 Emulation header

* adldata.cpp	 - Automatically generated database of FM banks from "fm_banks" directory via "gen_adldata" tool. **Don't build it if you have defined `DISABLE_EMBEDDED_BANKS` macro!**
* adlmidi.cpp   - code of library
* adlmidi_load.cpp	- Source of file loading and parsing processing
* adlmidi_midiplay.cpp	- MIDI event sequencer
* adlmidi_opl3.cpp	- OPL3 chips manager
* adlmidi_private.cpp	- some internal functions sources
* dbopl.cpp     - DOSBOX OPL Emulation code (used when `ADLMIDI_USE_DOSBOX_OPL` macro is defined)
* nukedopl3.c   - Nuked OPL3 Emulation code (used by default, disabled when `ADLMIDI_USE_DOSBOX_OPL` macro is defined)

#### MUS2MIDI converter
To remove MUS support, define `ADLMIDI_DISABLE_MUS_SUPPORT` macro and remove those files:
* adlmidi_mus2mid.h - MUS2MID converter header
* adlmidi_mus2mid.c	- MUS2MID converter source

#### XMI2MIDI converter
To remove XMI support, define `ADLMIDI_DISABLE_XMI_SUPPORT` macro and remove those files:
* adlmidi_xmi2mid.h - XMI2MID converter header
* adlmidi_xmi2mid.c	- XMI2MID converter source


**Important**: Please use DosBox emulator on mobile devices because it requires small CPU power. Nuked OPL synthesizer is very accurate (compared to real OPL3 chip), but it requires much more power device and is high probability your device will lag and playback will be choppy.

**Tip 1**: If you want to work with custom WOPL banks without using of embedded banks, you can create them by using [OPL3 Bank Editor](https://github.com/Wohlstand/OPL3BankEditor) where also included some WOPL examples, or you are able to save any other bank as WOPL.

**Tip 2**: To compile libADLMIDI without embedded banks, define the `DISABLE_EMBEDDED_BANKS` macro and remove building of the `adldata.cpp` file in your project.

# Example
In the utils/midiplay you will found an example project which uses this library.
That example is a simple audio player based on SDL Audio usage.

To build that example you will need to have installed SDL2 library.

# Working demos

* [PGE MusPlay for Win32](http://wohlsoft.ru/docs/_laboratory/_Builds/win32/bin-w32/_packed/pge-musplay-dev-win32.zip) and [Win64](http://wohlsoft.ru/docs/_laboratory/_Builds/win32/bin-w64/_packed/pge-musplay-dev-win64.zip) (also available for other platforms as part of [PGE Project](https://github.com/WohlSoft/PGE-Project)) - a little music player which uses SDL Mixer X library (fork of the SDL Mixer 2.0) which has embedded libADLMIDI to play MIDI files independently from operating system's settings and drivers. <br>(source code of player can be find [here](https://github.com/WohlSoft/PGE-Project/tree/master/MusicPlayer) and source code of SDL Mixer X [here](https://github.com/WohlSoft/SDL-Mixer-X/))
* [ADLMIDI Player for Android](https://github.com/Wohlstand/ADLMIDI-Player-Java/releases) - a little MIDI-player for Android which uses libADLMIDI to play MIDI files and provides flexible GUI with ability to change bank, flags, number of emulated chips, etc.

# Todo
* Check out for XG/GS standards to provide a support to use any channels as percussion and also check some of SysEx commands.
* Add support of MIDI Format 2 files (FL Studio made MIDI-files are wired and opening of those files making lossy of tempo and some meta-information events)

# Changelog
## 1.3.2   dev
 * Added ability to disable MUS and XMI converters
 * Added ability to disable embedded MIDI sequencer to use library as RealTime synthesizer only or use any custom MIDI sequencer plugins.
 * Fixed blank instruments fallback in multi-bank support. When using non-zero bank, if instrument is blank, then, instrument will be taken from a root (I.e. zero bank).

## 1.3.1   2017-12-16
 * Added Real-Time MIDI API (MIDI event functions and adl_generate() to generate PCM between of event rows) which allows you to implement plugin for media players or even a real time MIDI playing driver.
 * Fixed some bugs
 * Fixed initialization that allows to use adl_generate without passing of any files.
 * No more extra output buffer is used between of PCM output requests
 * Deep Tremolo and Vibrato now can be toggled without reopening of a MIDI file

## 1.3.0   2017-10-17
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
 * Tri-state support for deep-tremolo/vibrato, scale modulators, and legacy adlib percussion mode. -1 means "auto", I.e. default and specified by bank.
 * Added new functions: adl_linkedLibraryVersion(), adl_errorInfo(), adl_tickEvents(), and adl_generate()
 * Error string is no more global, now every ADL_MIDIPlayer instance has own thread-safe error info that can be retreived by using adl_errorInfo() function. The adl_errorString() will return library initialization errors only;
 * Added С++ Extra public API which now includes instrument testing feature (which is required by classic ADLMIDI utility)
 * Multi-bank WOPL files now supported! Feel free to implement GS or XG - compatible bank
 * Added support for DJGPP compiler to build libADLMIDI for DOS to use hardware OPL3 chip
 * Added XG percussion bank channel handling support, XG MIDI files are using custom percussion channels are now playing fine!
 * Fixed damaged playback while loop music caused by state of controllers came from end of current melody
 * Added Brightness (CC74) controller which will affect modulator scale

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

