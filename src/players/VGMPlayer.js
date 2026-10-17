import range from 'lodash/range';
import { SOUNDFONT_MOUNTPOINT, SOUNDFONT_URL_PATH } from '../config';
import Player from "./Player.js";
import autoBind from 'auto-bind';
import { allOrNone, ensureEmscFileWithUrl } from '../util';

const fileExtensions = [
  'vgm',
  'vgz',
  'gym',
  's98',
  'dro',
];

const INT32_MAX = 0x8000000; // 2147483648
const YRW801_ROM_PATH = `${SOUNDFONT_MOUNTPOINT}/yrw801.rom`;

export default class VGMPlayer extends Player {
  paramDefs = [
    {
      id: 'enhancedStereo',
      label: 'Enhanced Stereo (for Mono Chips)',
      type: 'toggle',
      hint: 'Enable stereo panning for Sega PSG, YM2413, AY8910, YM2203, and YM2608.',
      defaultValue: true,
    },
    {
      id: 'indefinitePlayback',
      label: 'Indefinite Playback',
      type: 'toggle',
      hint: 'Ignore track length metadata for looping tracks and loop indefinitely.',
      defaultValue: false,
    },
  ];

  constructor(...args) {
    super(...args);
    autoBind(this);

    this.playerKey = 'vgm';
    this.name = 'LibVGM Player';
    this.speed = 1;
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4 * 2);
    this.vgmCtx = this.core._lvgm_init(this.sampleRate);
    this.core._lvgm_set_yrw801_rom_path(this.vgmCtx, this.core.stringToNewUTF8(YRW801_ROM_PATH));
    // Fade start captured when repeat one / indefinite playback is enabled
    // while a fade is already running; see syncFadeTailCapture().
    this.fadeTailStartMs = null;
    // Set when leaving repeat one after more than the default loop count: the
    // song now runs past the load-time duration (see applyLoopCount).
    this.durationExtended = false;
  }

  async loadData(data, filepath, persistedSettings) {
    const dataPtr = this.copyToHeap(data);
    const err = this.core._lvgm_load_data(this.vgmCtx, dataPtr, data.byteLength);
    this.core._free(dataPtr);
    const filepathMeta = Player.metadataFromFilepath(filepath);

    if (err !== 0) {
      console.error("lvgm_load_data failed. error code: %d", err);
      throw Error('Unable to load this file!');
    }

    const metaPtr = this.core._lvgm_get_metadata(this.vgmCtx);
    const meta = {
      title:   this.core.UTF8ToString(this.core.getValue(metaPtr + 0, 'i32')) || filepathMeta.title,
      artist:  this.core.UTF8ToString(this.core.getValue(metaPtr + 4, 'i32')),
      game:    this.core.UTF8ToString(this.core.getValue(metaPtr + 8, 'i32')),
      system:  this.core.UTF8ToString(this.core.getValue(metaPtr + 12, 'i32')),
      date:    this.core.UTF8ToString(this.core.getValue(metaPtr + 16, 'i32')),
      comment: this.core.UTF8ToString(this.core.getValue(metaPtr + 20, 'i32')),
    };

    // Loop region, when the file defines one (libvgm reports GetLoopTicks()).
    // Feature-detect the getters so older chip-core builds still load.
    if (typeof this.core._lvgm_get_loop_start_ms === 'function') {
      const loopStartMs = this.core._lvgm_get_loop_start_ms(this.vgmCtx);
      const loopEndMs = this.core._lvgm_get_loop_end_ms(this.vgmCtx);
      if (loopEndMs > loopStartMs) {
        meta.intro_length = loopStartMs;
        meta.loop_length = loopEndMs - loopStartMs;
      }
    }

    // Custom title/subtitle formatting, same as GMEPlayer:
    meta.formatted = {
      title: meta.game === meta.title ?
        meta.title :
        allOrNone(meta.game, ' - ') + meta.title,
      subtitle: [meta.artist, meta.system].filter(x => x).join(' - ') +
        allOrNone(' (', meta.date, ')'),
    };
    this.metadata = meta;

    // If OPL4 sound chip is used, load the yrw801.rom.
    const numVoices = this.core._lvgm_get_voice_count(this.vgmCtx);
    const hasOpl4 = range(numVoices).some(i =>
      this.core.UTF8ToString(this.core._lvgm_get_voice_chip_name(this.vgmCtx, i)).includes('YMF278'));
    if (hasOpl4) {
      console.debug(`${filepath} uses Yamaha OPL4 chip.`);
      // The ROM fetch is this load's async window: hold a seek issued in it
      // until playback is running, so it is not lost to _lvgm_start -- and drop
      // anything held from a previous load.
      this.beginAsyncLoad();
      await ensureEmscFileWithUrl(this.core, YRW801_ROM_PATH, `${SOUNDFONT_URL_PATH}/yrw801.rom`);
    }

    this.core._lvgm_start(this.vgmCtx);

    this.resolveParamValues(persistedSettings);
    this.setTempo(persistedSettings.tempo || 1);
    this.fadeTailStartMs = null;
    this.durationExtended = false;
    // Sync the engine's indefinite flag here, where the loop count is still 0
    // (so it cannot latch a fade), in case a mid-play toggle was masked by
    // Repeat One and skipped the native update.
    this.core._lvgm_set_indefinite_playback(this.vgmCtx, !!this.params.indefinitePlayback);
    this.applyLoopCount(false);
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

    const samplesWritten = this.core._lvgm_render(this.vgmCtx, this.buffer, this.bufferSize);
    if (samplesWritten === 0) {
      this.handleSongEnd();
      return;
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 4 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 4,                 // channel offset * bytes per sample
          'i32'                   // the sample values are 32-bit integer
        ) / INT32_MAX;
      }
    }
  }

  getTempo() {
    if (this.vgmCtx)
      return this.core._lvgm_get_playback_speed(this.vgmCtx);
  }

  setTempo(val) {
    if (this.vgmCtx)
      return this.core._lvgm_set_playback_speed(this.vgmCtx, val);
  }

  getPositionMs() {
    if (this.vgmCtx)
      return this.core._lvgm_get_position_ms(this.vgmCtx);
  }

  // The "playlist" position the slider head follows, distinct from the absolute
  // getPositionMs(). The head's phase in the loop body, (abs - A) mod B, maps
  // onto the band as bandStart + phase; looping and leaving share that mapping,
  // so toggling repeat never jumps. Once the fade starts -- or was already
  // running when repeat was enabled (fadeTailStartMs) -- the head runs the fade
  // tail from the band end instead of folding again.
  getDisplayPositionMs() {
    const abs = this.getPositionMs();
    const meta = this.metadata;
    const A = meta ? meta.intro_length : -1;
    const B = meta ? meta.loop_length : 0;
    if (!this.vgmCtx || !(A >= 0) || !(B > 0) || abs == null)
      return abs;

    const band = this.getLoopBandMs();
    if (!band) return abs;
    const bandStart = band.startMs;
    const bandEnd = band.endMs;
    const looping = this.looping || !!this.params.indefinitePlayback;

    // Before the first loop body is reached, show the real lead-in (the intro
    // and the first pass through I0).
    if (abs <= bandStart) return abs;

    if (typeof this.core._lvgm_get_fade_start_ms === 'function') {
      const fadeStart = this.fadeTailStartMs != null ? this.fadeTailStartMs
        : (looping ? null : this.core._lvgm_get_fade_start_ms(this.vgmCtx));
      if (fadeStart != null && abs >= fadeStart) {
        // Fade tail: run out from the band end.
        return Math.min(bandEnd + (abs - fadeStart), this.getDurationMs());
      }
    }

    // Inside the loop body: fold into the highlighted band. On the very first
    // pass (abs < bandEnd) play it through live, as the game would.
    if (abs < bandEnd) return abs;
    const phase = ((abs - A) % B + B) % B;
    return bandStart + phase;
  }

  getDurationMs() {
    if (this.vgmCtx)
      return this.core._lvgm_get_duration_ms(this.vgmCtx);
  }

  // Current native loop index (0 = 1st loop, 1 = 2nd, ...); 0 in the intro.
  getCurLoop() {
    if (this.vgmCtx && typeof this.core._lvgm_get_cur_loop === 'function')
      return this.core._lvgm_get_cur_loop(this.vgmCtx);
    return 0;
  }

  getVoiceGroups() {
    const voiceGroups = [];
    const numVoices = this.core._lvgm_get_voice_count(this.vgmCtx);
    let currChipName;
    let currGroup;
    for (let i = 0; i < numVoices; i++) {
      const voiceName = this.core.UTF8ToString(this.core._lvgm_get_voice_name(this.vgmCtx, i));
      const chipName = this.core.UTF8ToString(this.core._lvgm_get_voice_chip_name(this.vgmCtx, i));
      if (chipName !== currChipName) {
        currGroup = {
          name: chipName,
          icon: true, // currently hardcoded CSS class 'image-chip' (images/icon-chip.png)
          voices: [],
        };
        currChipName = chipName;
        voiceGroups.push(currGroup);
      }
      currGroup.voices.push({
        idx: i,
        name: voiceName,
      });
    }
    return voiceGroups;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(seekMs) {
    if (this.deferSeekUntilPlaying(seekMs)) return;
    if (this.vgmCtx) {
      // libvgm cancels a fade when seeking before its start; drop the display
      // capture so the fold mapping takes over again.
      this.fadeTailStartMs = null;
      this.core._lvgm_seek_ms(this.vgmCtx, seekMs);
      // libvgm keeps its loop count across a seek, and the extended count /
      // durationExtended were for the pass we just abandoned. A seek repositions
      // into the normal timeline, so restore the default finite count (the
      // wrapper's two passes at load) and let the end detector re-arm -- unless
      // Repeat One or Indefinite Playback still has it looping forever. Without
      // this, leaving a deep repeat and seeking back plays the extended count's
      // passes from the new position (measured: 7).
      if (!this.looping && !this.params.indefinitePlayback &&
          typeof this.core._lvgm_set_loop_count === 'function') {
        this.durationExtended = false;
        this.core._lvgm_set_loop_count(this.vgmCtx, 2);
      }
    }
  }

  // Capture the fade start before the loop count changes to 0 (which makes
  // _lvgm_get_fade_start_ms meaningless). Only meaningful when a fade is
  // already running: the audio then finishes it and the song ends.
  syncFadeTailCapture(looping) {
    if (!looping || this.vgmCtx == null || this.fadeTailStartMs != null ||
        typeof this.core._lvgm_get_fade_start_ms !== 'function')
      return;
    const fadeStart = this.core._lvgm_get_fade_start_ms(this.vgmCtx);
    if (fadeStart > 0 && this.getPositionMs() >= fadeStart)
      this.fadeTailStartMs = fadeStart;
  }

  // Repeat-one loops indefinitely (0) instead of fading after two passes; the
  // count is re-derived only when actually leaving a looping state
  // (`wasLooping`). Re-deriving when already not looping would push the fade
  // start past a fade already running (the engine loops through the fade, so
  // curLoop has advanced), folding the head back into the band on a no-op
  // toggle.
  applyLoopCount(wasLooping) {
    if (this.vgmCtx && typeof this.core._lvgm_set_loop_count === 'function') {
      const looping = this.looping || !!this.params.indefinitePlayback;
      if (looping) {
        this.core._lvgm_set_loop_count(this.vgmCtx, 0);
      } else if (wasLooping) {
        const curLoop = this.getCurLoop();
        // Some libvgm versions never count past the first pass, so treat
        // "already past the two-pass end" as deep too.
        const band = this.getLoopBandMs();
        const absNow = this.getPositionMs() || 0;
        const deep = curLoop >= 2 || (band != null && absNow >= band.endMs);
        const count = deep ? Math.max(2, curLoop + 1) : 2;
        this.core._lvgm_set_loop_count(this.vgmCtx, count);
        // Already past the reported duration, so the base end detector
        // (re-armed now) would cut the song before the re-scheduled fade can
        // play; `durationExtended` stands it down. The engine still ends the
        // song after the fade and its trailing silence.
        if (deep)
          this.durationExtended = true;
      }
    }
  }

  setLooping(looping) {
    const wasLooping = this.looping || !!this.params.indefinitePlayback;
    // Capture before applyLoopCount() reconfigures the loop count.
    this.syncFadeTailCapture(looping);
    super.setLooping(looping);
    this.applyLoopCount(wasLooping);
  }

  // Repeat One and Indefinite Playback loop forever by themselves, and a
  // just-left deep repeat is playing out an extended fade tail — in all three
  // cases the position legitimately runs past the reported duration, so the
  // base end detector must stay out of the way. The engine ends the song.
  isPlayingIndefinitely() {
    return this.looping || !!this.params.indefinitePlayback || !!this.durationExtended;
  }

  getVoiceName(index) {
    // TODO: Add voice chip map like github.com/mmontag/chip-player-js/commit/a698e9b
    if (this.vgmCtx) return this.core.UTF8ToString(this.core._lvgm_get_voice_name(this.vgmCtx, index));
  }

  getVoiceMask() {
    if (this.vgmCtx) {
      const bitmask = this.core._lvgm_get_voice_mask(this.vgmCtx);
      const voiceMask = [];
      for (let i = 0n; i < 64n; i++) {
        voiceMask.push((bitmask & (1n << i)) === 0n);
      }
      return voiceMask;
    }
    return [];
  }

  setVoiceMask(voiceMask) {
    if (this.vgmCtx) {
      let bitmask = 0n;
      voiceMask.forEach((isEnabled, i) => {
        if (!isEnabled) {
          bitmask += 1n << BigInt(i);
        }
      });
      this.core._lvgm_set_voice_mask(this.vgmCtx, bitmask);
    }
  }

  stop() {
    this.suspend();
    if (this.vgmCtx) this.core._lvgm_stop(this.vgmCtx);
    console.debug('VGMPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }

  setParameter(id, value) {
    switch (id) {
      case 'enhancedStereo':
        value = !!value;
        this.params[id] = value;
        if (this.vgmCtx) this.core._lvgm_set_enhanced_stereo(this.vgmCtx, value);
        break;
      case 'indefinitePlayback': {
        const wasLooping = this.looping || !!this.params.indefinitePlayback;
        value = !!value;
        this.params[id] = value;
        // Capture before the loop count changes (same as setLooping).
        this.syncFadeTailCapture(value || this.looping);
        // While Repeat One owns the loop count, sync the engine's indefinite
        // flag at load instead (loadData). Updating it here would make libvgm
        // start a fade that a later SetLoopCount(0) cannot cancel, so the song
        // would fade even though Repeat One is still on.
        if (this.vgmCtx && !this.looping)
          this.core._lvgm_set_indefinite_playback(this.vgmCtx, value);
        // Turning indefinite playback off starts a fade at the current position
        // (above). Capture that start *before* applyLoopCount() re-derives the
        // count: otherwise the fade-start getter reports the new schedule and
        // the head folds back into the band instead of showing the live tail.
        if (!value && !this.looping)
          this.syncFadeTailCapture(true);
        // Repeat-one owns the loop count while it is active.
        this.applyLoopCount(wasLooping);
        break;
      }
      default:
    }
  }
}
