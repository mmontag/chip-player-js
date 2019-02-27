import Player from "./Player.js";
import App from "../App";

let emu = null;
let libgme = null;
const INT16_MAX = 65535;
const fileExtensions = [
  'nsf',
  'nsfe',
  'spc',
  'gym',
  'vgm',
  'vgz',
  'ay',
];

export default class GMEPlayer extends Player {
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);

    libgme = chipCore;
    this.paused = false;
    this.fileExtensions = fileExtensions;
    this.subtune = 0;
    this.tempo = 1.0;

    this.buffer = libgme.allocate(this.bufferSize * 16, 'i16', libgme.ALLOC_NORMAL);
    this.emuPtr = libgme.allocate(1, 'i32', libgme.ALLOC_NORMAL);

    this.setAudioProcess(this.gmeAudioProcess);
  }

  gmeAudioProcess(e) {
    let i, channel;
    const channels = [];
    for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
      channels[channel] = e.outputBuffer.getChannelData(channel);
    }

    if (this.paused) {
      for (channel = 0; channel < channels.length; channel++) {
        channels[channel].fill(0);
      }
      return;
    }

    if (this.getPositionMs() >= this.getDurationMs() && this.fadingOut === false) {
      console.log('Fading out at %d ms.', this.getPositionMs());
      this.setFadeout(this.getPositionMs());
      this.fadingOut = true;
    }

    if (libgme._gme_track_ended(emu) !== 1) {
      libgme._gme_play(emu, this.bufferSize * 2, this.buffer);

      for (channel = 0; channel < channels.length; channel++) {
        for (i = 0; i < this.bufferSize; i++) {
          channels[channel][i] = libgme.getValue(this.buffer +
            // Interleaved channel format
            i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
            channel * 2,            // channel offset * bytes per sample
            'i16') / INT16_MAX;     // convert int16 to float
        }
      }
    } else {
      this.subtune++;

      if (this.subtune >= libgme._gme_track_count(emu) || this.playSubtune(this.subtune) !== 0) {
        this.suspend();
        this.onPlayerStateUpdate(true);
      }
    }
  }

  restart() {
    this.fadingOut = false;
    libgme._gme_seek(emu, 0);
    this.resume();
  }

  playSubtune(subtune) {
    this.fadingOut = false;
    this.subtune = subtune;
    this.metadata = this._parseMetadata(subtune);
    this.onPlayerStateUpdate(false);
    return libgme._gme_start_track(emu, subtune);
  }

  loadData(data, filepath) {
    this.subtune = 0;
    this.fadingOut = false;
    this.filepathMeta = Player.metadataFromFilepath(filepath);
    this.connect();

    if (libgme.ccall(
      "gme_open_data",
      "number",
      ["array", "number", "number", "number"],
      [data, data.length, this.emuPtr, this.audioCtx.sampleRate]
    ) !== 0) {
      console.error("gme_open_data failed.");
      this.stop();
      throw Error('Unable to load this file!');
    }
    emu = libgme.getValue(this.emuPtr, "i32");
    if (this.playSubtune(this.subtune) !== 0) {
      console.error("gme_start_track failed.");
      throw Error('Unable to play this subtune!');
    }

    this.resume();
  }

  _parseMetadata(subtune) {
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

    const meta = {};

    meta.length = readInt32();
    meta.intro_length = readInt32();
    meta.loop_length = readInt32();
    meta.play_length = readInt32();

    offset += 4 * 12; // skip unused bytes

    meta.system = readString();
    meta.game = readString();
    meta.title = readString() || meta.game || this.filepathMeta.title;
    meta.artist = readString() || this.filepathMeta.artist;
    meta.copyright = readString();
    meta.comment = readString();

    meta.formatted = {
      title: meta.game === meta.title ?
        meta.title :
        App.allOrNone(meta.game, ' - ') + meta.title,
      subtitle: [meta.artist, meta.system].filter(x => x).join(' - ') +
        App.allOrNone(' (', meta.copyright, ')'),
    };

    return meta;
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

  getSubtune() {
    return this.subtune;
  }

  getPositionMs() {
    if (emu) return libgme._gme_tell_scaled(emu);
    return 0;
  }

  getDurationMs() {
    if (emu) return this.metadata.play_length;
    return 0;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused() && libgme._gme_track_ended(emu) !== 1;
  }

  setTempo(val) {
    this.tempo = val;
    if (emu) libgme._gme_set_tempo(emu, val);
  }

  setFadeout(startMs) {
    if (emu) libgme._gme_set_fade(emu, startMs, 4000);
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

  seekMs(positionMs) {
    if (emu) {
      Player.muteAudioDuringCall(this.audioNode, () =>
        libgme._gme_seek_scaled(emu, positionMs));
    }
  }

  stop() {
    this.suspend();
    if (emu) libgme._gme_delete(emu);
    emu = null;
    this.onPlayerStateUpdate(true);
  }
}