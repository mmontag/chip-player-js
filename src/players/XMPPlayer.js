import Player from "./Player.js";
import autoBind from 'auto-bind';

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

// noinspection PointlessArithmeticExpressionJS
export default class XMPPlayer extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    this.name = 'XMP Player';
    this.xmpCtx = this.core._xmp_create_context();
    this.infoPtr = this.core._malloc(2048);
    this.fileExtensions = fileExtensions;
    this.lastBPM = 125;
    this.tempoScale = this.lastTempoScale = 1; // TODO: rename to speed
    this._positionMs = 0;
    this._durationMs = 1000;
    this.buffer = this.core._malloc(this.bufferSize * 16); // i16
    this.infoTexts = [];
  }

  processAudioInner(channels) {
    let i, ch, err;
    const infoPtr = this.infoPtr;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    err = this.core._xmp_play_buffer(this.xmpCtx, this.buffer, this.bufferSize * 4, 1);
    if (err === -1) {
      this.stop();
    } else if (err !== 0) {
      this.suspend();
      console.error("xmp_play_buffer failed. error code: %d", err);
      throw Error('xmp_play_buffer failed');
    }

    // Get current module BPM
    // see http://xmp.sourceforge.net/libxmp.html#id25
    this.core._xmp_get_frame_info(this.xmpCtx, infoPtr);
    const bpm = this.core.getValue(infoPtr + 6 * 4, 'i32');
    this._positionMs = this.core.getValue(infoPtr + 7 * 4, 'i32'); // xmp_frame_info.time
    this._maybeInjectTempo(bpm);

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // the sample values are signed 16-bit integers
        ) / INT16_MAX;            // convert int16 to float
      }
    }
  }

  _parseMetadata() {
    const meta = {};
    const infoText = [];
    const xmp = this.core;
    const infoPtr = this.infoPtr;

    // Match layout of xmp_module_info struct
    // http://xmp.sourceforge.net/libxmp.html
    // #void-xmp-get-module-info-xmp-context-c-struct-xmp-module-info-info
    xmp._xmp_get_module_info(this.xmpCtx, infoPtr);
    const xmp_modulePtr = xmp.getValue(infoPtr + 20, '*');
    meta.title = xmp.UTF8ToString(xmp_modulePtr, 256);
    meta.system = xmp.UTF8ToString(xmp_modulePtr + 64, 256);
    meta.comment = xmp.UTF8ToString(xmp.getValue(infoPtr + 24, '*'), 512);
    if (meta.comment) infoText.push('Comment:', meta.comment);

    xmp._xmp_get_frame_info(this.xmpCtx, infoPtr);
    this._durationMs = xmp.getValue(infoPtr + 8 * 4, 'i32');

    // XMP-specific metadata
    meta.patterns =        xmp.getValue(xmp_modulePtr + 128 + 4 * 0, 'i32'); // patterns
    meta.tracks =          xmp.getValue(xmp_modulePtr + 128 + 4 * 1, 'i32'); // tracks
    meta.numChannels =     xmp.getValue(xmp_modulePtr + 128 + 4 * 2, 'i32'); // tracks per pattern
    meta.numInstruments =  xmp.getValue(xmp_modulePtr + 128 + 4 * 3, 'i32'); // instruments
    meta.numSamples =      xmp.getValue(xmp_modulePtr + 128 + 4 * 4, 'i32'); // samples
    meta.initialSpeed =    xmp.getValue(xmp_modulePtr + 128 + 4 * 5, 'i32'); // initial speed
    meta.initialBPM =      xmp.getValue(xmp_modulePtr + 128 + 4 * 6, 'i32'); // initial bpm
    meta.moduleLength =    xmp.getValue(xmp_modulePtr + 128 + 4 * 7, 'i32'); // module length
    meta.restartPosition = xmp.getValue(xmp_modulePtr + 128 + 4 * 8, 'i32'); // restart position
    const xmp_instPtr =    xmp.getValue(xmp_modulePtr + 128 + 4 * 12, 'i32');

    // xmp_envelope struct =       7 * 4 + 128 =                                    156 bytes
    // xmp_instrument map struct = 2 * 122 (rounded up from 121) =                  244 bytes
    // xmp_instrument struct =     32 + 4 + 4 + 4 + 156 + 156 + 156 + 244 + 4 + 4 = 764 bytes
    const instStructSize = 764;
    const instStrings = [];
    for (let i = 0; i < meta.numInstruments; i++) {
      const ptr = xmp_instPtr + i * instStructSize;
      instStrings.push(xmp.UTF8ToString(ptr));
    }
    const instText = instStrings.join('\n');
    if (instText.trim()) infoText.push('Instruments:', instText);

    // Filename fallback
    if (!meta.title) meta.title = this.filepathMeta.title;

    this.lastBPM = meta.initialBPM;
    this.metadata = meta;
    this.infoTexts = infoText.length ? [ infoText.join('\n\n') ] : [];
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    err = this.core.ccall(
      'xmp_load_module_from_memory', 'number',
      ['number', 'array', 'number'],
      [this.xmpCtx, data, data.length]
    );
    if (err !== 0) {
      console.error("xmp_load_module_from_memory failed. error code: %d", err);
      throw Error('xmp_load_module_from_memory failed');
    }

    err = this.core._xmp_start_player(this.xmpCtx, this.sampleRate, 0);
    if (err !== 0) {
      console.error('xmp_start_player failed. error code: %d', err);
      throw Error('xmp_start_player failed');
    }

    this._parseMetadata(filename);

    this.resume();
    this.emit('playerStateUpdate', {
      ...this.getBasePlayerState(),
      isStopped: false
    });
  }

  getVoiceMask() {
    const voiceMask = [];
    for (let i = 0; i < this.metadata.numChannels; i++) {
      voiceMask.push(!this.core._xmp_channel_mute(this.xmpCtx, i, -1));
    }
    return voiceMask;
  }

  setVoiceMask(voiceMask) {
    voiceMask.forEach((isEnabled, i) => {
      this.core._xmp_channel_mute(this.xmpCtx, i, isEnabled ? 0 : 1);
    });
  }

  getTempo() {
    return this.tempoScale;
  }

  setTempo(val) {
    if (this.metadata && !this.metadata.initialSpeed) {
      console.log('Unable to set speed for %s.', this.filepathMeta.title);
      return;
    }
    this.tempoScale = val;
  }

  _maybeInjectTempo(measuredBPM) {
    const xmp = this.core;
    const minBPM = 20;
    const maxBPM = 255;
    const estimatedBPM = Math.floor(Math.max(Math.min(this.lastBPM * this.tempoScale, maxBPM), minBPM));

    if (estimatedBPM === measuredBPM) return;

    let targetBPM = this.metadata.initialBPM;
    if (this.lastTempoScale === this.tempoScale) {  // tempo event received
      this.lastBPM = measuredBPM;
      if (this.tempoScale === 1) return;
      targetBPM = Math.floor(Math.max(Math.min(measuredBPM * this.tempoScale, maxBPM), minBPM));
    } else {                                        // `Speed` slider changed
      targetBPM = estimatedBPM;
      this.lastTempoScale = this.tempoScale;
    }

    console.log('Injecting %d BPM into libxmp. (Initial: %d)', targetBPM, this.metadata.initialBPM);
    const xmp_eventPtr = xmp._malloc(8);
    for (let i = 0; i < 8; i++) xmp.setValue(xmp_eventPtr + i, 0, 'i8');
    xmp.setValue(xmp_eventPtr + 3, 0x87, 'i8');
    xmp.setValue(xmp_eventPtr + 4, targetBPM, 'i32');
    xmp._xmp_inject_event(this.xmpCtx, 0, xmp_eventPtr);
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
    const playingState = this.core._xmp_get_player(this.xmpCtx, XMP_PLAYER_STATE);
    return !this.isPaused() && playingState === XMP_STATE_PLAYING;
  }

  seekMs(seekMs) {
    this.core._xmp_seek_time(this.xmpCtx, seekMs);
  }

  stop() {
    this.suspend();
    this.core._xmp_stop_module(this.xmpCtx);
    console.debug('XMPPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
