import Player from "./Player.js";
import autoBind from 'auto-bind';
import { allOrNone } from '../util';

const fileExtensions = [
  'vgm',
  'vgz',
  'gym',
  's98',
  'dro',
];

const INT32_MAX = 0x8000000; // 2147483648

export default class VGMPlayer extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    this.name = 'LibVGM Player';
    this.speed = 1;
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4 * 2);
    this.vgmCtx = this.core._lvgm_init(this.sampleRate);
  }

  loadData(data, filename) {
    const err = this.core.ccall(
      'lvgm_load_data', 'number',
      ['number', 'array', 'number'],
      [this.vgmCtx, data, data.byteLength]
    );
    // const err = this.core._lvgm_load_data(this.vgmCtx, data, data.byteLength);

    if (err !== 0) {
      console.error("lvgm_load_data failed. error code: %d", err);
      throw Error('Unable to load this file!');
    }

    this.core._lvgm_start(this.vgmCtx);

    const metaPtr = this.core._lvgm_get_metadata(this.vgmCtx);
    const meta = {
      title:   this.core.UTF8ToString(this.core.getValue(metaPtr + 0, 'i32')),
      artist:  this.core.UTF8ToString(this.core.getValue(metaPtr + 4, 'i32')),
      game:    this.core.UTF8ToString(this.core.getValue(metaPtr + 8, 'i32')),
      system:  this.core.UTF8ToString(this.core.getValue(metaPtr + 12, 'i32')),
      date:    this.core.UTF8ToString(this.core.getValue(metaPtr + 16, 'i32')),
      comment: this.core.UTF8ToString(this.core.getValue(metaPtr + 20, 'i32')),
    };

    // Custom title/subtitle formatting, same as GMEPlayer:
    meta.formatted = {
      title: meta.game === meta.title ?
        meta.title :
        allOrNone(meta.game, ' - ') + meta.title,
      subtitle: [meta.artist, meta.system].filter(x => x).join(' - ') +
        allOrNone(' (', meta.date, ')'),
    };
    this.metadata = meta;

    this.resume();
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false
    });
  }

  processAudioInner(channels) {
    let i, ch;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    const samplesWritten = this.core._lvgm_render(this.vgmCtx, this.buffer, this.bufferSize);
    if (samplesWritten === 0) {
      this.stop();
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 4 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 4,                 // channel offset * bytes per sample
          'i32'                   // the sample values are 32-bit integer
        ) / INT32_MAX;
      }
    }
  }

  getTempo() {
    if (this.vgmCtx)
      return this.core._lvgm_get_playback_speed(this.vgmCtx);
  }

  setTempo(val) {
    if (this.vgmCtx)
      return this.core._lvgm_set_playback_speed(this.vgmCtx, val);
  }

  getPositionMs() {
    if (this.vgmCtx)
      return this.core._lvgm_get_position_ms(this.vgmCtx);
  }

  getDurationMs() {
    if (this.vgmCtx)
      return this.core._lvgm_get_duration_ms(this.vgmCtx);
  }

  getNumVoices() {
    if (this.vgmCtx)
      return this.core._lvgm_get_voice_count(this.vgmCtx);
  }

  getVoiceGroups() {
    const voiceGroups = [];
    const numVoices = this.core._lvgm_get_voice_count(this.vgmCtx);
    let currChipName;
    let currGroup;
    for (let i = 0; i < numVoices; i++) {
      const voiceName = this.core.UTF8ToString(this.core._lvgm_get_voice_name(this.vgmCtx, i));
      const chipName = this.core.UTF8ToString(this.core._lvgm_get_voice_chip_name(this.vgmCtx, i));
      if (chipName !== currChipName) {
        currGroup = {
          name: chipName,
          voices: [],
        };
        currChipName = chipName;
        voiceGroups.push(currGroup);
      }
      currGroup.voices.push({
        idx: i,
        name: voiceName,
      });
    }
    return voiceGroups;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(seekMs) {
    if (this.vgmCtx)
      this.core._lvgm_seek_ms(this.vgmCtx, seekMs);
  }

  getVoiceName(index) {
    if (this.vgmCtx) return this.core.UTF8ToString(this.core._lvgm_get_voice_name(this.vgmCtx, index));
  }

  getVoiceMask() {
    if (this.vgmCtx) {
      const bitmask = this.core._lvgm_get_voice_mask(this.vgmCtx);
      const voiceMask = [];
      for (let i = 0n; i < 64n; i++) {
        voiceMask.push((bitmask & (1n << i)) === 0n);
      }
      return voiceMask;
    }
    return [];
  }

  setVoiceMask(voiceMask) {
    if (this.vgmCtx) {
      let bitmask = 0n;
      voiceMask.forEach((isEnabled, i) => {
        if (!isEnabled) {
          bitmask += 1n << BigInt(i);
        }
      });
      this.core._lvgm_set_voice_mask(this.vgmCtx, bitmask);
    }
  }

  stop() {
    this.suspend();
    if (this.vgmCtx) this.core._lvgm_stop(this.vgmCtx);
    console.debug('VGMPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
