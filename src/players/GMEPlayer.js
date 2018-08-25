import Player from "./Player";
const LibGME = require('../libgme');

let emu = null;
let libgme = null;
const INT16_MAX = Math.pow(2, 16) - 1;
const BUFFER_SIZE = 1024;

export default class GMEPlayer extends Player {
  constructor(audioContext, initCallback) {
    super();

    libgme = new LibGME({
      // Look for .wasm file in web root, not the same location as the app bundle (static/js).
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm') || path.endsWith('.wast')) return './' + path;
        return prefix + path;
      },
      onRuntimeInitialized: () => {
        this.isReady = true;
        if (typeof initCallback === 'function') {
          initCallback();
        }
      },
    });
    this.isReady = false;
    this.audioCtx = audioContext;
    this.paused = false;
    this.metadata = {};
    this.audioNode = null;
  }

  togglePause() {
    this.paused = !this.paused;
    return this.paused;
  }

  isPaused() {
    return this.paused;
  }

  resume() {
    this.paused = false;
  }

  restart() {
    libgme._gme_seek(emu, 0);
    this.resume();
  }

  playSubtune(subtune) {
    this.metadata = this.parseMetadata(subtune);
    return libgme._gme_start_track(emu, subtune);
  }

  loadData(data, callback = null) {
    let endSongCallback = callback;
    const subtune = 0;
    const trackEnded = libgme._gme_track_ended(emu) === 1;
    const buffer = libgme.allocate(BUFFER_SIZE * 16, "i16", libgme.ALLOC_NORMAL);
    const emuPtr = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);

    if (!this.audioNode) {
      this.audioNode = this.audioCtx.createScriptProcessor(BUFFER_SIZE, 2, 2);
      this.audioNode.connect(this.audioCtx.destination);
      this.audioNode.onaudioprocess = (e) => {
        let i, channel;
        const channels = [];
        for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
          channels[channel] = e.outputBuffer.getChannelData(channel);
        }

        if (this.paused || trackEnded) {
          if (trackEnded && typeof endSongCallback === 'function') {
            endSongCallback();
            endSongCallback = null;
          }
          for (channel = 0; channel < channels.length; channel++) {
            channels[channel].fill(0);
          }
          return;
        }

        libgme._gme_play(emu, BUFFER_SIZE * 2, buffer);

        for (channel = 0; channel < channels.length; channel++) {
          for (i = 0; i < BUFFER_SIZE; i++) {
            channels[channel][i] = libgme.getValue(buffer +
              // Interleaved channel format
              i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
              channel * 2,            // channel offset * bytes per sample
              "i16") / INT16_MAX;     // convert int16 to float
          }
        }
      };
      window.savedReferences = [this.audioCtx, this.audioNode];
    }

    if (libgme.ccall("gme_open_data", "number", ["array", "number", "number", "number"], [data, data.length, emuPtr, this.audioCtx.sampleRate]) !== 0) {
      console.error("gme_open_data failed.");
      return;
    }
    emu = libgme.getValue(emuPtr, "i32");
    if (this.playSubtune(subtune) !== 0) {
      console.error("gme_start_track failed.");
      return;
    }

    this.resume();
  }

  setVoices(voices) {
    let mask = 0;
    voices.forEach((isEnabled, i) => {
      if (!isEnabled) {
        mask += 1 << i;
      }
    });
    if (emu) libgme._gme_mute_voices(emu, mask);
  }

  setTempo(val) {
    if (emu) libgme._gme_set_tempo(emu, val);
  }

  setFadeout(startMs) {
    if (emu) libgme._gme_set_fade(emu, startMs);
  }

  getPosition() {
    if (emu) return libgme._gme_tell(emu);
  }

  parseMetadata(subtune) {
    const metadataPtr = libgme.allocate(1, "i32", libgme.ALLOC_NORMAL);
    if (libgme._gme_track_info(emu, metadataPtr, subtune) !== 0)
      console.error("could not load metadata");
    const ref = libgme.getValue(metadataPtr, "*");

    let offset = 0;

    const readInt32 = function () {
      var value = libgme.getValue(ref + offset, "i32");
      offset += 4;
      return value;
    };

    const readString = function () {
      var value = libgme.Pointer_stringify(libgme.getValue(ref + offset, "i8*"));
      offset += 4;
      return value;
    };

    const res = {};

    res.length = readInt32();
    res.intro_length = readInt32();
    res.loop_length = readInt32();
    res.play_length = readInt32();

    offset += 4 * 12; // skip unused bytes

    res.system = readString();
    res.game = readString();
    res.song = readString();
    res.author = readString();
    res.copyright = readString();
    res.comment = readString();

    return res;
  }

  getVoiceName(index) {
    if (emu) return libgme.Pointer_stringify(libgme._gme_voice_name(emu, index));
  }

  getNumVoices() {
    if (emu) return libgme._gme_voice_count(emu);
  }

  getNumSubtunes() {
    if (emu) return libgme._gme_track_count(emu);
  }

  getDurationMs() {
    return this.metadata.play_length;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return this.isReady && !this.isPaused() && libgme._gme_track_ended(emu) !== 1;
  }

  seekMs(seekMs) {
    if (emu) return libgme._gme_seek(emu, seekMs);
  }

  isReady() {
    return this.isReady;
  }
}