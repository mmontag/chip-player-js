import Player from "./Player.js";
import autoBind from 'auto-bind';
import { vectorToArray } from '../util';

const fileExtensions = [
  'sid', 'mus'
];

export default class SIDPlayer extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    this.playerKey = 'sid';
    this.name = 'SID Player';
    this.speed = 1;
    this.fileExtensions = fileExtensions;
    this.bufferL = this.core._malloc(this.bufferSize * 4);
    this.bufferR = this.core._malloc(this.bufferSize * 4);

    this.core._sid_init(this.sampleRate);
  }

  loadData(data, filename, persistedSettings) {
    const dataPtr = this.copyToHeap(data);
    const err = this.core._sid_load_data(dataPtr, data.byteLength);
    this.core._free(dataPtr);

    if (err !== 0) {
      throw Error('Unable to load this file!');
    }

    this.metadata = { title: filename };

    this.mask = Array(18).fill(1);
    this.resolveParamValues(persistedSettings);
    this.setTempo(persistedSettings.tempo || 1);
    this.resume();
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false
    });
  }

  processAudioInner(channels) {
    if (this.paused) {
      channels[0].fill(0);
      channels[1].fill(0);
      return;
    }

    if (this.lastHeapBuffer !== this.core.HEAP8.buffer) {
      console.debug('SIDPlayer: Detected HEAP8 buffer change, updating views.');
      this.wasmViewL = new Float32Array(this.core.HEAP8.buffer, this.bufferL, this.bufferSize);
      this.wasmViewR = new Float32Array(this.core.HEAP8.buffer, this.bufferR, this.bufferSize);
      this.lastHeapBuffer = this.core.HEAP8.buffer;
    }

    const samplesWritten = this.core._sid_render(this.bufferL, this.bufferR, this.bufferSize);
    if (samplesWritten === 0) {
      this.stop();
    }

    channels[0].set(this.wasmViewL);
    channels[1].set(this.wasmViewR);
  }

  getNumSubtunes() {
    return this.core._sid_get_num_subtunes();
  }

  getSubtune() {
    return this.core._sid_get_subtune();
  }

  playSubtune(subtune) {
    this.core._sid_set_subtune(subtune);
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false,
    });
  }

  getTempo() {
    return this.speed;
  }

  setTempo(val) {
    this.speed = this.core._sid_set_speed(val) ? val : this.speed;
    return this.speed;
  }

  getPositionMs() {
    return this.core._sid_get_position_ms();
  }

  getDurationMs() {
    return this.core._sid_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  getVoiceGroups() {
    return vectorToArray(this.core.sidGetVoiceGroups()).map((group, g) => ({
      icon: true,
      name: group.groupName,
      voices: vectorToArray(group.voiceNames).map((name, v) => ({ name, idx: g*3+v })),
    }));
  }

  getVoiceMask() {
    return this.mask;
  }

  setVoiceMask(mask) {
    this.mask = mask;

    let bitmask = 0;
    mask.forEach((b, i) => {
      if (!b) bitmask |= (1 << i);
    });

    this.core._sid_set_voice_mask(bitmask);
  }

  seekMs(seekMs) {
    this.muteAudioDuringCall(this.audioNode, () => this.core._sid_set_position_ms(seekMs));
  }

  stop() {
    this.suspend();
    console.debug('SIDPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
