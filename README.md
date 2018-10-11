# Chip Player JS

Play online: [Chip Player JS](https://mmontag.github.io/chip-player-js)

Chip Player JS is a work in progress. Goals:

- Support popular game console formats and tracker formats (not exhaustive)
- Advanced sound control (channel volume, panning, etc.) like [NotSoFatso](https://disch.zophar.net/notsofatso.php)'s stereo and bandlimiting controls
- Built-in online music library like [Chipmachine](http://sasq64.github.io/chipmachine/)
- Simple music management (at least the ability to save favorites) like Winamp/Spotify
- High-quality MIDI playback with JS wavetable synthesis
    * Bonus: user-selectable soundbanks

### Development Notes

#### Architecture

This project was bootstrapped with [Create React App](https://github.com/facebookincubator/create-react-app).

The player engines come from C/C++ libraries such as [game-music-emu](https://bitbucket.org/mpyne/game-music-emu/wiki/Home) and [libxmp](https://github.com/cmatsuoka/libxmp), compiled to JS with [Emscripten](https://kripken.github.io/emscripten-site).

As more player engines are added, it seems the best way to combine them is probably to create a wrapper C project with one interface that gets exposed through Emscripten, rather than compiling several separate C/C++ libraries to JS and unifying their interfaces in the JS layer. This would make the bindings easier and reduce the overhead of running multiple Emscripten runtimes.

### Related Projects and Resources

##### Chipmachine (Native)
http://sasq64.github.io/chipmachine/

Chipmachine is a multiplatform player supporting an enormous number of formats. Downloads music from an impressive variety of [external sources](https://github.com/sasq64/chipmachine/blob/master/lua/db.lua).
Most of these come from HTTP sources without CORS headers, not feasible for direct playback. 

##### Muki (JS)
http://muki.io

Muki, by [Tomás Pollak](https://github.com/tomas), is a polished JS player pulling together [Timidity (MIDI)](http://timidity.sourceforge.net/), [Munt (MT-32)](https://github.com/munt/munt), [libopenmpt](https://lib.openmpt.org/libopenmpt/) (instead of libxmp), game-music-emu, Wildmidi, Adplug, [Adlmidi (OPL3)](https://bisqwit.iki.fi/source/adlmidi.html), mdxmini, and [sc68](http://sc68.atari.org/apidoc/index.html). The music is a collection of PC game music.

##### Chiptune Blaster (JS)
https://github.com/wothke?tab=repositories

Jeurgen Wothke's collection of chipmusic projects ported to the web with Emscripten. He's beaten me to it, but with a rudimentary player and no built-in music collection. http://www.wothke.ch/blaster

##### SaltyGME (JS)
http://gamemusic.multimedia.cx/about

SaltyGME is a GME-based web player targeting Google Chrome NaCl. (Deprecated)

##### Cirrus Retro (JS)
https://github.com/multimediamike/cirrusretro-players

Cirrusretro is an updated version of SaltyGME compiled with Emscripten. Self-hosted file archive.

##### Audio Overload (Native)
https://www.bannister.org/software/ao.htm

Audio Overload is a multiplatform player supporting 33 formats.

##### JSGME (JS)
http://onakasuita.org/jsgme/

One of the first examples of GME compiled with Emscripten. Music collection is a self-hosted mirror of Famicompo entries.

##### MoseAmp (Native + JS)
https://github.com/osmose/moseamp

MoseAmp is a multiplatform player built with Electron. Some nice game console icons: https://www.deviantart.com/jaffacakelover/art/Pixel-Gaming-Machine-Icons-413704203

#### MIDI Stuff

The best modern option for playing MIDI is probably using a well-designed GM SoundFont bank with a good SoundFont 2.01 implementation like FluidSynth.

- Timidity compiled by Emscripten: https://bitmidi.com/
    * https://github.com/feross/timidity/commit/d1790eef24ff3b4067c536e45aa88c0863ad9676
    * Uses the 32 MB ["Old FreePats sound set"](http://freepats.zenvoid.org/SoundSets/general-midi.html)
- SoundFonts at MuseScore: https://musescore.org/en/handbook/soundfonts-and-sfz-files#list
- SoundFonts at Woolyss: https://woolyss.com/chipmusic-soundfonts.php
- GeneralUser SF2 sound bank: http://schristiancollins.com/generaluser.php and SF2 Overview: https://schristiancollins.wordpress.com/2016/03/02/using-soundfonts-in-2016/
- MIDI file library: https://github.com/craigsapp/midifile
- FluidSynth Lite, supports SF3: https://github.com/divideconcept/FluidLite
- Compress SF2 to SF3: https://github.com/cognitone/sf2convert

##### Music archives

- The best pop music MIDI archive comes from [Colin Raffel's thesis work](https://colinraffel.com/projects/lmd/) on MIDI alignment. About 20,000 cleaned MIDI files
    * Colin Raffel. "Learning-Based Methods for Comparing Sequences, with Applications to Audio-to-MIDI Alignment and Matching". PhD Thesis, 2016.

#### Miscellaneous

[ISO 226 Equal loudness curves](https://github.com/IoSR-Surrey/MatlabToolbox/blob/master/%2Biosr/%2Bauditory/iso226.m)