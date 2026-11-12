import EventEmitter from 'events';
import axios from 'redaxios';
import { getMetadataUrlForFilepath, getUrlFromFilepath } from '../util';
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
    this.pendingLoad = null;
    this.catalogMeta = {};
    this.catalogMetaPromise = null;
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
    this.hostFade = false;
    this.fading = false;
    this.fadeStartMs = null;
    this.fadeDurationMs = 5000;
    this.seekTargetMs = null;
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

  // A new load replaces any pending work from the previous song. Async player
  // operations must check the signal after each await before changing state.
  beginLoad() {
    if (this.pendingLoad) this.pendingLoad.abort();
    this.pendingLoad = new AbortController();
    this.catalogMeta = {};
    this.catalogMetaPromise = null;
    // A held seek belongs to the load that issued it. Clear it here as well as
    // in stop(): a replaced load may not have been stopped first, and the next
    // player's first block must not replay a seek from the previous song.
    this.loadingAsync = false;
    this.deferredSeekMs = null;
    this.resetFade();
    return this.pendingLoad.signal;
  }

  fetchCatalogMetadata(filepath, loadSignal = this.pendingLoad?.signal, subtune = 0) {
    if (!filepath || filepath.startsWith('local/')) {
      return Promise.resolve({});
    }
    if (this.catalogMetaPromise) {
      return this.catalogMetaPromise;
    }
    const metadataUrl = getMetadataUrlForFilepath(filepath, subtune);
    this.catalogMetaPromise = axios.get(metadataUrl, {
        fetch: (url, options) => fetch(url, { ...options, signal: loadSignal }),
      })
      .then(response => {
        if (loadSignal?.aborted) return {};
        const {
          imageUrl: imagePath, infoTexts = [], md5 = null, songId = null, soundfont = null,
          subtuneCount = null, subtuneTitle = null,
        } = response.data || {};
        this.catalogMeta = {
          imageUrl: imagePath ? getUrlFromFilepath(imagePath) : null,
          infoTexts,
          md5,
          songId,
          soundfont,
          subtuneCount,
          subtuneTitle,
        };
        if (!this.stopped) {
          this.emit('playerStateUpdate', this.getCatalogState());
        }
        return this.catalogMeta;
      })
      .catch(e => {
        if (!loadSignal?.aborted) console.warn('Could not load metadata:', e);
        return {};
      });
    return this.catalogMetaPromise;
  }

  /**
   * @param {ArrayBuffer} data - File contents
   * @param {string} filename - Filename for metadata fallback
   */
  async loadData(data, filename) {
    throw Error('Player.loadData() must be implemented.');
  }

  /**
   * Stops audio rendering and resets playback state.
   * Cancels in-flight loads and silences audio output.
   */
  stop() {
    if (this.pendingLoad) {
      this.pendingLoad.abort();
      this.pendingLoad = null;
    }
    this.catalogMeta = {};
    this.catalogMetaPromise = null;
    // An async load can leave a seek held for its first rendered block. If the
    // song is stopped or replaced before that block, the seek belongs to the
    // old load and must not be applied to the next song.
    this.loadingAsync = false;
    this.deferredSeekMs = null;
    this.stopped = true;
    this.paused = true;
    this.silenceSamplesRemaining = 0;
    this.trailingSilenceSamples = 0;
    this.seekTargetMs = null;
    this.resetFade();
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

  setFadeDuration(ms) {
    this.fadeDurationMs = Math.max(0, Number(ms) || 0);
  }

  startFade(startMs = this.getPositionMs()) {
    this.fading = true;
    this.fadeStartMs = startMs;
  }

  resetFade() {
    this.fading = false;
    this.fadeStartMs = null;
    this.trailingSilenceSamples = 0;
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
    this.trailingSilenceSamples = 0;
    this.seekTargetMs = null;
    this.resetFade();
    console.debug(`Player.seekMs() not implemented for ${this.constructor.name}.`);
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
  getVoiceGroups() {
    const mask = this.getVoiceMask();
    if (!mask || mask.length === 0) return [];
    return [{
      name: 'Voices',
      voices: mask.map((_, i) => ({
        idx: i,
        name: `Voice ${i + 1}`,
      })),
    }];
  }

  getNumSubtunes() {
    return 1;
  }

  getSubtune() {
    return 0;
  }

  // A player can select a tune in the file it has loaded, but it never walks
  // to the next tune on its own. Sub-tune sequencing belongs to the Sequencer:
  // every sub-tune is its own SongRef/context entry.
  playSubtune(subtune) {
    this.silenceSamplesRemaining = 0;
    this.trailingSilenceSamples = 0;
    this.resetFade();
  }

  getMetadata() {
    const metadata = this.metadata || { title: null, author: null };
    return {
      ...metadata,
      imageUrl: metadata.imageUrl || this.catalogMeta?.imageUrl || null,
    };
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
    if (paramId === 'indefinitePlayback') {
      if (value) {
        this.resetFade();
        this.trailingSilenceSamples = 0;
      } else if (this.hostFade && !this.looping) {
        const duration = this.durationMs || this.getDurationMs();
        const pos = this.getPositionMs();
        if (duration > 0 && pos >= duration && !this.fading) {
          this.startFade(pos);
        }
      }
    }
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

  getCatalogState() {
    return {
      metadata: this.getMetadata(),
      infoTexts: [...this.getInfoTexts(), ...(this.catalogMeta?.infoTexts || [])],
      md5: this.catalogMeta?.md5 || null,
      songId: this.catalogMeta?.songId || null,
      subtuneCount: this.catalogMeta?.subtuneCount ?? null,
      subtuneTitle: this.catalogMeta?.subtuneTitle ?? null,
    };
  }

  getBasePlayerState() {
    this.durationMs = this.getDurationMs() || 0;
    return {
      ...this.getCatalogState(),
      durationMs: this.durationMs,
      positionMs: this.getPositionMs(),
      numSubtunes: this.getNumSubtunes(),
      subtune: this.getSubtune(),
      paramDefs: this.getParamDefs(),
      paramValues: this.getParamValues(),
      tempo: this.getTempo(),
      voiceMask: this.getVoiceMask(),
      voiceGroups: this.getVoiceGroups(),
      paused: this.paused,
    };
  }

  setLooping(looping) {
    this.looping = looping;
    if (looping) this.resetFade();
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

  handleSongEnd() {
    if (this.silenceDuration >= 0) {
      const trailingSilenceSec = this.trailingSilenceSamples / this.sampleRate;
      const neededSilenceSec = Math.max(0, this.silenceDuration - trailingSilenceSec);
      this.silenceSamplesRemaining = Math.round(neededSilenceSec * this.sampleRate);
      this.trailingSilenceSamples = 0;

      if (this.silenceSamplesRemaining <= 0) {
        this.silenceSamplesRemaining = 0;
        this.finishSong();
      }
    } else {
      this.trailingSilenceSamples = 0;
      this.finishSong();
    }
  }

  // A player plays exactly one song. The Sequencer owns sub-tune navigation:
  // every sub-tune is its own SongRef/context entry, so walking to the next
  // tune here would be a second sequencer and would desynchronise the playing
  // audio from the selected song identity.
  finishSong() {
    this.stop();
    this.emit('playerSongEnd');
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

  checkTrailingSilence(output) {
    const duration = this.durationMs || this.getDurationMs();
    const position = this.getPositionMs();

    if (duration > 0 && position >= (duration - 5000)) {
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

      if (this.trailingSilenceSamples >= (0.4 * this.sampleRate)) {
        return true;
      }
    } else {
      this.trailingSilenceSamples = 0;
    }
    return false;
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
          this.finishSong();
        }
      }
      return;
    }

    const start = performance.now();
    this.processAudioInner(output);
    this.flushDeferredSeek();
    const end = performance.now();

    if (this.stopped || this.paused) return;

    // Apply anti-pop smoothing and attenuation during seek
    if (this.seekTargetMs) {
      const fadeLength = Math.min(256, Math.floor(this.bufferSize / 2));
      for (let ch = 0; ch < output.length; ch++) {
        const channel = output[ch];
        for (let i = 0; i < fadeLength; i++) {
          const fade = i / fadeLength;
          channel[i] *= fade;
          channel[this.bufferSize - (i + 1)] *= fade;
        }
        for (let i = 0; i < this.bufferSize; i++) {
          channel[i] *= 0.2;
        }
      }
    }

    // The shared end checks must stand down for every indefinite mode, not
    // only the two base flags: VGM/MDX set `durationExtended` when leaving a
    // deep repeat, and their fade tail legitimately runs past the duration.
    const indefinitely = this.isPlayingIndefinitely() || !!this.params.indefinitePlayback;

    // Early silence cutoff (for gapless transitions and dead-air trimming)
    if (!this.fading && !indefinitely) {
      if (this.checkTrailingSilence(output)) {
        this.handleSongEnd();
        for (let ch = 0; ch < output.length; ch++) output[ch].fill(0);
        return;
      }
    }

    // Host Fade (for engines without native fadeout, e.g. GME and SID)
    if (this.hostFade && !indefinitely) {
      const duration = this.durationMs || this.getDurationMs();
      const pos = this.getPositionMs();

      if (!this.fading && duration > 0 && pos >= duration) {
        this.startFade(pos);
      }

      if (this.fading && this.fadeStartMs !== null) {
        const elapsed = pos - this.fadeStartMs;
        if (elapsed >= this.fadeDurationMs || this.fadeDurationMs === 0) {
          this.resetFade();
          this.handleSongEnd();
          for (let ch = 0; ch < output.length; ch++) output[ch].fill(0);
          return;
        }
        const fadeFactor = Math.pow(0.025, elapsed / this.fadeDurationMs);
        for (let ch = 0; ch < output.length; ch++) {
          const channel = output[ch];
          for (let i = 0; i < channel.length; i++) {
            channel[i] *= fadeFactor;
          }
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
