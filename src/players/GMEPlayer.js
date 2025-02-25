import Player from "./Player.js";
import SubBass from "../effects/SubBass";
import { allOrNone, remap01 } from '../util';
import path from 'path';
import autoBind from 'auto-bind';

let core = null;

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
  'ay',
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
  paramDefs = [
    {
      id: 'subbass',
      label: 'Sub Bass',
      type: 'number',
      min: 0.0,
      max: 2.0,
      step: 0.01,
      defaultValue: 1.0,
    },
    {
      id: 'stereoWidth',
      label: 'Stereo Width',
      type: 'number',
      min: 0.0,
      max: 1.0,
      step: 0.01,
      defaultValue: 1.0,
    },
    {
      id: 'disableEcho',
      label: 'Disable SPC Echo',
      hint: 'Disable echo effect for Super Nintendo SPC files.',
      type: 'toggle',
      defaultValue: false,
    },
    {
      id: 'enableAccuracy',
      label: 'Accurate SPC Filter',
      hint: 'Simple low-pass and high-pass filter to better match sound output of the SNES.',
      type: 'toggle',
      defaultValue: false,
    },
  ];

  constructor(...args) {
    super(...args);
    autoBind(this);

    core = this.core;

    this.name = 'Game Music Emu Player';
    this.paused = false;
    this.fileExtensions = fileExtensions;
    this.subtune = 0;
    this.tempo = 1.0;
    this.params = { subbass: 1 };
    this.voiceMask = []; // GME does not expose a method to get the current voice mask
    this.gmeCtx = null;

    // Seek timeslicing TODO: move to base Player
    this.seekRequestId = null;
    this.seekTargetMs = null;
    this.currentFileExt = null;

    this.buffer = core._malloc(this.bufferSize * 16); // i16
    this.emuPtr = core._malloc(4); // i32

    this.subBass = new SubBass(this.sampleRate);

    this.params = {};
    this.paramDefs.forEach(p => this.setParameter(p.id, p.defaultValue));
  }

  processAudioInner(channels) {
    let ch, i;
    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    if (this.getPositionMs() >= this.getDurationMs() && this.fadingOut === false) {
      console.log('Fading out at %d ms.', this.getPositionMs());
      this.setFadeout(this.getPositionMs());
      this.fadingOut = true;
    }

    if (core._gme_track_ended(this.gmeCtx) !== 1) {
      core._gme_play(this.gmeCtx, this.bufferSize * 2, this.buffer);

      for (ch = 0; ch < channels.length; ch++) {
        for (i = 0; i < this.bufferSize; i++) {
          channels[ch][i] = core.getValue(this.buffer +
            // Interleaved channel format
            i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
            ch * 2,                 // chhannel offset * bytes per sample
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

      if (this.params.stereoWidth < 1) {
        const width = remap01(this.params.stereoWidth, 0.5, 1);
        for (i = 0; i < this.bufferSize; i++) {
          const left = channels[0][i];
          const right = channels[1][i];
          channels[0][i] = left * width + right * (1 - width);
          channels[1][i] = right * width + left * (1 - width);
        }
      }

      if (this.params.subbass > 0) {
        for (i = 0; i < this.bufferSize; i++) {
          const sub = this.subBass.process(channels[0][i]) * this.params.subbass;
          for (ch = 0; ch < channels.length; ch++) {
            channels[ch][i] += sub;
          }
        }
      }
    } else {
      this.subtune++;

      if (this.subtune >= core._gme_track_count(this.gmeCtx) || this.playSubtune(this.subtune) !== 0) {
        this.suspend();
        console.debug(
          'GMEPlayer.gmeAudioProcess(): _gme_track_ended == %s and subtune (%s) > _gme_track_count (%s).',
          core._gme_track_ended(this.gmeCtx),
          this.subtune,
          core._gme_track_count(this.gmeCtx)
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
    return core._gme_start_track(this.gmeCtx, subtune);
  }

  loadData(data, filepath, subtune = 0) {
    this.subtune = subtune;
    this.fadingOut = false;
    this.seekTargetMs = null;
    this.seekRequestId = null;
    this.currentFileExt = path.extname(filepath);
    this.filepathMeta = Player.metadataFromFilepath(filepath);
    const formatNeedsBass = filepath.match(
      /(\.sgc$|\.kss$|\.nsfe?$|\.ay$|Master System|Game Gear)/i
    );
    this.params.subbass = formatNeedsBass ? 1 : 0;

    const dataPtr = this.copyToHeap(data);
    const err = core._gme_open_data(dataPtr, data.length, this.emuPtr, this.sampleRate);
    core._free(dataPtr);

    if (err !== 0) {
      this.stop();
      throw Error('gme_open_data failed');
    }
    this.gmeCtx = core.getValue(this.emuPtr, "i32");
    this.voiceMask = Array(core._gme_voice_count(this.gmeCtx)).fill(true);

    core._gme_ignore_silence(this.gmeCtx, 0);
    core._gme_set_stereo_depth(this.gmeCtx, this.params.stereoWidth);
    core._gme_disable_echo(this.gmeCtx, this.params.disableEcho ? 1 : 0);

    this.resume();
    if (this.playSubtune(this.subtune) !== 0) {
      this.stop();
      throw Error('gme_start_track failed');
    }
  }

  _parseMetadata(subtune) {
    const metadataPtr = core._malloc(4); // i32
    if (core._gme_track_info(this.gmeCtx, metadataPtr, subtune) !== 0)
      console.error("could not load metadata");
    const ref = core.getValue(metadataPtr, "*");

    let offset = 0;

    const readInt32 = function () {
      var value = core.getValue(ref + offset, "i32");
      offset += 4;
      return value;
    };

    const readString = function () {
      let value = '';

      // Interpret as UTF8 (disabled)
      // value = core.UTF8ToString(core.getValue(ref + offset, "i8*"));

      // Interpret as ISO-8859-1 (unsigned integer values, 0 to 255)
      const ptr = core.getValue(ref + offset, 'i8*');
      for (let i = 0; i < 255; i++) {
        let char = core.getValue(ptr + i, 'i8');
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
    if (this.gmeCtx) return core.UTF8ToString(core._gme_voice_name(this.gmeCtx, index));
  }

  getNumVoices() {
    if (this.gmeCtx) return core._gme_voice_count(this.gmeCtx);
  }

  getNumSubtunes() {
    if (this.gmeCtx) return core._gme_track_count(this.gmeCtx);
  }

  getSubtune() {
    return this.subtune;
  }

  getPositionMs() {
    if (this.gmeCtx) return core._gme_tell_scaled(this.gmeCtx);
    return 0;
  }

  getDurationMs() {
    if (this.gmeCtx) return this.metadata.play_length;
    return 0;
  }

  getMetadata() {
    return this.metadata;
  }

  getParameter(id) {
    return this.params[id];
  }

  setParameter(id, value) {
    switch (id) {
      case 'subbass':
        this.params[id] = parseFloat(value);
        break;
      case 'stereoWidth':
        this.params[id] = parseFloat(value);
        if (this.gmeCtx) core._gme_set_stereo_depth(this.gmeCtx, value);
        break;
      case 'disableEcho':
        this.params[id] = !!value;
        if (this.gmeCtx) core._gme_disable_echo(this.gmeCtx, value ? 1 : 0);
        break;
      case 'enableAccuracy':
        this.params[id] = !!value;
        if (this.gmeCtx) core._gme_enable_accuracy(this.gmeCtx, value ? 1 : 0);
      default:
        console.warn('GMEPlayer has no parameter with id "%s".', id);
    }
  }

  isPlaying() {
    return !this.isPaused() && core._gme_track_ended(this.gmeCtx) !== 1;
  }

  getTempo() {
    return this.tempo;
  }

  setTempo(val) {
    this.tempo = val;
    if (this.gmeCtx) core._gme_set_tempo(this.gmeCtx, val);
  }

  setFadeout(startMs) {
    if (this.gmeCtx) core._gme_set_fade(this.gmeCtx, startMs, 4000);
  }

  getVoiceMask() {
    return this.voiceMask;
  }

  setVoiceMask(voiceMask) {
    if (this.gmeCtx) {
      let bitmask = 0;
      voiceMask.forEach((isEnabled, i) => {
        if (!isEnabled) {
          bitmask += 1 << i;
        }
      });
      core._gme_mute_voices(this.gmeCtx, bitmask);
      // Disable silence detection if any voice is muted.
      core._gme_ignore_silence(this.gmeCtx, bitmask === 0 ? 0 : 1);
      console.log('GMEPlayer: Silence detection is %s.', bitmask === 0 ? 'enabled' : 'disabled');
      this.voiceMask = voiceMask;
    }
  }

  doIncrementalSeek(seekMsIncrement) {
    // console.log('Scheduling incremental seek of %s ms...', seekMsIncrement);
    this.seekRequestId = requestIdleCallback(() => {
      const seekIntermediateMs = Math.min(this.getPositionMs() + seekMsIncrement, this.seekTargetMs);
      core._gme_seek_scaled(this.gmeCtx, seekIntermediateMs);
      if (seekIntermediateMs < this.seekTargetMs) {
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        // console.log('Done Seeking');
        core._gme_set_tempo(this.gmeCtx, this.tempo);
        this.seekTargetMs = null;
        this.seekRequestId = null;
      }
    });
  }

  seekMs(positionMs) {
    if (this.gmeCtx) {
      if (TIMESLICED_SEEK_MS_MAP[this.currentFileExt]) {
        cancelIdleCallback(this.seekRequestId);
        this.seekTargetMs = positionMs;
        const seekMsIncrement = TIMESLICED_SEEK_MS_MAP[this.currentFileExt];
        if (positionMs < this.getPositionMs()) {
          // reset to position 0 if seeking backward
          core._gme_seek_scaled(this.gmeCtx, 0);
        }
        core._gme_set_tempo(this.gmeCtx, 2);
        this.doIncrementalSeek(seekMsIncrement);
      } else {
        this.muteAudioDuringCall(this.audioNode, () =>
          core._gme_seek_scaled(this.gmeCtx, positionMs));
      }
    }
  }

  stop() {
    this.suspend();
    if (this.gmeCtx) core._gme_delete(this.gmeCtx);
    this.gmeCtx = null;
    console.debug('GMEPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
