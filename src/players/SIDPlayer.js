import axios from 'redaxios';
import autoBind from 'auto-bind';
import pathe from 'pathe';

import Player from "./Player.js";
import { vectorToArray } from '../util';
import { API_BASE } from '../config';

const fileExtensions = [
  'sid', 'mus'
];

const DEFAULT_SONG_LENGTH_MS = 2.5 * 60 * 1000;

// Convert song length to milliseconds. Each song length is of format:
// mm:ss[.SSS]
function parseSongLength(length) {
  const parts = length.split(':');
  return Math.floor((parseFloat(parts[0]) * 60 + parseFloat(parts[1])) * 1000);
}

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
    this.subtuneDurations = [];

    this.core._sid_init(this.sampleRate);
  }

  getSidMetadata(md5) {
    console.log("SIDPlayer: Fetching metadata for SID:", md5);
    const metadataUrl = `${API_BASE}/hvsc?sidHash=${md5}`;

    axios.get(metadataUrl, {
      validateStatus: status => status === 404 || (status >= 200 && status < 300)
    }).then(response => {
      if (response.status === 404) return;
      console.log('SIDPlayer: Got metadata for SID:', response.data);
      const { lengths, name, author, copyright, image_url } = response.data;
      this.subtuneDurations = lengths.split(' ').map(parseSongLength);
      this.metadata = {
        imageUrl: image_url,
        formatted: {
          title: name,
          subtitle: `${author} - ${copyright}`,
        },
      };
      this.emit('playerStateUpdate', this.getBasePlayerState());
    });
  }

  loadData(data, filepath, persistedSettings) {
    const dataPtr = this.copyToHeap(data);
    const err = this.core._sid_load_data(dataPtr, data.byteLength);
    this.core._free(dataPtr);
    this.subtuneDurations = Array(this.getNumSubtunes()).fill(DEFAULT_SONG_LENGTH_MS);

    if (err !== 0) {
      throw Error('Unable to load this file!');
    }

    const ptr = this.core._sid_get_song_md5();
    const md5 = this.core.UTF8ToString(ptr);
    this.getSidMetadata(md5);

    this.metadata = { title: pathe.basename(filepath) };

    this.mask = Array(18).fill(true);
    this.core._sid_set_voice_mask(0);
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

    if (this.lastHeapBuffer !== this.core.HEAPU8.buffer) {
      console.debug('SIDPlayer: Detected HEAPU8 buffer change, updating views.');
      this.wasmViewL = new Float32Array(this.core.HEAPU8.buffer, this.bufferL, this.bufferSize);
      this.wasmViewR = new Float32Array(this.core.HEAPU8.buffer, this.bufferR, this.bufferSize);
      this.lastHeapBuffer = this.core.HEAPU8.buffer;
    }

    const samplesWritten = this.core._sid_render(this.bufferL, this.bufferR, this.bufferSize);
    if (samplesWritten === 0 || this.getPositionMs() > this.subtuneDurations[this.getSubtune()]) {
      // TODO: consolidate with GMEPlayer subtune sequencing
      const subtune = this.getSubtune() + 1;
      if (subtune >= this.getNumSubtunes()) this.stop();
      else this.playSubtune(subtune);
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
    this.core._sid_set_speed(val);
    this.speed = val;
  }

  getPositionMs() {
    return this.core._sid_get_position_ms();
  }

  getDurationMs() {
    return this.subtuneDurations[this.getSubtune()];
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
    this.core._sid_stop();
    console.debug('SIDPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
