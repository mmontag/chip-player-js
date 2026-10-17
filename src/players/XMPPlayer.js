import Player from "./Player.js";
import autoBind from 'auto-bind';

const INT16_MAX = Math.pow(2, 16) - 1;
const XMP_PLAYER_STATE = 8;
const XMP_STATE_PLAYING = 2;
// How far a seek's landing may sit from the requested time and still count as
// "landed where it was asked" rather than "the engine moved us to a boundary".
// Above one buffer of read jitter, far below an order.
const SEEK_SNAP_MS = 250;
const fileExtensions = [
  // libxmp-lite:
  'it',  //  Impulse Tracker  1.00, 2.00, 2.14, 2.15
  'mod', //  Sound/Noise/Protracker M.K., M!K!, M&K!, N.T., CD81
  's3m', //  Scream Tracker 3 3.00, 3.01+
  'xm',  //  Fast Tracker II  1.02, 1.03, 1.04
];

// Defines copied from xmp.h
const XMP_PLAYER_INTERP	= 2;
const XMP_INTERP_NEAREST = 0;
const XMP_INTERP_LINEAR	= 1;
const XMP_INTERP_SPLINE	= 2;
const XMP_INFO_TIME = 7;
const XMP_INFO_ROW = 2;

// noinspection PointlessArithmeticExpressionJS
export default class XMPPlayer extends Player {
  paramDefs = [
    {
      id: 'interpolation',
      label: 'Interpolation',
      type: 'enum',
      options: [{
        label: 'Interpolation Mode',
        items: [
          { label: 'Nearest-neighbor (None)', value: XMP_INTERP_NEAREST },
          { label: 'Linear', value: XMP_INTERP_LINEAR },
          { label: 'Spline (Cubic)', value: XMP_INTERP_SPLINE },
        ],
      }],
      defaultValue: XMP_INTERP_LINEAR,
    },
  ];

  constructor(...args) {
    super(...args);
    autoBind(this);

    this.playerKey = 'xmp';
    this.name = 'XMP Player';
    this.xmpCtx = this.core._xmp_create_context();
    this.infoPtr = this.core._malloc(2048);
    this.fileExtensions = fileExtensions;
    this.tempoScale = 1; // TODO: rename to speed
    this._positionMs = 0;
    this._durationMs = 1000;
    // Loop passes to hand libxmp for the current state (xmp_play_buffer's
    // loop count; 0 = forever). 1 ends at the scan end, like before.
    this._loopCount = 1;
    // Learned loop band: position -> first-visit frame time. libxmp exposes no
    // loop start, but the first backward order jump (the engine looping)
    // resolves it exactly, tempo included. First visits are absolute, so seeks
    // can't corrupt them; a backward jump onto an order the linear flow never
    // visited has an unknowable start, so it is skipped -- a loop-end time must
    // never become a loop start.
    //
    // Tracked at two granularities, and the band start comes from the row map:
    // a loop can re-enter an order mid-pattern (libxmp's "Bxx+Dxx within same
    // position" line jump applies the pattern break's row), so the order's
    // row-0 time would draw a band over music the loop never revisits. The row
    // key is the entry row's own first-pass time; when that row was never
    // played (a pattern break can skip it), the band is left unset rather than
    // guessed. The order map still answers "was this order visited" for the
    // out-of-flow and seek-landing rules.
    this._orderFirstSeen = new Map();
    this._rowFirstSeen = new Map();
    this._lastPos = -1;
    this._skipOrder = null;
    this._seekPending = false;
    this._seekRequestedMs = 0;
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

    err = this.core._xmp_play_buffer(this.xmpCtx, this.buffer, this.bufferSize * 4, this._loopCount);
    if (err === -1) {
      this.handleSongEnd();
      return;
    } else if (err !== 0) {
      this.suspend();
      console.error("xmp_play_buffer failed. error code: %d", err);
      throw Error('xmp_play_buffer failed');
    }

    // Get current module BPM
    // see http://xmp.sourceforge.net/libxmp.html#id25
    this.core._xmp_get_frame_info(this.xmpCtx, infoPtr);
    this._positionMs = this.core.getValue(infoPtr + XMP_INFO_TIME * 4, 'i32'); // xmp_frame_info.time
    const pos = this.core.getValue(infoPtr, 'i32'); // xmp_frame_info.pos
    const row = this.core.getValue(infoPtr + XMP_INFO_ROW * 4, 'i32'); // xmp_frame_info.row
    this.learnLoopFromOrder(pos, row);
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
    meta.comment = xmp.UTF8ToString(xmp.getValue(infoPtr + 24, '*'), 2048);
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

    this.metadata = meta;
    this.infoTexts = infoText.length ? [ infoText.join('\n\n') ] : [];
  }

  loadData(data, filename, persistedSettings) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const dataPtr = this.copyToHeap(data);
    err = this.core._xmp_load_module_from_memory(this.xmpCtx, dataPtr, data.length);
    this.core._free(dataPtr);

