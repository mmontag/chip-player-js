# Chip Player JS

![Screen Shot 2019-11-19 at 1 21 04 PM](https://user-images.githubusercontent.com/946117/69187458-80955600-0acf-11ea-9a1f-e090032dcb00.png)

Play online: [Chip Player JS](https://mmontag.github.io/chip-player-js). Feature requests? [Create an issue](https://github.com/mmontag/chip-player-js/issues/new).

### Features

- Support popular game console formats and tracker formats (not exhaustive)
- Advanced sound control (channel volume, panning, etc.) like [NotSoFatso](https://disch.zophar.net/notsofatso.php)'s stereo and bandlimiting controls
- Built-in online music library like [Chipmachine](http://sasq64.github.io/chipmachine/)
- Simple music management (at least the ability to save favorites) like Winamp/Spotify
- High-quality MIDI playback with JS wavetable synthesis
    * Bonus: user-selectable soundbanks
- Track sequencer with player controls and shuffle mode
- Media key support in Chrome
- High performance
   - Time-to-audio under 500 ms (i.e. https://mmontag.github.io/chip-player-js/?play=ModArchives/aryx.s3m)
   - Instant search results
   - CPU usage under 25% in most circumstances

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

Prerequisites: npm, cmake, emsdk.

* Clone the repository. 
* Run `npm install`.

In building the subprojects, we ultimately invoke `emmake make` instead of `make` to yield an object file that Emscripten can link to in the final build.

* Install the [Emscripten SDK (emsdk)](https://github.com/emscripten-core/emsdk).
* The build script in [package.json](package.json) looks for the emsdk in `~/src/emsdk`. Modify this line to match your emsdk install location if necessary:

  ```"build-chip-core": "source ~/src/emsdk/emsdk_env.sh; node scripts/build-chip-core.js",```
  
#### Tight coupling to Github Pages

The project is currently set up to deploy to Github Pages. The `deploy` and `deploy-lite` NPM scripts invoke the gh-pages NPM module. 

If you wish to deploy to your own Github Pages account, change the `"homepage"` field in package.json.

#### User Accounts and Saved Favorites Functionality

User account management is provided through Firebase Cloud Firestore. You must obtain your own [Google Firebase](https://console.firebase.google.com/) credentials and update [src/config/firebaseConfig.js](src/config/firebaseConfig.js) with these credentials. This file is not tracked. Without these credentials, Login/Favorites functionality won't work.

#### External project: libxmp-lite

Our goal is to produce **../libxmp/build/libxmp-lite.a** (assumes you have cloned **libxmp** side-by-side with chip-player-js).

A **libxmp** subtree was previously included in this repository, but this is deprecated.

```sh
git clone git@github.com:libxmp/libxmp.git
cd libxmp/
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
mkdir build                      # create a build folder for Cmake output
cd build
emcmake cmake -DBUILD_LITE -DBUILD_STATIC ..  # or use ccmake (see note below)
emmake make
```

Optionally, use **ccmake** instead to configure the build.  See screenshot below for recommended build options. We want `BUILD_LITE` and `BUILD_STATIC`.

```
ccmake -DCMAKE_TOOLCHAIN_FILE="$(dirname $(which emcc))/cmake/Modules/Platform/Emscripten.cmake" ..
```

![image](https://github.com/user-attachments/assets/1ca9de9c-8123-4619-a691-aff289eb6066)


#### External project: fluidlite

Our goal is to produce **../fluidlite/build/libfluidlite.a** (**build-chip-core.js** assumes you have cloned FluidLite side-by-side with chip-player-js). A **fluidlite** subtree was previously included in this repository, but this is deprecated and can be ignored.

Build fluidlite (uses Cmake):

```sh
git clone git@github.com:divideconcept/FluidLite.git
cd FluidLite/                    # navigate to fluidlite root
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
mkdir build                      # create a build folder for Cmake output
cd build                         
emcmake cmake ..                 # Cmake will generate a Makefile
                                 # Note: SF3 support is now disabled by default
                                 # Problems here? Try deleting CMake cache files
emmake make fluidlite-static
```

#### Subproject: psflib and lazyusf2

Our goal is to produce **psflib/libpsflib.a** and **lazyusf2/liblazyusf.a**. These use a special Emscripten.Makefile (loosely based on [Jeurgen Wothke's webn64 .bat script](https://github.com/wothke/webn64/blob/master/emscripten/makeEmscripten.bat)).

Build psflib:

```sh
cd chip-player-js/psflib/
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
emmake make -f Emscripten.Makefile libpsflib.a
```

Build liblazyusf:

```sh
cd ../lazyusf2/
emmake make -f Emscripten.Makefile liblazyusf.a
```

#### Subproject: libvgm

Our goal is to produce **libvgm/build/bin/libvgm-emu.a**, **libvgm/build/bin/libvgm-player.a**, and **libvgm/build/bin/libvgm-util.a**.

```sh
cd chip-player-js/libvgm/
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
mkdir build                      # create a build folder for Cmake output
cd build
emcmake cmake ..
emmake make
```

To reconfigure the build-enabled sound chips with CMake UI, run `emcmake ccmake ..` from the build folder. Make desired changes to build flags, then `c` to configure and `g` to generate new Makefile. Then run `emmake make`. Optionally, commit the same changes back to CMakeLists.txt in libvgm parent folder. 

#### External project: game-music-emu

Our goal is to produce **../game-music-emu/build/libgme.a** (assumes you have cloned **game-music-emu** side-by-side with chip-player-js).

A **game-music-emu** subtree was previously included in this repository, but this is deprecated.

```sh
git clone git@github.com:mmontag/game-music-emu.git
cd game-music-emu/
source ~/src/emsdk/emsdk_env.sh  # load the emscripten environment variables
mkdir build                      # create a build folder for Cmake output
cd build
emcmake cmake ..
emmake make
```

Optionally, use **ccmake** instead to configure the build.  See screenshot below for recommended build options. We do not want VGM support as this is handled by libvgm.

```
ccmake -DCMAKE_TOOLCHAIN_FILE="$(dirname $(which emcc))/cmake/Modules/Platform/Emscripten.cmake" ..
```

![game-music-emu cmake example](https://github.com/user-attachments/assets/1899b5e6-5620-4cf2-b253-672b39212124)

#### WebAssembly build

Once these are in place we can build the parent project.
Our goal is to produce **public/chip-core.wasm**.

```sh
cd chip-player-js/
npm run build-chip-core
```

This will use object files created in the previous steps and link them into chip-core.wasm.
If you change some C/C++ component of the subprojects, you'll need to redo this process.
Once we have chip-core.wasm, we can proceed to develop JavaScript interactively on localhost:

```sh
npm start
```

Build the entire project:

```sh
npm build
```

Or deploy to Github Pages:

```sh
npm deploy
```

Deploy to Github Pages without rebuilding chip-core.wasm: 

```sh
npm deploy-lite
```

## Related Projects

#### Chipmachine (Native)
http://sasq64.github.io/chipmachine/

Chipmachine is a multiplatform player supporting an enormous number of formats. Downloads music from an impressive variety of [external sources](https://github.com/sasq64/chipmachine/blob/master/lua/db.lua).
Most of these come from HTTP sources without CORS headers, not feasible for direct playback. 

#### Muki (JS)
http://muki.io

Muki, by [Tomás Pollak](https://github.com/tomas), is a polished JS player pulling together [Timidity (MIDI)](http://timidity.sourceforge.net/), [Munt (MT-32)](https://github.com/munt/munt), [libopenmpt](https://lib.openmpt.org/libopenmpt/) (instead of libxmp), game-music-emu, Wildmidi, Adplug, [Adlmidi (OPL3)](https://bisqwit.iki.fi/source/adlmidi.html), mdxmini, and [sc68](http://sc68.atari.org/apidoc/index.html). The music is a collection of PC game music.

#### Chiptune Blaster (JS)
https://github.com/wothke?tab=repositories

Jeurgen Wothke's collection of chipmusic projects ported to the web with Emscripten. He's beaten me to it, but with a rudimentary player and no built-in music collection. http://www.wothke.ch/blaster

#### SaltyGME (JS)
http://gamemusic.multimedia.cx/about

SaltyGME is a GME-based web player targeting Google Chrome NaCl. (Deprecated)

#### Cirrus Retro (JS)
https://github.com/multimediamike/cirrusretro-players

Cirrusretro is an updated version of SaltyGME compiled with Emscripten. Self-hosted file archive.

#### Audio Overload (Native)
https://www.bannister.org/software/ao.htm

Audio Overload is a multiplatform player supporting 33 formats.

#### JSGME (JS)
http://onakasuita.org/jsgme/

One of the first examples of GME compiled with Emscripten. Music collection is a self-hosted mirror of Famicompo entries.

#### MoseAmp (Native + JS)
https://github.com/osmose/moseamp

MoseAmp is a multiplatform player built with Electron. Some nice game console icons: https://www.deviantart.com/jaffacakelover/art/Pixel-Gaming-Machine-Icons-413704203

## Resources

### MIDI Stuff

The best modern option for playing MIDI is probably using a well-designed GM SoundFont bank with a good SoundFont 2.01 implementation like FluidSynth.

- SF2 Overview: https://schristiancollins.wordpress.com/2016/03/02/using-soundfonts-in-2016/
- Timidity compiled by Emscripten: https://bitmidi.com/
    * https://github.com/feross/timidity/commit/d1790eef24ff3b4067c536e45aa88c0863ad9676
    * Uses the 32 MB ["Old FreePats sound set"](http://freepats.zenvoid.org/SoundSets/general-midi.html)
- MIDI file library: https://github.com/craigsapp/midifile
- FluidSynth Lite, supports SF3: https://github.com/divideconcept/FluidLite
- Compress SF2 to SF3: https://github.com/cognitone/sf2convert

### SoundFont Credits

#### GM SoundFonts

- GeneralUser SF2 sound bank: http://schristiancollins.com/generaluser.php
- Many excellent piano SoundFonts: https://sites.google.com/site/soundfonts4u
- The Ultimate Megadrive SoundFont: https://musical-artifacts.com/artifacts/24
- NTONYX SoundFont: http://ntonyx.com/sf_f.htm
- SoundFonts at MuseScore: https://musescore.org/en/handbook/soundfonts-and-sfz-files#list
- SoundFonts at Woolyss: https://woolyss.com/chipmusic-soundfonts.php
- Weeds GM3: Rich "Weeds" Nagel, 2010 http://bhservices.us/weeds/Temp/

#### Novelty SoundFonts

- PC Beep: Rich "Weeds" Nagel, 1998

### Music Archive Sources

- MIDI
  - Lakh MIDI Dataset: [Colin Raffel's thesis work](https://colinraffel.com/projects/lmd/) on MIDI alignment. About 20,000 cleaned popular music MIDI files.
      * Colin Raffel. "Learning-Based Methods for Comparing Sequences, with Applications to Audio-to-MIDI Alignment and Matching". PhD Thesis, 2016.
  - Sound Canvas MIDI Collection: https://archive.org/details/sound_canvas_midi_collection
  - Piano E-Competition MIDI: http://www.piano-e-competition.com/midiinstructions.asp
- Video Games
  - VGM Rips: https://vgmrips.net • [VGMRips full download packs](https://vgmrips.net/forum/viewtopic.php?f=1&t=496&start=45&sid=4ff047600e6a72a701d09381b8a01964)
  - VGMusic.com: https://www.vgmusic.com • [VGMusic.com 2018 Archive](https://archive.org/details/31581VideogameMusicMIDIFileswReplayGain8mbgmsfx.sf2)
  - Zophar's Domain: https://www.zophar.net/music
  - Mirsoft World of Game MIDs/MODs: http://mirsoft.info • [Mirsoft July 2021 Torrent](magnet:?xt=urn:btih:c3354503aa06d46c2c77193afb4ff6bc40c0e368&dn=mirsoftJuly2021snapshot&tr=udp%3a%2f%2ftracker.opentrackr.org%3a1337%2fannounce)
- The Mod Archive: https://modarchive.org
- OPL Archive: http://opl.wafflenet.com
- Modland: https://modland.com/pub/modules
- Famicompo NSF Competition: https://mini.famicompo.com/compo/top.html • [Famicompo NSFE Archive](https://www.dropbox.com/s/8snytwvzqcnjn54/Famicompo%20NSFE.rar?dl=1)

### Miscellaneous

[ISO 226 Equal loudness curves](https://github.com/IoSR-Surrey/MatlabToolbox/blob/master/%2Biosr/%2Bauditory/iso226.m)

## License

A word about licensing: chip-player-js represents the hard work of many individuals because it is built upon several open-source projects. Each of these projects carries their own license restrictions, and chip-player-js as a whole must adhere to the most restrictive licenses among these. Therefore, chip-player-js is *generally* licensed under [GPLv3](LICENSE). 

However, each subdirectory in this project *may* contain additional, more specific license info that pertains to files contained therein. For example, the code under [src/](src) is written by me and is more permissively [MIT licensed](src/LICENSE).
