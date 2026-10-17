import EventEmitter from 'events';
//
// Player can be viewed as a state machine with
// 3 states (playing, paused, stopped) and 5 transitions
// (open, pause, unpause, seek, stop).
// If the player has reached end of the song,
// it is in the terminal "stopped" state and is
// no longer playable:
//                         ╭– (seek) –╮
//                         ^          v
//  ┌─ ─ ─ ─ ─┐         ┌─────────────────┐            ┌─────────┐
//  │ stopped │–(open)–>│     playing     │––(stop)–––>│ stopped │
//  └ ─ ─ ─ ─ ┘         └─────────────────┘            └─────────┘
//                        | ┌────────┐ ^
//                (pause) ╰>│ paused │–╯ (unpause)
//                          └────────┘
//
// "stopped" is synonymous with closed/empty.
//
export default class Player extends EventEmitter {
  /**
   * @param {object} core - Emscripten module
   * @param {number} sampleRate - Audio sample rate
   * @param {number} [bufferSize=2048] - Audio buffer size
   * @param {boolean} [debug=false] - Enable debug logging
   */
  constructor(core, sampleRate, bufferSize=2048, debug=false) {
    super();

    this.playerKey = null; // Should be overridden by subclasses
    this.core = core;
    this.paused = true;
    this.stopped = true;
    this.fileExtensions = [];
    this.metadata = null;
    this.sampleRate = sampleRate;
    this.bufferSize = bufferSize;
    this.debug = debug;
    this.timeCount = 0;
    this.renderTime = 0;
    this.perfLoggingInterval = 100;
    this.paramDefs = [];
    this.params = {};
    this.infoTexts = [];
    this.looping = false; // infinite looping mode (vs. normal mode where it stops at end of song)
    this.silenceDuration = -1;
    this.silenceSamplesRemaining = 0;
    this.onSilenceEnd = null;
    this.trailingSilenceSamples = 0;
    this.durationMs = 0;
    // A seek asked for while an async load is still starting (SID waits for
    // HVSC metadata, VGM for an OPL4 ROM). It is held until the first rendered
    // block: some cores block inside a seek until their own clock advances and
    // there is no clock yet, and a load that finishes afterwards re-selects the
    // sub-tune and resets the position, which would discard it. App's ?t= timer
    // fires ~100ms after starting a song, inside this window when the fetch is
    // slow -- the case that used to lose the link's position.
    this.loadingAsync = false;
    this.deferredSeekMs = null;
  }

  /**
   * Copies data to the Emscripten heap.
   * Useful for loading files.
   *
   * @param {ArrayBuffer} data
   * @return {number} - A pointer to the allocated memory.
   */
  copyToHeap(data) {
    // TODO: the JSDoc promises an ArrayBuffer, but HEAPU8.set() only accepts a
    // TypedArray/array-like, so an ArrayBuffer silently copies nothing. Callers
    // pass a Uint8Array today; normalize `data` here.
    const dataPtr = this.core._malloc(data.byteLength);
    this.core.HEAPU8.set(data, dataPtr);
    return dataPtr;
  }

  togglePause() {
    this.paused = !this.paused;
    return this.paused;
  }

  isPaused() {
    return this.paused;
  }

  resume() {
    this.stopped = false;
    this.paused = false;
  }

  canPlay(fileExtension) {
    return this.fileExtensions.indexOf(fileExtension.toLowerCase()) !== -1;
  }

  /**
   * @param {ArrayBuffer} data - File contents
   * @param {string} filename - Filename for metadata fallback
   */
  loadData(data, filename) {
    throw Error('Player.loadData() must be implemented.');
  }

  stop() {
    throw Error('Player.stop() must be implemented.');
  }

  isPlaying() {
    throw Error('Player.isPlaying() must be implemented.');
  }

  getTempo() { // TODO: rename all tempo to speed
    console.debug(`Player.getTempo() not implemented for ${this.constructor.name}.`);
    return 1;
  }

  setTempo() {
    console.debug(`Player.setTempo() not implemented for ${this.constructor.name}.`);
  }

  setFadeout(startMs) {
    console.debug(`Player.setFadeout() not implemented for ${this.constructor.name}.`);
  }

  getDurationMs() {
    console.debug(`Player.getDurationMs() not implemented for ${this.constructor.name}.`);
    return 5000;
  }

  getPositionMs() {
    console.debug(`Player.getPositionMs() not implemented for ${this.constructor.name}.`);
    return 0;
  }

