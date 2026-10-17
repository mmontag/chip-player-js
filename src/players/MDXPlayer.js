import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl, pathJoin } from '../util';
import { CATALOG_PREFIX } from '../config';
import pathe from 'pathe';
import autoBind from 'auto-bind';
import chipImage from '../images/chip.png';

const fileExtensions = [
  'mdx',
];
const MOUNTPOINT = '/mdx';
const INT16_MAX = Math.pow(2, 16) - 1;
// MDX songs with an infinite loop play this many times before fading out.
// Two passes matches libvgm's default and the app's loop model (see
// Player.getLoopBandMs), so MDX gets the same slider band and head fold.
const DEFAULT_LOOP_COUNT = 2;

export default class MDXPlayer extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    // Initialize MDX filesystem
    this.core.FS.mkdirTree(MOUNTPOINT);
    this.core.FS.mount(this.core.FS.filesystems.IDBFS, {}, MOUNTPOINT);

    this.playerKey = 'mdx';
    this.name = 'Sharp X68000 MDX Player';
    this.speed = 1;
    this._durationMs = 0;
    this._maxLoopCount = DEFAULT_LOOP_COUNT;
    this.durationExtended = false; // repeat-one left after passing the fade
    // Fade start captured when repeat one is enabled while a fade is already
    // running; see syncFadeTailCapture().
    this.fadeTailStartMs = null;
    // Whether mdx_open has succeeded and not yet been closed. Guards
    // _mdx_set_max_loop against a freed song (see applyLoopCount).
    this.songOpen = false;
    this.mdxCtx = this.core._mdx_create_context();
    this.core._mdx_set_rate(this.sampleRate);
    this.core._mdx_set_dir(this.mdxCtx, MOUNTPOINT);
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4); // 2 ch, 16-bit
  }

  loadData(data, filename, persistedSettings) {
    // MDXPlayer reads song data from the Emscripten filesystem,
    // rather than loading bytes from memory like other players.
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);
    const dir = pathe.dirname(filename);
    const mdxFilename = pathJoin(MOUNTPOINT, filename);
    // First, write PDX sample files into Emscripten filesystem.
    return ensureEmscFileWithData(this.core, mdxFilename, data)
      .then(() => {
        const pdx = this.core.ccall(
          'mdx_get_pdx_filename', 'string',
          ['number', 'string'],
          [this.mdxCtx, mdxFilename],
        );
        if (pdx) {
          const pdxFilename = pathJoin(MOUNTPOINT, dir, pdx);
          // Force upper case in the URL, as the entire MDX archive is upper case.
          // MDX files were authored on old case-insensitive filesystems, but
          // the music server filesystem (and URLs in general) are case-sensitive.
          const pdxUrl = pathJoin(CATALOG_PREFIX, dir, pdx.toUpperCase());
          // Write MDX file into Emscripten filesystem.
          return ensureEmscFileWithUrl(this.core, pdxFilename, pdxUrl);
        }
      })
      .then(() => {
        this.muteAudioDuringCall(this.audioNode, () => {
          err = this.core.ccall(
            'mdx_open', 'number',
            ['number', 'string', 'string'],
            [this.mdxCtx, mdxFilename, null],
          );

          if (err !== 0) {
            console.error("mdx_load_file failed. error code: %d", err);
            throw Error('mdx_load_file failed');
          }

          // Metadata
          const ptr = this.core._malloc(256);
          this.core._mdx_get_title(this.mdxCtx, ptr);
          const buf = this.core.HEAPU8.subarray(ptr, ptr + 256);
          const len = buf.indexOf(0);
          const title = new TextDecoder("shift-jis").decode(buf.subarray(0, len));
          this.metadata = { title: title || pathe.basename(filename) };
          this.songOpen = true;
          this._readLoopRegion();
          this.applyLoopCount(false);

          this.resolveParamValues(persistedSettings);
          this.setTempo(persistedSettings.tempo || 1);
          this.resume();
          this.emit('playerStateUpdate', {
            ...this.getBasePlayerState(),
            isStopped: false,
          });
        });
      });
  }

  // Measure the song's built-in loop region once, at load. _mdx_get_length
  // re-parses the song and rewinds the track, so it must run only here (callers
  // poll getDurationMs every 100 ms). max_loop=2 makes the engine record where
  // its infinite loop completes each pass: length(k) = intro + k * loop + fade,
  // so two loop points give the exact loop length and, since the fade is a
  // constant offset, the intro too. No loop API: a plain duration, no band.
  _readLoopRegion() {
    const core = this.core;
    this.durationExtended = false;
    this.fadeTailStartMs = null;
    this._maxLoopCount = DEFAULT_LOOP_COUNT;
    if (typeof core._mdx_set_max_loop === 'function') {
      core._mdx_set_max_loop(this.mdxCtx, DEFAULT_LOOP_COUNT);
    }
    this._durationMs = core._mdx_get_length(this.mdxCtx) * 1000;
    if (typeof core._mdx_get_loop_start_ms === 'function' &&
        typeof core._mdx_get_loop_length_ms === 'function') {
      const introMs = core._mdx_get_loop_start_ms(this.mdxCtx);
      const loopMs = core._mdx_get_loop_length_ms(this.mdxCtx);
      if (loopMs > 0) {
        // The shared intro_length/loop_length vocabulary maps MDX onto the
        // generic loop hooks, exactly as VGMPlayer does for libvgm. The band the
        // slider highlights is the second pass; Player.getLoopBandMs() hides it
        // when the song cannot reach that far (see there).
        this.metadata.intro_length = introMs;
        this.metadata.loop_length = loopMs;
      }
    }
  }

  processAudioInner(channels) {
    let i, ch;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    const next = this.core._mdx_calc_sample(this.mdxCtx, this.buffer, this.bufferSize);
    if (next === 0) {
      this.handleSongEnd();
      return;
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // the sample values are signed 16-bit integers
        ) / INT16_MAX;
      }
    }
  }

  getTempo() {
    return this.speed;
  }

  setTempo(val) {
    this.speed = val;
    return this.core._mdx_set_speed(this.mdxCtx, val);
  }

  getPositionMs() {
    return this.core._mdx_get_position_ms(this.mdxCtx);
  }

  // Repeat One loops the engine's own infinite loop (0 = forever) so the region
  // repeats seamlessly; leaving restores a finite count so the current pass
  // finishes and the engine fades, like libvgm.
  setLooping(looping) {
    const wasLooping = this.looping;
    // Capture before applyLoopCount() changes the pass count.
    this.syncFadeTailCapture(looping);
    super.setLooping(looping);
    this.applyLoopCount(wasLooping);
  }

  // Absolute position where the configured pass count fades out, or 0 when the
  // song has no measured loop region.
  _fadeStartMs() {
    const meta = this.metadata;
    const intro = meta ? meta.intro_length : -1;
    const loop = meta ? meta.loop_length : 0;
    if (!(intro >= 0) || !(loop > 0)) return 0;
    return intro + this._maxLoopCount * loop;
  }

  // Capture the fade start before the pass count changes to 0. mdxmini latches
  // fade_out and only reinit clears it, so a fade already running keeps running
  // (enabling repeat one then ends the song); remembering where it began keeps
  // the head on that tail (cf. VGMPlayer.syncFadeTailCapture).
  syncFadeTailCapture(looping) {
    if (!looping || this.fadeTailStartMs != null) return;
    const fadeStart = this._fadeStartMs();
    if (fadeStart > 0 && this.getPositionMs() >= fadeStart) {
      this.fadeTailStartMs = fadeStart;
    }
  }

  // Loop count to hand the engine for the current state: 0 forever while
  // repeating, otherwise finish the in-progress pass and fade. Only re-derive
  // when actually leaving a looping state; a no-op toggle must not push the
  // fade boundary past a fade that is already scheduled.
  applyLoopCount(wasLooping) {
    // Gate the pre-load call and any call after stop(): Sequencer.playSong
    // invokes setLooping() before loadData, and mdxmini's mdx_set_max_loop
    // dereferences data->mdx. `metadata` alone cannot gate that -- stop() frees
    // the song but leaves metadata set -- so gate on `songOpen`, set by
    // mdx_open and cleared by stop().
    if (!this.mdxCtx || !this.songOpen || typeof this.core._mdx_set_max_loop !== 'function') return;
    const hasLoop = this.metadata && this.metadata.loop_length > 0;
    if (this.looping) {
      this._maxLoopCount = 0;
    } else if (wasLooping) {
      // Only re-derive when actually leaving a looping state. A false-to-false
      // toggle (Repeat Off <-> All) must not reset a count already extended for
      // a deep pass, or the engine fades the pass short.
      if (hasLoop) {
        const curLoop = this.getCurLoop();
        this._maxLoopCount = Math.max(DEFAULT_LOOP_COUNT, curLoop + 1);
        // Leaving always stands the base end detector down: the engine's own
        // fade has to end the song. _durationMs is the parse's length at the
        // default two-pass count, measured on the same joint-section grid
        // playback gates on, so a deep leave (curLoop >= 2) is already at or
        // past it and the re-scheduled fade starts where the detector's gate
        // would cut. The engine's finite count still ends the song, so standing
        // the detector down cannot hang; a shallow leave is the same path.
        this.durationExtended = true;
      } else {
        // No measured loop region: finish the pass and fade.
        this._maxLoopCount = DEFAULT_LOOP_COUNT;
      }
    }
    // One unit: the engine counts joint sections itself (mdxmml_ym2151.c's
    // loop_gate_value projects each track's repeats onto the section the band
    // is drawn from), so _maxLoopCount is what it is handed -- no conversion.
    this.core._mdx_set_max_loop(this.mdxCtx, this._maxLoopCount);
  }

  // Which pass of the built-in loop we are in (0 during the intro/first pass).
  getCurLoop() {
    const meta = this.metadata;
    const intro = meta ? meta.intro_length : -1;
    const loop = meta ? meta.loop_length : 0;
    if (!(loop > 0) || !(intro >= 0)) return 0;
    return Math.max(0, Math.floor((this.getPositionMs() - intro) / loop));
  }

  // Same playlist-position model as VGMPlayer: the head cycles inside the
  // highlighted band (the last non-fade pass) while repeating, and runs the
  // fade tail from the band end once the fade starts, so toggling Repeat One
  // never makes the head jump.
  getDisplayPositionMs() {
    const abs = this.getPositionMs();
    const meta = this.metadata;
    const intro = meta ? meta.intro_length : -1;
    const loop = meta ? meta.loop_length : 0;
    if (!(intro >= 0) || !(loop > 0) || abs == null) return abs;

    const band = this.getLoopBandMs();
    if (!band) return abs;
    const bandStart = band.startMs;
    const bandEnd = band.endMs;
    if (abs <= bandStart) return abs;

    // While repeating, the configured fade start is meaningless (0 passes), so
    // only a captured tail applies; a seek or the next load drops it.
    const fadeStartMs = this.fadeTailStartMs != null ? this.fadeTailStartMs
      : (this.looping ? null : this._fadeStartMs());
    if (fadeStartMs != null && abs >= fadeStartMs) {
      return Math.min(bandEnd + (abs - fadeStartMs), this.getDurationMs());
    }

    if (abs < bandEnd) return abs;
    const phase = ((abs - intro) % loop + loop) % loop;
    return bandStart + phase;
  }

  isPlayingIndefinitely() {
    return this.looping || !!this.durationExtended;
  }

  getDurationMs() {
    return this._durationMs;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  getVoiceName(index) {
    if (this.mdxCtx) return this.core.UTF8ToString(this.core._mdx_get_track_name(this.mdxCtx, index));
  }

  getVoiceGroups() {
    if (!this.mdxCtx) return [];
    const voiceGroups = [];
    const numVoices = this.core._mdx_get_tracks(this.mdxCtx);
    let currGroup;
    for (let i = 0; i < numVoices; i++) {
      const voiceName = this.core.UTF8ToString(this.core._mdx_get_track_name(this.mdxCtx, i));
      if (i === 0) {
        currGroup = {
          name: 'YM2151 (OPM)',
          icon: chipImage,
          voices: [],
        };
        voiceGroups.push(currGroup);
      }
      if (i === 8) {
        currGroup = {
          name: numVoices === 9 ? 'OKI MSM6258' : 'Mercury Unit (PCM8)',
          icon: chipImage,
          voices: [],
        };
        voiceGroups.push(currGroup);
      }
      currGroup.voices.push({
        idx: i,
        name: voiceName,
      });
    }
    return voiceGroups;
  }

  getVoiceMask() {
    const voiceMask = [];
    const mask = this.core._mdx_get_track_mask(this.mdxCtx);
    for (let i = 0; i < this.core._mdx_get_tracks(this.mdxCtx); i++) {
      voiceMask.push(((mask >> i) & 1) === 0);
    }
    return voiceMask;
  }

  setVoiceMask(voiceMask) {
    let mask = 0;
    voiceMask.forEach((isEnabled, i) => {
      if (!isEnabled) {
        mask += 1 << i;
      }
    });
    if (this.mdxCtx) this.core._mdx_set_track_mask(this.mdxCtx, mask);
  }

  seekMs(seekMs) {
    // The captured tail belongs to the old position.
    this.fadeTailStartMs = null;
    this.muteAudioDuringCall(this.audioNode, () =>
      this.core._mdx_set_position_ms(this.mdxCtx, seekMs)
    );
    // mdxmini keeps max_infinite_loops across a seek, and the extended count /
    // durationExtended were for the pass we just abandoned. A seek repositions
    // into the normal timeline, so restore the default finite count and let the
    // end detector re-arm -- unless Repeat One is still on (count 0).
    if (this.songOpen && !this.looping && typeof this.core._mdx_set_max_loop === 'function') {
      this._maxLoopCount = DEFAULT_LOOP_COUNT;
      this.durationExtended = false;
      this.core._mdx_set_max_loop(this.mdxCtx, this._maxLoopCount);
    }
  }

  stop() {
    this.suspend();
    this.core._mdx_close(this.mdxCtx);
    // mdx_close frees the song data behind mdxCtx; mark it closed so a later
    // setLooping() (Sequencer calls it before the next loadData) cannot hand
    // the freed context to _mdx_set_max_loop.
    this.songOpen = false;
    console.debug('MDXPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
