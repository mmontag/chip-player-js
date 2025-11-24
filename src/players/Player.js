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
  }

  /**
   * Copies data to the Emscripten heap.
   * Useful for loading files.
   *
   * @param {ArrayBuffer} data
   * @return {number} - A pointer to the allocated memory.
   */
  copyToHeap(data) {
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

  seekMs(ms) {
    console.debug(`Player.seekMs() not implemented for ${this.constructor.name}.`);
  }

  getVoiceName(index) {
    console.debug(`Player.getVoiceName() not implemented for ${this.constructor.name}.`);
  }

  getVoiceMask() {
    console.debug(`Player.getVoiceMask() not implemented for ${this.constructor.name}.`);
    return [];
  }

  setVoiceMask() {
    console.debug(`Player.setVoiceMask() not implemented for ${this.constructor.name}.`);
  }

  getNumVoices() {
    console.debug(`Player.getNumVoices() not implemented for ${this.constructor.name}.`);
    return 0;
  }

  getVoiceNames() {
    const names = [];
    for (let i = 0; i < this.getNumVoices(); i++) {
      names.push(this.getVoiceName(i));
    }
    return names;
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
      if (this.getParameter(paramId) === resolvedValue) continue;
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
    return {
      metadata: this.getMetadata(),
      durationMs: this.getDurationMs(),
      positionMs: this.getPositionMs(),
      numVoices: this.getNumVoices(),
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

  suspend() {
    this.stopped = true;
    this.paused = true;
  }

  processAudio(output) {
    const start = performance.now();
    this.processAudioInner(output);
    const end = performance.now();

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