  // Position for the slider head and time label. getPositionMs() is the
  // absolute "time playing" (it keeps counting across loops); players that can
  // loop a region override this to fold the loop phase into the displayed
  // "playlist" position so the head repeats the region instead of running on.
  getDisplayPositionMs() {
    return this.getPositionMs();
  }

  seekMs(ms) {
    this.silenceSamplesRemaining = 0;
    this.onSilenceEnd = null;
    this.trailingSilenceSamples = 0;
    console.debug(`Player.seekMs() not implemented for ${this.constructor.name}.`);
  }

  // Deprecated in favor of getVoiceGroups
  getVoiceName(index) {
    return 'Voice ' + (index + 1);
  }

  // Deprecated in favor of getVoiceGroups
  getVoiceNames() {
    const names = [];
    for (let i = 0; i < this.getVoiceMask().length; i++) {
      names.push(this.getVoiceName(i));
    }
    return names;
  }

  getVoiceMask() {
    console.debug(`Player.getVoiceMask() not implemented for ${this.constructor.name}.`);
    return [];
  }

  setVoiceMask() {
    console.debug(`Player.setVoiceMask() not implemented for ${this.constructor.name}.`);
  }

  /*
  [
    {
      name: 'Sega PSG',
      voices: [
        {
          idx: 0,
          name: 'Pulse 1',
        },
        {
          idx: 1,
          name: 'Pulse 2'
        },
      ],
    },
    {
      name: 'YM2612',
      voices: [
        {
          idx: 2,
          name: 'FM 1',
        },
        {
          idx: 3,
          name: 'FM 2'
        },
      ],
    },
  ]
   */
  // a nice replacement for getNumVoices and getVoiceName.
  getVoiceGroups() {
    return [];
  }

  getNumSubtunes() {
    return 1;
  }

  getSubtune() {
    return 0;
  }

  // Sub-tune selection is handled by the sequencer (one SongRef per sub-tune),
  // not by the player.
  playSubtune() {}

  getMetadata() {
    return {
      title: null,
      author: null,
    }
  }

  getInfoTexts() {
    return this.infoTexts;
  }

  getParamDefs() {
    return this.paramDefs;
  }

  getParamDefault(paramId) {
    const paramDef = this.paramDefs.find(p => p.id === paramId);
    return paramDef?.defaultValue;
  }

  getParameter(paramId) {
    return this.params[paramId];
  }

  setParameter(paramId, value) {
    this.params[paramId] = value;
  }

  resolveParamValue(paramId, transientValue, persistedSettings) {
    // Priority 1: Transient value discovered by the player during load.
    if (transientValue !== undefined && transientValue !== null) {
      return transientValue;
    }

    // Priority 2: User's persisted ("pinned") setting.
    const persistedKey = `${this.playerKey}.${paramId}`;
    if (persistedSettings?.hasOwnProperty(persistedKey)) {
      return persistedSettings[persistedKey];
    }

    // Priority 3: Player's hard-coded default.
    return this.getParamDefault(paramId);
  }

  resolveParamValues(persistedSettings) {
    for (const paramDef of this.paramDefs) {
      const paramId = paramDef.id;
      const resolvedValue = this.resolveParamValue(paramId, undefined, persistedSettings);
      // Reloading the soundfont is expensive.
      if (paramId === 'soundfont' && this.getParameter(paramId) === resolvedValue) continue;
      if (resolvedValue !== undefined) {
        this.setParameter(paramId, resolvedValue);
      }
    }
  }

  getParamValues() {
    const paramValues = {};
    if (this.getParameter) {
      for (const def of this.paramDefs) {
        paramValues[def.id] = this.getParameter(def.id);
      }
    }
    return paramValues;
  }

  getBasePlayerState() {
    this.durationMs = this.getDurationMs() || 0;
    return {
      metadata: this.getMetadata(),
      durationMs: this.durationMs,
      positionMs: this.getPositionMs(),
      numSubtunes: this.getNumSubtunes(),
      subtune: this.getSubtune(),
      paramDefs: this.getParamDefs(),
      paramValues: this.getParamValues(),
      tempo: this.getTempo(),
      voiceMask: this.getVoiceMask(),
      voiceNames: this.getVoiceNames(),
      voiceGroups: this.getVoiceGroups(),
      infoTexts: this.getInfoTexts(),
      isStopped: this.stopped,
      isPaused: this.paused,
    };
  }

  setLooping(looping) {
    this.looping = looping;
  }

  // End of the loop region in ms, or null when the track defines no loop.
  getLoopEndMs() {
    const meta = this.metadata;
    return meta && meta.loop_length > 0 && meta.intro_length >= 0
      ? meta.intro_length + meta.loop_length
      : null;
  }

