# Chip Player JS

```
                                            _
                                ________   (_)
                               / ____/ /_  ______
                              / /  _/ __ \/ / __ \
                             / /___/ / / / / /_/ /\
                        ____ \______/ /_/_/ ____/ /         __ ____
                       / __ \/ /__ ____  /_/__  _/___      / /‾___/\
                      / /_/ / /‾__‾ / /\/ /‾_ \/ ‾__/\__  / /\__ \_/
                     / ____/ / /_/ / {_/ /  __/ /‾ __/ /_/ /___/ /\
                    /_/ __/_/\__,_/\__  /\___/_/ /   \____//____/ /
                    \__/  \__/\____│___/ /\_____/     \____\_____/
                                    \___/

```

Play online: [Chip Player JS](https://mmontag.github.io/chip-player-js)

Chip Player JS is a work in progress. Goals:

- [x] Support popular game console formats and tracker formats (not exhaustive)
- [x] Advanced sound control (channel volume, panning, etc.) like [NotSoFatso](https://disch.zophar.net/notsofatso.php)'s stereo and bandlimiting controls
- [x] Built-in online music library like [Chipmachine](http://sasq64.github.io/chipmachine/)
- [x] Simple music management (at least the ability to save favorites) like Winamp/Spotify
- [x] High-quality MIDI playback with JS wavetable synthesis
    * [x] Bonus: user-selectable soundbanks
- [x] Track sequencer with player controls and shuffle mode
- [x] Media key support in Chrome
- [ ] High performance
   - [ ] Cold cache time-to-play under 500 ms (i.e. https://mmontag.github.io/chip-player-js/?play=ModArchives/aryx.s3m in incognito window)
   - [x] Instant search results
   - [x] CPU usage under 25% in most circumstances

## Development Notes

### Architecture

This project was bootstrapped with [Create React App](https://github.com/facebookincubator/create-react-app).

The player engines come from C/C++ libraries such as [game-music-emu](https://bitbucket.org/mpyne/game-music-emu/wiki/Home) and [libxmp](https://github.com/cmatsuoka/libxmp), compiled to JS with [Emscripten](https://kripken.github.io/emscripten-site). Where possible, these projects are incorporated using `git subtree`.

The C/C++ code is compiled by [scripts/build-chip-core.js](scripts/build-chip-core.js). This file also defines the list of exports that will be available to the JavaScript program. Components that go into this build are as follows:

* Manually selected cpp sources from game-music-emu.
* For libraries with their own build system like LibXMP and Fluidlite (detailed [below](#Building)):
    * Build a static library with Emscripten (i.e. using emconfigure, emmake)
    * Link the static library in build-chip-core.js
* **tinyplayer.c**: a super light MIDI file reader/player
* **showcqtbar.c**: a modified [FFMPEG plugin](https://github.com/mfcc64/html5-showcqtbar) providing lovely [constant Q](https://en.wikipedia.org/wiki/Constant-Q_transform#Comparison_with_the_Fourier_transform) spectrum analysis for the visualizer.

The music catalog is created by [scripts/build-catalog.js](scripts/build-catalog.js). **This script looks for a ./catalog folder to build a music index.** This location is untracked, so put a symlink here that points to your local music archive. TODO: Document the corresponding public location (`CATALOG_PREFIX`).

### Local Development Setup

Prerequisites: yarn, cmake, emsdk.

* Clone the repository. 
* Run `yarn install`.

In building the subprojects, we ultimately invoke `emmake make` instead of `make` to yield an object file that can Emscripten can link to in the final build.

* Install the [Emscripten SDK (emsdk)](https://github.com/emscripten-core/emsdk).
* The build script in [package.json](package.json) looks for the emsdk in `~/src/emsdk`. Modify this line to match your emsdk install location if necessary:

  ```"build-chip-core": "source ~/src/emsdk/emsdk_env.sh; node scripts/build-chip-core.js",```

#### User Accounts and Saved Favorites Functionality

User account management is provided through Firebase Cloud Firestore. You must obtain your own [Google Firebase](https://console.firebase.google.com/) credentials and update [src/config/firebaseConfig.js](src/config/firebaseConfig.js) with these credentials. This file is not tracked. Without these credentials, Login/Favorites functionality won't work.

#### Subproject: libxmp-lite

Our goal is to produce **libxmp/libxmp-lite-stagedir/lib/libxmp-lite.a**.
Build libxmp (uses GNU make):

```sh
cd chip-player-js/libxmp/        # navigate to libxmp root
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
autoconf
emconfigure ./configure
emmake make
```

Proceed to build libxmp-lite:

```sh
emmake make -f Makefile.lite     # this will have some errors, but they can be ignored
cd libxmp-lite-stagedir/
autoconf
emconfigure ./configure --enable-static --disable-shared
emmake make
```

#### Subproject: fluidlite

Our goal is to produce **fluidlite/build/libfluidlite.a**.
Build fluidlite (uses Cmake):

```sh
cd chip-player-js/fluidlite/     # navigate to fluidlite root
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
mkdir build                      # create a build folder for Cmake output
cd build                         
emcmake cmake -DDISABLE_SF3=1 .. # Cmake will generate a Makefile by default
emmake make fluidlite-static
```

Once these are in place we can build the parent project.
Our goal is to produce **public/chip-core.wasm**.

```sh
cd chip-player-js/
yarn run build-chip-core
```

This will use object files created in the previous steps and link them into chip-core.wasm.
If you change some C/C++ component of the subprojects, you'll need to redo this process.
Once we have chip-core.wasm, we can proceed to develop JavaScript interactively on localhost:

```sh
yarn start
```

Build the entire project:

```sh
yarn build
```

Or deploy to Github Pages:

```sh
yarn deploy
```

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

- SF2 Overview: https://schristiancollins.wordpress.com/2016/03/02/using-soundfonts-in-2016/
- Timidity compiled by Emscripten: https://bitmidi.com/
    * https://github.com/feross/timidity/commit/d1790eef24ff3b4067c536e45aa88c0863ad9676
    * Uses the 32 MB ["Old FreePats sound set"](http://freepats.zenvoid.org/SoundSets/general-midi.html)
- SoundFonts at MuseScore: https://musescore.org/en/handbook/soundfonts-and-sfz-files#list
- SoundFonts at Woolyss: https://woolyss.com/chipmusic-soundfonts.php
- MIDI file library: https://github.com/craigsapp/midifile
- FluidSynth Lite, supports SF3: https://github.com/divideconcept/FluidLite
- Compress SF2 to SF3: https://github.com/cognitone/sf2convert

##### SoundFont credits

Diverse and usable GM SoundFonts.

- GeneralUser SF2 sound bank: http://schristiancollins.com/generaluser.php
- The Ultimate Megadrive SoundFont: https://musical-artifacts.com/artifacts/24
- NTONYX SoundFont: http://ntonyx.com/sf_f.htm

##### Music archives

- The best pop music MIDI archive comes from [Colin Raffel's thesis work](https://colinraffel.com/projects/lmd/) on MIDI alignment. About 20,000 cleaned MIDI files
    * Colin Raffel. "Learning-Based Methods for Comparing Sequences, with Applications to Audio-to-MIDI Alignment and Matching". PhD Thesis, 2016.

#### Miscellaneous

[ISO 226 Equal loudness curves](https://github.com/IoSR-Surrey/MatlabToolbox/blob/master/%2Biosr/%2Bauditory/iso226.m)

## License

A word about licensing: chip-player-js represents the hard work of many individuals because it is built upon several open-source projects. Each of these projects carries their own license restrictions, and chip-player-js as a whole must adhere to the most restrictive licenses among these. Therefore, chip-player-js is *generally* licensed under [GPLv3](LICENSE). 

However, each subdirectory in this project *may* contain additional, more specific license info that pertains to files contained therein. For example, the code under [src/](src) is written by me and is more permissively [MIT licensed](src/LICENSE).