    if (err !== 0) {
      console.error('xmp_load_module_from_memory failed. error code: %d', err);
      throw Error('xmp_load_module_from_memory failed');
    }

    err = this.core._xmp_start_player(this.xmpCtx, this.sampleRate, 0);
    if (err !== 0) {
      console.error('xmp_start_player failed. error code: %d', err);
      throw Error('xmp_start_player failed');
    }

    this._parseMetadata(filename);

    // Fresh timeline: order first-visits (for the learned loop band) start over,
    // and the loop count is re-derived from the current repeat state so a count
    // preserved from the previous module (see setLooping) cannot leak into this
    // one.
    this._loopCount = this.looping ? 0 : 1;
    this._orderFirstSeen = new Map();
    this._rowFirstSeen = new Map();
    this._lastPos = -1;
    this._skipOrder = null;
    this._seekPending = false;
    this._seekRequestedMs = 0;

    this.resolveParamValues(persistedSettings);
    this.setTempo(persistedSettings.tempo || 1);
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
    if (!this.xmpCtx) return;
    this.core._xmp_set_tempo_factor(this.xmpCtx, 1 / val); // Expects inverse value.
    this.tempoScale = val;
  }

  getVoiceName(index) {
    return `Ch ${index + 1}`;
  }

  getPositionMs() {
    return this._positionMs;
  }

  // Record each order's first visit; the first backward order jump is the
  // engine looping, and its target's first-visit time is the loop start.
  // The band is the repeating span [loopStart, trackEnd), shown from the
  // first loop end on. Files without a backward jump never learn one and
  // keep today's UI.
  learnLoopFromOrder(pos, row) {
    const seen = this._orderFirstSeen;
    if (!seen) {
      this._lastPos = pos;
      return;
    }
    // What the engine did with the seek decides whether the landing order can be
    // timed. Landing where it was asked means landing inside an order, whose
    // start is behind the landing point -- leave it unseen and let a later
    // backward jump onto it take the out-of-flow path below, like any other
    // arrival from outside the linear flow. But a landing the engine *moved* is
    // one it chose: libxmp's xmp_seek_time snaps to an order boundary, so that
    // point is a real start and can be recorded. The tolerance is well above the
    // read jitter (one buffer) and well below an order, so a frame-accurate seek
    // can never invent a band and a short snap just stays unlearned. Orders
    // visited before the seek keep their real first-visit times either way.
    if (this._seekPending) {
      this._seekPending = false;
      const snapped = Math.abs(this._positionMs - this._seekRequestedMs) > SEEK_SNAP_MS;
      if (!snapped && !seen.has(pos)) {
        // The landing order can't be timed, and neither can a poll later in the
        // same order: the guard has to outlive the poll that saw the seek, or
        // the order's next poll records a mid-order time as its start and a
        // later backward jump onto it draws a band from that point. `_skipOrder`
        // holds until playback leaves the order, like any other arrival from
        // outside the linear flow.
        this._skipOrder = pos;
        this._lastPos = pos;
        return;
      }
    }
    if (this._skipOrder != null && pos !== this._skipOrder) this._skipOrder = null;
    if (this._skipOrder == null) {
      // A backward order jump is the loop; its arrival is never a first visit,
      // so nothing is recorded for it -- the band start must come from the
      // linear pass, or a loop-end time would become a loop start.
      const backward = pos < this._lastPos;
      if (!backward) {
        // Order 0's first visit is the song's start; _positionMs has already
        // advanced by a buffer, so record 0 rather than a small nonzero intro
        // (which would pass the loopStartMs > 0 test and draw a false band for
        // a track that loops from its end back to the start).
        const visitTime = pos === 0 && seen.size === 0 ? 0 : this._positionMs;
        // Row first visits: a Bxx+Dxx line jump re-enters an order mid-pattern,
        // so the band start must be that row's time, not the order's row-0 time
        // (see the constructor comment). Recorded for every new (pos,row), not
        // just an order's first row, since the loop target row is usually a
        // later one.
        const rowKey = pos + ':' + row;
        if (!this._rowFirstSeen.has(rowKey)) this._rowFirstSeen.set(rowKey, visitTime);
        if (!seen.has(pos)) seen.set(pos, visitTime);
      } else if (!seen.has(pos)) {
        // Backward jump onto a never-visited order: arrival from outside the
        // linear flow, true start unknown -- skip recording this visit (and the
        // ones that follow it) so the loop-end time can't become the loop start.
        // Learning simply never fires here.
        this._skipOrder = pos;
      } else if (this.metadata && this.metadata.intro_length == null) {
        const loopStartMs = this._rowFirstSeen.get(pos + ':' + row);
        // A loop from the very start spans the whole track, so highlighting
        // it says nothing -- the repeat indicator already covers that case.
        // An unrecorded row (the first pass never played it) is likewise left
        // unset: a start guessed at the order's row 0 would cover music the
        // loop never revisits.
        if (loopStartMs != null && loopStartMs > 0 && loopStartMs < this._durationMs) {
          this.metadata.intro_length = loopStartMs;
          this.metadata.loop_length = this._durationMs - loopStartMs;
          // The band appears mid-song, after the footer already rendered
          // without one -- tell the app to re-render it. Once per song.
          this.emit('playerStateUpdate', {
            ...this.getBasePlayerState(),
            isStopped: false,
          });
        }
      }
    }
    this._lastPos = pos;
  }

  // Repeat One loops natively: libxmp's own play-buffer loop count (0 =
  // forever) keeps the engine replaying its loop seamlessly instead of
  // stopping at the scan end. The engine clock is positional, so it wraps
  // back into the loop on its own (no head fold or blind-loop parking
  // needed); the base display stays truthful throughout.
  setLooping(looping) {
    const wasLooping = this.looping;
    super.setLooping(looping);
    if (this.looping) {
      this._loopCount = 0;
    } else if (wasLooping) {
      // Finish the in-progress pass, then stop at its end (like VGMPlayer's
      // applyLoopCount). Only re-derive when actually leaving a looping
      // state; a no-op toggle must not move the scheduled end.
      this._loopCount = Math.max(1, this.getCurLoop() + 1);
    }
    // A false-to-false toggle (Repeat One -> Repeat Off -> Repeat All) leaves the
    // scheduled pass count alone. Re-deriving it to 1 here would reset the pass
    // the user already asked to finish, and the engine, already past that count,
    // would stop early. loadData() resets the count for a new module.
  }

  // Which scan-end pass the engine is in (0 during the intro/first pass).
  getCurLoop() {
    if (!this.xmpCtx) return 0;
    this.core._xmp_get_frame_info(this.xmpCtx, this.infoPtr);
    return this.core.getValue(this.infoPtr + 14 * 4, 'i32') || 0; // loop_count
  }

  // The repeating span itself, learned at the first backward order jump.
  // Unlike the base two-pass band, this marks the loop content: XMP keeps
  // the single-pass duration and the positional clock revisits this span on
  // every pass, so there is no "last pass" to highlight.
  getLoopBandMs() {
    const meta = this.metadata;
    if (!meta || !(meta.intro_length >= 0) || !(meta.loop_length > 0)) return null;
    const durationMs = this.getDurationMs();
    const endMs = durationMs > 0
      ? Math.min(meta.intro_length + meta.loop_length, durationMs)
      : meta.intro_length + meta.loop_length;
    return endMs > meta.intro_length ? { startMs: meta.intro_length, endMs } : null;
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
    // A seek is not a loop: forget the last position so the landing can't read
    // as a backward jump. First visits are absolute, so learning continues
    // across the seek; only a never-linearly-visited start stays unlearned.
    this._lastPos = -1;
    // What the engine does with the request decides whether the landing order
    // can be timed (see learnLoopFromOrder): remembered here, compared at the
    // next order poll.
    this._seekRequestedMs = seekMs;
    // The landing is an arrival from outside the linear flow, which the next
    // order poll has to know.
    this._seekPending = true;
    // xmp_seek_time_frame (libxmp 4.7+) is a more accurate seek; fall back to
    // xmp_seek_time on older builds (same millisecond units).
    if (this.core._xmp_seek_time_frame) {
      this.core._xmp_seek_time_frame(this.xmpCtx, seekMs);
    } else {
      this.core._xmp_seek_time(this.xmpCtx, seekMs);
    }
    // A pass target armed for "finish the pass you're in" must be re-derived
    // here: libxmp's frame-accurate seek forces an xmp_play_buffer refresh, and
    // that is exactly where it zeroes its completed-loop counter -- so a target
    // computed before the seek would be spent counting from zero, i.e. one pass
    // too many. Read the counter the seek left behind rather than the pre-seek
    // one still cached in the frame info, and leave the snapping fallback alone:
    // it does not touch the counter, so the re-derivation is a no-op there.
    if (!this.looping && this._loopCount > 0) {
      this.core._xmp_get_frame_info(this.xmpCtx, this.infoPtr);
      this._loopCount = Math.max(1, this.getCurLoop() + 1);
    }
  }

  stop() {
    this.suspend();
    this.core._xmp_stop_module(this.xmpCtx);
    console.debug('XMPPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }

  getParameter(id) {
    if (!this.xmpCtx) return null;
    switch (id) {
      case 'interpolation':
        return this.core._xmp_get_player(this.xmpCtx, XMP_PLAYER_INTERP);
      default:
        console.warn('Unknown parameter id:', id);
        return null;
    }
  }

  setParameter(id, value, isTransient=false) {
    switch (id) {
      case 'interpolation':
        this.core._xmp_set_player(this.xmpCtx, XMP_PLAYER_INTERP, value);
        break;
      default:
        console.warn('Unknown parameter id:', id);
    }
  }
}