  // Highlight band for the time slider, in ms, or null when the track defines
  // no loop region. The band is the LAST loop instance before the fade
  // (default playback is intro + two passes + fade): that gives the user as
  // long as possible to decide to stay, and toggling repeat never shifts the
  // band. Reads the shared intro_length/loop_length vocabulary; engines whose
  // loop semantics differ override this.
  getLoopBandMs() {
    const meta = this.metadata;
    if (!meta || !Number.isFinite(meta.intro_length) || meta.intro_length < 0 ||
        !Number.isFinite(meta.loop_length) || meta.loop_length <= 0)
      return null;
    const startMs = meta.intro_length + meta.loop_length;
    let endMs = meta.intro_length + 2 * meta.loop_length;
    const durationMs = this.getDurationMs();
    if (durationMs > 0) {
      // A song can be shorter than its region claims: tracks that run out of
      // loops before the second pass completes leave the band reaching past the
      // end of the song. Clamping there left a sliver the song never plays,
      // drawn while it was already ending. No band is the honest answer, and the
      // blind-loop UI covers it. The second of slack absorbs the whole-second
      // length.
      if (endMs > durationMs + 1000) return null;
      endMs = Math.min(endMs, durationMs);
    }
    return endMs > startMs ? { startMs, endMs } : null;
  }

  // True when playback legitimately runs past durationMs and the engine (not
  // the base end detector) owns the end. The base returns Repeat One only;
  // subclasses OR in their own indefinite-playback setting and any extended
  // tail left over from leaving a deep repeat.
  isPlayingIndefinitely() {
    return this.looping;
  }

  // True when the fallback loop UI applies: the song runs past its length with
  // no known loop region, so no honest position exists -- the transport parks
  // the head while the elapsed time climbs. Engines land here by free-running
  // past a listed length (an NSF driver looping internally; SID/N64 under
  // indefinite playback); a bounded clock -- a loop region, a native loop
  // (XMP), a JS wrap (MIDI with a region), or a stop + reload (V2M) -- never
  // does. The past-the-end term scopes it.
  isBlindLoop() {
    if (!this.isPlayingIndefinitely()) return false;
    if (this.getLoopBandMs()) return false;
    const duration = this.getDurationMs();
    if (!(duration > 0)) return false;
    // Not merely "not playing": a paused song is still in its indefinite loop,
    // and only the transport is held. Pausing must keep the Loop label and the
    // climbing elapsed time, so the term is for a player that really stopped
    // (the engine reported the end) -- never for the paused state, which every
    // player reports as not playing.
    if (!this.isPlaying() && !this.isPaused()) return false;
    return this.getPositionMs() >= duration;
  }

  setSilenceDuration(seconds) {
    const val = Number(seconds);
    this.silenceDuration = isNaN(val) ? -1 : val;
  }

  handleSongEnd(onSilenceEnd = null) {
    // A player plays exactly one song and then stops. Sub-tune navigation is
    // the sequencer's job: each sub-tune is its own context entry.
    const endAction = onSilenceEnd || (() => this.stop());

    if (this.silenceDuration >= 0) {
      const trailingSilenceSec = this.trailingSilenceSamples / this.sampleRate;
      const neededSilenceSec = Math.max(0, this.silenceDuration - trailingSilenceSec);
      this.silenceSamplesRemaining = Math.round(neededSilenceSec * this.sampleRate);
      this.onSilenceEnd = endAction;
      this.trailingSilenceSamples = 0;

      if (this.silenceSamplesRemaining <= 0) {
        this.silenceSamplesRemaining = 0;
        this.onSilenceEnd = null;
        endAction();
      }
    } else {
      this.trailingSilenceSamples = 0;
      endAction();
    }
  }

  suspend() {
    this.stopped = true;
    this.paused = true;
    this.silenceSamplesRemaining = 0;
    this.onSilenceEnd = null;
    this.trailingSilenceSamples = 0;
    // A suspended player is not loading anything any more, and a seek it was
    // still holding was asked for by the load that just got cancelled -- it
    // must not survive into the next song.
    this.loadingAsync = false;
    this.deferredSeekMs = null;
  }

  // Called at the top of a load that waits on something before playback starts
  // (SID's HVSC metadata, VGM's OPL4 ROM). Any seek still held from an earlier
  // load is dropped here: it belongs to the song it was asked for, and carrying
  // it into this one would skip the new song to the old link's time.
  beginAsyncLoad() {
    this.loadingAsync = true;
    this.deferredSeekMs = null;
  }

