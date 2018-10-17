import Player from "./Player.js";

const INT16_MAX = Math.pow(2, 16) - 1;
const XMP_PLAYER_STATE = 8;
const XMP_STATE_PLAYING = 2;
const fileExtensions = [
  // libxmp-lite:
  'it',  //  Impulse Tracker  1.00, 2.00, 2.14, 2.15
  'mod', //  Sound/Noise/Protracker M.K., M!K!, M&K!, N.T., CD81
  's3m', //  Scream Tracker 3 3.00, 3.01+
  'xm',  //  Fast Tracker II  1.02, 1.03, 1.04
];

export default class XMPPlayer extends Player {
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);

    this.libxmp = chipCore;
    this.xmpCtx = chipCore._xmp_create_context();
    this.bufferSize = 2048;
    this.xmp_frame_infoPtr = chipCore._malloc(2048);
    this.fileExtensions = fileExtensions;
    this.initialBPM = 125;
    this.tempoScale = 1;
    this._positionMs = 0;
    this._durationMs = 1000;
    this.buffer = chipCore.allocate(this.bufferSize * 16, 'i16', chipCore.ALLOC_NORMAL);

    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    const infoPtr = this.xmp_frame_infoPtr;

    this.audioProcess = (e) => {
      let err;
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

      err = xmp._xmp_play_buffer(ctx, this.buffer, this.bufferSize * 4, 1);
      if (err === -1) {
        this.audioNode.disconnect();
        this.audioNode = null;
        this.onPlayerStateUpdate(true);
      } else if (err !== 0) {
        this.audioNode.disconnect();
        console.error("xmp_play_buffer failed. error code: %d", err);
        throw Error('Unable to play this file!');
      }

      // Get current module BPM
      // see http://xmp.sourceforge.net/libxmp.html#id25
      xmp._xmp_get_frame_info(ctx, infoPtr);
      const bpm = xmp.getValue(infoPtr + 6 * 4, 'i32');
      this._positionMs = xmp.getValue(infoPtr + 7 * 4, 'i32'); // xmp_frame_info.time
      this._maybeInjectTempo(bpm);

      for (channel = 0; channel < channels.length; channel++) {
        for (i = 0; i < this.bufferSize; i++) {
          channels[channel][i] = xmp.getValue(
            this.buffer +           // Interleaved channel format
            i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
            channel * 2,            // channel offset * bytes per sample
            'i16'                   // the sample values are signed 16-bit integers
          ) / INT16_MAX;            // convert int16 to float
        }
      }
    };
  }

  _parseMetadata() {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    const res = {};

    // Match layout of xmp_module_info struct
    // http://xmp.sourceforge.net/libxmp.html
    // #void-xmp-get-module-info-xmp-context-c-struct-xmp-module-info-info
    const xmp_module_infoPtr = xmp._malloc(2048);
    xmp._xmp_get_module_info(ctx, xmp_module_infoPtr);
    const xmp_modulePtr = xmp.getValue(xmp_module_infoPtr + 20, '*');
    res.title = xmp.Pointer_stringify(xmp_modulePtr, 256);
    res.system = xmp.Pointer_stringify(xmp_modulePtr + 64, 256);
    res.comment = xmp.Pointer_stringify(xmp.getValue(xmp_module_infoPtr + 24, '*'), 256);

    const infoPtr = this.xmp_frame_infoPtr;
    xmp._xmp_get_frame_info(ctx, infoPtr);
    this._durationMs = xmp.getValue(infoPtr + 8 * 4, 'i32');

    // XMP-specific metadata
    res.patterns = xmp.getValue(xmp_modulePtr + 128 + 4 * 0, 'i32'); // patterns
    res.tracks = xmp.getValue(xmp_modulePtr + 128 + 4 * 1, 'i32'); // tracks
    res.numChannels = xmp.getValue(xmp_modulePtr + 128 + 4 * 2, 'i32'); // tracks per pattern
    res.numInstruments = xmp.getValue(xmp_modulePtr + 128 + 4 * 3, 'i32'); // instruments
    res.numSamples = xmp.getValue(xmp_modulePtr + 128 + 4 * 4, 'i32'); // samples
    res.initialSpeed = xmp.getValue(xmp_modulePtr + 128 + 4 * 5, 'i32'); // initial speed
    res.initialBPM = xmp.getValue(xmp_modulePtr + 128 + 4 * 6, 'i32'); // initial bpm
    res.moduleLength = xmp.getValue(xmp_modulePtr + 128 + 4 * 7, 'i32'); // module length
    res.restartPosition = xmp.getValue(xmp_modulePtr + 128 + 4 * 8, 'i32'); // restart position

    this.initialBPM = res.initialBPM;

    return res;
  }

  restart() {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    xmp._xmp_restart_module(ctx);
    this.resume();
  }

  loadData(data) {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    let err;

    err = xmp.ccall(
      'xmp_load_module_from_memory', 'number',
      ['number', 'array', 'number'],
      [ctx, data, data.length]
    );
    if (err !== 0) {
      console.error("xmp_load_module_from_memory failed. error code: %d", err);
      throw Error('Unable to load this file!');
    }

    err = xmp._xmp_start_player(ctx, this.audioCtx.sampleRate, 0);
    if (err !== 0) {
      console.error('xmp_start_player failed. error code: %d', err);
    }

    this.metadata = this._parseMetadata();

    this.connect();
    this.resume();
  }

  setVoices(voices) {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;

    voices.forEach((isEnabled, i) => {
      xmp._xmp_channel_mute(ctx, i, isEnabled ? 0 : 1);
    });
  }

  setTempo(val) {
    if (!this.metadata.initialSpeed) {
      console.error('Unable to set speed for this file format.');
      return;
    }
    this.tempoScale = val;
  }

  _maybeInjectTempo(measuredBPM) {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;

    const minBPM = 20;
    const maxBPM = 255;
    const targetBPM = Math.floor(Math.max(Math.min(this.metadata.initialBPM * this.tempoScale, maxBPM), minBPM));

    if (this.tempoScale === 1 || targetBPM === measuredBPM) return;

    console.log('Injecting %d BPM into libxmp. (Initial: %d)', targetBPM, this.metadata.initialBPM);
    const xmp_eventPtr = xmp._malloc(8);
    xmp._memset(xmp_eventPtr, 0, 8);
    xmp.setValue(xmp_eventPtr + 3, 0x87, 'i8');
    xmp.setValue(xmp_eventPtr + 4, targetBPM, 'i32');
    xmp._xmp_inject_event(ctx, 0, xmp_eventPtr);
  }

  getVoiceName(index) {
    return `Ch ${index + 1}`;
  }

  getNumVoices() {
    return this.metadata.numChannels;
  }

  getNumSubtunes() {
    return 0;
  }

  getPositionMs() {
    return this._positionMs;
  }

  getDurationMs() {
    return this._durationMs;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    const playingState = xmp._xmp_get_player(ctx, XMP_PLAYER_STATE);

    return !this.isPaused() && playingState === XMP_STATE_PLAYING;
  }

  seekMs(seekMs) {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    xmp._xmp_seek_time(ctx, seekMs);
  }

  stop() {
    const xmp = this.libxmp;
    const ctx = this.xmpCtx;
    this.paused = true;
    xmp._xmp_stop_module(ctx);
    this.disconnect();
    this.onPlayerStateUpdate(true);
  }
}