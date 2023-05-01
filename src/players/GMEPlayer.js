import Player from "./Player.js";
import SubBass from "../effects/SubBass";
import { allOrNone } from '../util';
import path from 'path';

let emu = null;
let libgme = null;
const INT16_MAX = 65535;
// "timesliced" seek in increments to prevent blocking UI/audio callback.
const TIMESLICED_SEEK_MS_MAP = {
  '.spc': 5000,
  '.gym': 10000,
  '.vgm': 10000,
  '.vgz': 10000,
};
const fileExtensions = [
  'nsf',
  'nsfe',
  'spc',
  'gym',
  'vgm',
  'vgz',
  'ay',
  'sgc',
  'kss',
  'gbs',
];

/* TODO: move this elsewhere
 * @see https://developers.google.com/web/updates/2015/08/using-requestidlecallback
 */
window.requestIdleCallback = window.requestIdleCallback ||
  function (cb) {
    return setTimeout(function () {
      var start = Date.now();
      cb({
        didTimeout: false,
        timeRemaining: function () {
          return Math.max(0, 50 - (Date.now() - start));
        }
      });
    }, 1);
  };

window.cancelIdleCallback = window.cancelIdleCallback ||
  function (id) {
    clearTimeout(id);
  };

export default class GMEPlayer extends Player {
  constructor(audioCtx, destNode, chipCore, bufferSize) {
    super(audioCtx, destNode, chipCore, bufferSize);
    this.setParameter = this.setParameter.bind(this);
    this.getParameter = this.getParameter.bind(this);
    this.getParamDefs = this.getParamDefs.bind(this);

    libgme = chipCore;
    this.paused = false;
    this.fileExtensions = fileExtensions;
    this.subtune = 0;
    this.tempo = 1.0;
    this.params = { subbass: 1 };
    this.voiceMask = []; // GME does not expose a method to get the current voice mask

    // Seek timeslicing TODO: move to base Player
    this.seekRequestId = null;
    this.seekTargetMs = null;
    this.currentFileExt = null;

    this.buffer = libgme.allocate(this.bufferSize * 16, 'i16', libgme.ALLOC_NORMAL);
    this.emuPtr = libgme.allocate(1, 'i32', libgme.ALLOC_NORMAL);

    this.subBass = new SubBass(audioCtx.sampleRate);

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

      // A hacky fade to prevent pops during timeslice seeking
      if (this.seekTargetMs) {
        const fadeLength = Math.min(256, this.bufferSize / 2);
        for (i = 0; i < fadeLength; i++) {
          const fade = i/fadeLength;
          channels[0][i] *= fade;
          channels[1][i] *= fade;
          channels[0][this.bufferSize - (i + 1)] *= fade;
          channels[1][this.bufferSize - (i + 1)] *= fade;
        }
        const attenuate = 0.2; // attenuate during seeking
        for (i = 0; i < this.bufferSize; i++) {
          channels[0][i] *= attenuate;
          channels[1][i] *= attenuate;
        }
      }

      if (this.params.subbass > 0) {
        for (i = 0; i < this.bufferSize; i++) {
          const sub = this.subBass.process(channels[0][i]) * this.params.subbass;
          for (let ch = 0; ch < channels.length; ch++) {
            channels[ch][i] += sub;
          }
        }
      }
    } else {
      this.subtune++;

      if (this.subtune >= libgme._gme_track_count(emu) || this.playSubtune(this.subtune) !== 0) {
        this.suspend();
        console.debug(
          'GMEPlayer.gmeAudioProcess(): _gme_track_ended == %s and subtune (%s) > _gme_track_count (%s).',
          libgme._gme_track_ended(emu),
          this.subtune,
          libgme._gme_track_count(emu)
        );
        this.emit('playerStateUpdate', { isStopped: true });
      }
    }
  }

  playSubtune(subtune) {
    this.fadingOut = false;
    this.subtune = subtune;
    this.metadata = this._parseMetadata(subtune);
    console.debug('GMEPlayer.playSubtune(subtune=%s)', subtune);
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false,
    });
    return libgme._gme_start_track(emu, subtune);
  }

  loadData(data, filepath) {
    this.subtune = 0;
    this.fadingOut = false;
    this.seekTargetMs = null;
    this.seekRequestId = null;
    this.currentFileExt = path.extname(filepath);
    this.filepathMeta = Player.metadataFromFilepath(filepath);
    const formatNeedsBass = filepath.match(
      /(\.sgc$|\.kss$|\.nsfe?$|\.ay$|Master System|Game Gear)/i
    );
    this.params.subbass = formatNeedsBass ? 1 : 0;

    if (libgme.ccall(
      "gme_open_data",
      "number",
      ["array", "number", "number", "number"],
      [data, data.length, this.emuPtr, this.audioCtx.sampleRate]
    ) !== 0) {
      this.stop();
      throw Error('gme_open_data failed');
    }
    emu = libgme.getValue(this.emuPtr, "i32");
    this.voiceMask = Array(libgme._gme_voice_count(emu)).fill(true);

    // Enable silence detection
    libgme._gme_ignore_silence(emu, 0);

    this.connect();
    this.resume();
    if (this.playSubtune(this.subtune) !== 0) {
      this.stop();
      throw Error('gme_start_track failed');
    }
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
      let value = '';

      // Interpret as UTF8 (disabled)
      // value = libgme.UTF8ToString(libgme.getValue(ref + offset, "i8*"));

      // Interpret as ISO-8859-1 (unsigned integer values, 0 to 255)
      const ptr = libgme.getValue(ref + offset, 'i8*');
      for (let i = 0; i < 255; i++) {
        let char = libgme.getValue(ptr + i, 'i8');
        if (char === 0) {
          break;
        } else if (char < 0) {
          char = (Math.abs(char) ^ 255) + 1;
        }
        value += String.fromCharCode(char);
      }

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
        allOrNone(meta.game, ' - ') + meta.title,
      subtitle: [meta.artist, meta.system].filter(x => x).join(' - ') +
        allOrNone(' (', meta.copyright, ')'),
    };

    return meta;
  }

  getVoiceName(index) {
    if (emu) return libgme.UTF8ToString(libgme._gme_voice_name(emu, index));
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

  getParameter(id) {
    return this.params[id];
  }

  getParamDefs() {
    return [
      {
        id: 'subbass',
        label: 'Sub Bass',
        type: 'number',
        min: 0.0,
        max: 2.0,
        step: 0.01,
        value: 1.0,
      },
    ];
  }

  setParameter(id, value) {
    switch (id) {
      case 'subbass':
        this.params[id] = parseFloat(value);
        break;
      default:
        console.warn('GMEPlayer has no parameter with id "%s".', id);
    }
  }

  isPlaying() {
    return !this.isPaused() && libgme._gme_track_ended(emu) !== 1;
  }

  getTempo() {
    return this.tempo;
  }

  setTempo(val) {
    this.tempo = val;
    if (emu) libgme._gme_set_tempo(emu, val);
  }

  setFadeout(startMs) {
    if (emu) libgme._gme_set_fade(emu, startMs, 4000);
  }

  getVoiceMask() {
    return this.voiceMask;
  }

  setVoiceMask(voiceMask) {
    if (emu) {
      let bitmask = 0;
      voiceMask.forEach((isEnabled, i) => {
        if (!isEnabled) {
          bitmask += 1 << i;
        }
      });
      libgme._gme_mute_voices(emu, bitmask);
      // Disable silence detection if any voice is muted.
      libgme._gme_ignore_silence(emu, bitmask === 0 ? 0 : 1);
      console.log('GMEPlayer: Silence detection is %s.', bitmask === 0 ? 'enabled' : 'disabled');
      this.voiceMask = voiceMask;
    }
  }

  doIncrementalSeek(seekMsIncrement) {
    // console.log('Scheduling incremental seek of %s ms...', seekMsIncrement);
    this.seekRequestId = requestIdleCallback(() => {
      const seekIntermediateMs = Math.min(this.getPositionMs() + seekMsIncrement, this.seekTargetMs);
      libgme._gme_seek_scaled(emu, seekIntermediateMs);
      if (seekIntermediateMs < this.seekTargetMs) {
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        // console.log('Done Seeking');
        libgme._gme_set_tempo(emu, this.tempo);
        this.seekTargetMs = null;
        this.seekRequestId = null;
      }
    });
  }

  seekMs(positionMs) {
    if (emu) {
      if (TIMESLICED_SEEK_MS_MAP[this.currentFileExt]) {
        cancelIdleCallback(this.seekRequestId);
        this.seekTargetMs = positionMs;
        const seekMsIncrement = TIMESLICED_SEEK_MS_MAP[this.currentFileExt];
        if (positionMs < this.getPositionMs()) {
          // reset to position 0 if seeking backward
          libgme._gme_seek_scaled(emu, 0);
        }
        libgme._gme_set_tempo(emu, 2);
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        this.muteAudioDuringCall(this.audioNode, () =>
          libgme._gme_seek_scaled(emu, positionMs));
      }
    }
  }

  stop() {
    this.suspend();
    if (emu) libgme._gme_delete(emu);
    emu = null;
    console.debug('GMEPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
