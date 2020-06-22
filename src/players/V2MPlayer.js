import Player from "./Player.js";

const fileExtensions = [
  'v2m',
];

export default class V2MPlayer extends Player {
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.loadData = this.loadData.bind(this);

    this.lib = chipCore;
    this.fileExtensions = fileExtensions;
    this.buffer = chipCore.allocate(this.bufferSize * 8, 'i32', chipCore.ALLOC_NORMAL);
    this.setAudioProcess(this.v2mAudioProcess);
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    err = this.lib.ccall(
      'v2m_open', 'number',
      ['array', 'number', 'number'],
      [data, data.byteLength, this.audioCtx.sampleRate]
    );

    if (err !== 0) {
      console.error("v2m_open failed. error code: %d", err);
      throw Error('Unable to load this file!');
    }

    this.metadata = { title: filename };

    this.connect();
    this.resume();
    this.onPlayerStateUpdate(false);
  }

  v2mAudioProcess(e) {
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

    const samplesWritten = this.lib._v2m_write_audio(this.buffer, this.bufferSize);
    if (samplesWritten === 0) {
      this.stop();
    }

    for (channel = 0; channel < channels.length; channel++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[channel][i] = this.lib.getValue(
          this.buffer +           // Interleaved channel format
          i * 4 * 2 +             // frame offset   * bytes per sample * num channels +
          channel * 4,            // channel offset * bytes per sample
          'float'                 // the sample values are signed 16-bit integers
        );
      }
    }
  }

  setVoices(voices) {
    console.error('Unable to set voices for this file format.');
  }

  setTempo(val) {
    return this.lib._v2m_set_speed(val);
    // console.error('Unable to set speed for this file format.');
  }

  getPositionMs() {
    return this.lib._v2m_get_position_ms();
  }

  getDurationMs() {
    return this.lib._v2m_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(seekMs) {
    this.lib._v2m_seek_ms(seekMs);
  }

  stop() {
    this.suspend();
    this.lib._v2m_close();
    console.debug('V2MPlayer.stop()');
    this.onPlayerStateUpdate(true);
  }
}