  // Holds a seek issued while an async load is still starting (see the fields
  // in the constructor). True means the caller must not touch the engine yet.
  deferSeekUntilPlaying(ms) {
    if (!this.loadingAsync) return false;
    this.deferredSeekMs = ms;
    return true;
  }

  // Runs after each rendered block: the load's first render means the engine's
  // clock is advancing, so a held seek is safe to hand to the platform now --
  // and it lands after whatever the load did to the transport, not before it.
  flushDeferredSeek() {
    if (!this.loadingAsync) return;
    this.loadingAsync = false;
    const ms = this.deferredSeekMs;
    this.deferredSeekMs = null;
    if (ms != null) this.seekMs(ms);
  }

  processAudio(output) {
    if (this.silenceSamplesRemaining > 0) {
      for (let ch = 0; ch < output.length; ch++) {
        output[ch].fill(0);
      }
      if (!this.paused) {
        this.silenceSamplesRemaining -= this.bufferSize;
        if (this.silenceSamplesRemaining <= 0) {
          this.silenceSamplesRemaining = 0;
          const cb = this.onSilenceEnd;
          this.onSilenceEnd = null;
          if (cb) {
            cb();
          } else {
            this.stop();
          }
        }
      }
      return;
    }

    const start = performance.now();
    this.processAudioInner(output);
    this.flushDeferredSeek();
    const end = performance.now();

    if (this.silenceSamplesRemaining > 0) {
      for (let ch = 0; ch < output.length; ch++) {
        output[ch].fill(0);
      }
    } else if (this.silenceDuration >= 0 && !this.paused && !this.isPlayingIndefinitely() && !this.fadingOut && output.length > 0) {
      const duration = this.durationMs;
      const position = this.getPositionMs();

      if (duration > 0) {
        const inFinalFiveSeconds = position >= (duration - 5000);
        if (inFinalFiveSeconds) {
          const threshold = 0.001; // -60 dB
          let isSilent = true;
          const left = output[0];
          const right = output[1] || output[0];
          const len = left.length;
          for (let i = 0; i < len; i++) {
            if (Math.abs(left[i]) > threshold || Math.abs(right[i]) > threshold) {
              isSilent = false;
              break;
            }
          }
          this.trailingSilenceSamples = isSilent ? (this.trailingSilenceSamples + this.bufferSize) : 0;
        } else {
          this.trailingSilenceSamples = 0;
        }

        const reachedEarlySilence = inFinalFiveSeconds && this.trailingSilenceSamples >= (0.4 * this.sampleRate);
        const reachedDuration = position >= duration;

        if (reachedEarlySilence || reachedDuration) {
          this.handleSongEnd();
          for (let ch = 0; ch < output.length; ch++) {
            output[ch].fill(0);
          }
          return;
        }
      }
    }

    if (this.debug) {
      this.renderTime += end - start;
      this.timeCount++;
      if (this.timeCount >= this.perfLoggingInterval) {
        const cost = this.renderTime / this.timeCount;
        const budget = 1000 * this.bufferSize / this.sampleRate;
        console.log(
          '[%s] %s ms to render %d frames (%s ms) (%s% utilization)',
          this.constructor.name,
          cost.toFixed(2),
          this.bufferSize,
          budget.toFixed(1),
          (100 * cost / budget).toFixed(1),
        );
        this.renderTime = 0.0;
        this.timeCount = 0;
      }
    }
  }

  processAudioInner() {
    throw Error('Player.processAudioInner() must be implemented.');
  }

  async muteAudioDuringCall(audioNode, fn) {
    // Workaround to eliminate stuttering.
    if (audioNode.context.state === 'running') {
      console.debug('Suspending audio context during expensive operation...');
      await audioNode.context.suspend();
      fn();
      await audioNode.context.resume();
    } else {
      fn();
    }
  }

  handleFileSystemReady() {}

  static metadataFromFilepath(filepath) {
    // Guess metadata from path/filename for MIDI files.
    // Assumes structure:  /Game MIDI/{game}/**/{title}
    //             ...or:  /MIDI/{artist}/**/{title}
    const parts = filepath.split('/');
    const meta = {};
    meta.title = parts.pop();
    meta.system = parts.shift();
    if (meta.system === 'Game MIDI') {
      meta.game = parts.shift();
    } else {
      meta.artist = parts.shift();
    }
    return meta;
  }
}

/**
 * Polyfill requestIdleCallback, which is used by doIncrementalSeek.
 * Still not supported in Safari as of March 2025.
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
