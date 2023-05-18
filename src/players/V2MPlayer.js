import Player from "./Player.js";

const fileExtensions = [
  'v2m',
];

export default class V2MPlayer extends Player {
  constructor(...args) {
    super(...args);

    this.loadData = this.loadData.bind(this);

    this.name = 'Farbrausch V2M Player';
    this.speed = 1;
    this.fileExtensions = fileExtensions;
    this.buffer = this.core.allocate(this.bufferSize * 8, 'i32', this.core.ALLOC_NORMAL);
  }

  loadData(data, filename) {
    const err = this.core.ccall(
      'v2m_open', 'number',
      ['array', 'number', 'number'],
      [data, data.byteLength, this.sampleRate]
    );

    if (err !== 0) {
      console.error("v2m_open failed. error code: %d", err);
      throw Error('Unable to load this file!');
    }

    this.metadata = { title: filename };

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

    const samplesWritten = this.core._v2m_write_audio(this.buffer, this.bufferSize);
    if (samplesWritten === 0) {
      this.stop();
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 4 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 4,                 // channel offset * bytes per sample
          'float'                 // the sample values are 32-bit floating point
        );
      }
    }
  }

  getTempo() {
    return this.speed;
  }

  setTempo(val) {
    this.speed = val;
    return this.core._v2m_set_speed(val);
  }

  getPositionMs() {
    return this.core._v2m_get_position_ms();
  }

  getDurationMs() {
    return this.core._v2m_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(seekMs) {
    this.core._v2m_seek_ms(seekMs);
  }

  stop() {
    this.suspend();
    this.core._v2m_close();
    console.debug('V2MPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
