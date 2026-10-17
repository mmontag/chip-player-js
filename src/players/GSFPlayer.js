import Player from "./Player.js";
import EndDetector, { DETECT_SONG_END_HINT } from './EndDetector.js';
import { ensureEmscFileWithData, ensureEmscFileWithUrl, pathJoin } from '../util';
import { CATALOG_PREFIX } from '../config';
import { parsePSF } from '../psf';
import pathe from 'pathe';
import autoBind from 'auto-bind';

const fileExtensions = [
  'minigsf',
  'gsf',
];
const MOUNTPOINT = '/gsf';
const INT16_MAX = 32767; // 2^15 - 1

const GBA_VOICE_NAMES = [
  'Square 1',
  'Square 2',
  'Wave',
  'Noise',
  'Left',
  'Right',
];

export default class GSFPlayer extends Player {
  paramDefs = [
    {
      id: 'indefinitePlayback',
      label: 'Indefinite Playback',
      type: 'toggle',
      hint: 'Ignore track length metadata for looping tracks and loop indefinitely.',
      defaultValue: false,
    },
    {
      id: 'detectSongEnd',
      label: 'Detect Song End While Repeat One',
      type: 'toggle',
      hint: DETECT_SONG_END_HINT,
      defaultValue: true,
    },
  ];

  constructor(...args) {
    super(...args);
    autoBind(this);

    // Initialize GSF filesystem in Emscripten IDBFS
    this.core.FS.mkdirTree(MOUNTPOINT);
    this.core.FS.mount(this.core.FS.filesystems.IDBFS, {}, MOUNTPOINT);

    this.playerKey = 'gsf';
    this.name = 'GSF Player';
    this.fileExtensions = fileExtensions;
    this.voiceMask = Array(GBA_VOICE_NAMES.length).fill(true);
    this.speed = 1.0;
    this.buffer = this.core._malloc(this.bufferSize * 4); // 2 ch, 16-bit
    // Same tail detector as SID/N64 (see EndDetector). GSF has no loop region --
    // the MP2K driver loops internally -- so Repeat One free-runs the engine and
    // this is what re-runs a one-shot whose content has ended.
    this.endDetector = new EndDetector({
      sampleRate: this.sampleRate,
      bufferSize: this.bufferSize,
    });
  }

  resetEndDetector() {
    this.endDetector.reset();
  }

  // Start of the end-detection trip window, one window before the expected end.
  getEndDetectTripAtMs() {
    return this.endDetector.getTripAtMs(this.getDurationMs());
  }

  loadData(data, filename, persistedSettings) {
    this.resetEndDetector();
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const psf = parsePSF(data, { filename, defaultSystem: 'Game Boy Advance' });
    this.metadata = psf;

    const dir = pathe.dirname(filename);
    const fsFilename = pathJoin(MOUNTPOINT, filename);
    const filePromises = [
      ensureEmscFileWithData(this.core, fsFilename, data),
      ...psf.libraries.map(gsflib => {
        const fsGsflibFilename = pathJoin(MOUNTPOINT, dir, gsflib);
        const url = pathJoin(CATALOG_PREFIX, dir, gsflib);
        return ensureEmscFileWithUrl(this.core, fsGsflibFilename, url);
      }),
    ];

    return Promise.all(filePromises)
      .then(() => {
        err = this.core.ccall(
          'gsf_load_file', 'number',
          ['string', 'number', 'number', 'number'],
          [fsFilename, this.buffer, this.bufferSize, this.sampleRate],
        );

        if (err !== 0) {
          console.error('gsf_load_file failed. error code: %d', err);
          throw Error('gsf_load_file failed');
        }

        this.resolveParamValues(persistedSettings);
        this.setTempo(persistedSettings.tempo || 1);

        const driverType = this.core._gsf_get_driver_type ? this.core._gsf_get_driver_type() : 0;
        const numTracks = (driverType === 1 && this.core._gsf_get_track_count)
          ? this.core._gsf_get_track_count()
          : GBA_VOICE_NAMES.length;
        this.driverType = driverType;
        this.numTracks = numTracks;
        this.voiceMask = Array(numTracks).fill(true);

        this.resume();
        this.emit('playerStateUpdate', {
          ...this.getBasePlayerState(),
          isStopped: false,
        });
      });
  }

  processAudioInner(channels) {
    if (this.paused) {
      for (const channel of channels) {
        channel.fill(0);
      }
      return;
    }

    let samplesWritten = this.core._gsf_render_audio(this.buffer, this.bufferSize);
    if (samplesWritten <= 0) {
      this.handleSongEnd();
      return;
    }

    const currentDriverType = this.core._gsf_get_driver_type ? this.core._gsf_get_driver_type() : 0;
    if (currentDriverType !== this.driverType && !(this.driverType === 1 && currentDriverType === 0)) {
      this.driverType = currentDriverType;
      const numTracks = (currentDriverType === 1 && this.core._gsf_get_track_count)
        ? this.core._gsf_get_track_count()
        : GBA_VOICE_NAMES.length;
      this.numTracks = numTracks;
      if (!this.voiceMask || this.voiceMask.length !== numTracks) {
        this.voiceMask = Array(numTracks).fill(true);
      }
      this.emit('playerStateUpdate', {
        ...this.getBasePlayerState(),
      });
    }

    this.writeChannels(channels);

    // Repeat One tail restart (rule and window in EndDetector). Under indefinite
    // playback the engine free-runs past durationMs: the MP2K driver loops
    // internally, so a looping track never goes quiet, but a one-shot whose
    // content has ended sits silent forever -- re-run it from the top in-buffer
    // (no refetch, no gap). The position gate runs first, so quiet intros and
    // mid-song breakdowns stay gated. Repeat-off keeps the end above.
    if (this.params.detectSongEnd && this.isPlayingIndefinitely() &&
        this.getPositionMs() >= this.getEndDetectTripAtMs() && this.updateEndDetector(channels)) {
      this.restartTrack();
      samplesWritten = this.core._gsf_render_audio(this.buffer, this.bufferSize);
      if (samplesWritten <= 0) {
        this.handleSongEnd();
        return;
      }
      this.writeChannels(channels);
    }
  }

  // Interleaved stereo i16 -> the float channels the Web Audio graph consumes.
  writeChannels(channels) {
    for (let ch = 0; ch < channels.length; ch++) {
      for (let i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // signed 16-bit integer
        ) / INT16_MAX;
      }
    }
  }

  // Fold one rendered buffer into the tail detector, tapping the converted float
  // channels (GSF's int16 buffer holds no float views, unlike SID).
  updateEndDetector(channels) {
    // Masking is applied in-engine, so a muted/soloed part reads as silence in
    // both gates -- stand down while the mask is not clean, exactly as
    // SIDPlayer does (the JS echo of GME disabling its own silence detection on
    // any mute).
    if (!Array.isArray(this.voiceMask) || !this.voiceMask.every(Boolean)) {
      this.endDetector.reset();
      return false;
    }
    return this.endDetector.fold(channels[0], channels[1] || channels[0]);
  }

  getPositionMs() {
    return this.core._gsf_get_position_ms();
  }

  getDurationMs() {
    return this.core._gsf_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  getTempo() {
    return this.speed;
  }

  setTempo(val) {
    this.speed = Math.max(0.1, Math.min(2.0, val));
    if (this.core._gsf_set_speed) {
      this.core._gsf_set_speed(this.speed);
    }
  }

  // Repeat One alone not reaching the engine would fade-and-end each cycle and
  // reload the whole gsflib per loop.
  syncIndefinitePlayback() {
    if (this.core._gsf_set_indefinite_playback) {
      this.core._gsf_set_indefinite_playback(this.looping || !!this.params.indefinitePlayback);
    }
  }

  // Repeat One and Indefinite Playback both free-run past durationMs via the
  // engine flag, so the base end detector must stay out of the way.
  isPlayingIndefinitely() {
    return this.looping || !!this.params.indefinitePlayback;
  }

  setLooping(looping) {
    super.setLooping(looping);
    this.syncIndefinitePlayback();
  }

  seekMs(positionMs) {
    // EndDetector's contract: reset on seek, so pre-seek audio cannot trip the
    // Repeat One tail restart before the new position has produced a window.
    this.resetEndDetector();
    this.muteAudioDuringCall(this.audioNode, () => {
      this.core._gsf_seek_ms(positionMs);
      this.setVoiceMask(this.voiceMask);
    });
  }

  // In-buffer restart for the Repeat One tail detector (like N64's restartTrack):
  // seek-to-0 re-runs the driver from the top without refetching the gsflib.
  // Deliberately synchronous -- muteAudioDuringCall() is async, so routing this
  // through seekMs() would let the re-render below read the old tail position.
  restartTrack() {
    this.resetEndDetector();
    this.core._gsf_seek_ms(0);
    this.setVoiceMask(this.voiceMask);
  }

  getVoiceName(index) {
    if (this.core._gsf_get_track_name) {
      return this.core.UTF8ToString(this.core._gsf_get_track_name(index));
    }
    return GBA_VOICE_NAMES[index] || `Channel ${index + 1}`;
  }

  getVoiceGroups() {
    const driverType = this.core._gsf_get_driver_type ? this.core._gsf_get_driver_type() : 0;
    if (driverType === 1) {
      const numTracks = this.core._gsf_get_track_count ? this.core._gsf_get_track_count() : 0;
      const dmgVoices = [];
      const directSoundVoices = [];

      for (let i = 0; i < numTracks; i++) {
        const type = this.core._gsf_get_track_type ? this.core._gsf_get_track_type(i) : 0;
        const voice = {
          idx: i,
          name: this.getVoiceName(i),
        };
        if (type >= 1 && type <= 4) {
          dmgVoices.push(voice);
        } else {
          directSoundVoices.push(voice);
        }
      }

      const groups = [];
      if (dmgVoices.length > 0) {
        groups.push({
          name: 'GameBoy DMG',
          icon: true,
          voices: dmgVoices,
        });
      }
      if (directSoundVoices.length > 0) {
        groups.push({
          name: 'Direct Sound',
          icon: true,
          voices: directSoundVoices,
        });
      }
      return groups;
    }

    const hasDmg = this.core._gsf_has_dmg_channels ? this.core._gsf_has_dmg_channels() : true;
    const groups = [];
    if (hasDmg) {
      groups.push({
        name: 'GameBoy DMG',
        icon: true,
        voices: [
          { idx: 0, name: 'Square 1' },
          { idx: 1, name: 'Square 2' },
          { idx: 2, name: 'Wave' },
          { idx: 3, name: 'Noise' },
        ],
      });
    }

    groups.push({
      name: 'Direct Sound',
      icon: true,
      voices: [
        { idx: 4, name: 'Left' },
        { idx: 5, name: 'Right' },
      ],
    });

    return groups;
  }

  getVoiceMask() {
    return this.voiceMask;
  }

  setVoiceMask(voiceMask) {
    if (!voiceMask) return;
    this.voiceMask = voiceMask;

    const driverType = this.core._gsf_get_driver_type ? this.core._gsf_get_driver_type() : 0;
    if (driverType === 1 && this.core._gsf_set_track_mask) {
      let bitmask = 0;
      for (let i = 0; i < voiceMask.length; i++) {
        if (voiceMask[i]) bitmask |= (1 << i);
      }
      this.core._gsf_set_track_mask(bitmask);
    } else {
      let bitmask = 0;
      if (voiceMask[0]) bitmask |= 0x001; // Pulse 1
      if (voiceMask[1]) bitmask |= 0x002; // Pulse 2
      if (voiceMask[2]) bitmask |= 0x004; // Wave
      if (voiceMask[3]) bitmask |= 0x008; // Noise
      if (voiceMask[4]) bitmask |= 0x100; // DirectSound A
      if (voiceMask[5]) bitmask |= 0x200; // DirectSound B
      this.core._gsf_set_channel_mask(bitmask);
    }

    this.emit('playerStateUpdate', {
      voiceMask: this.voiceMask,
    });
  }

  setParameter(id, value) {
    switch (id) {
      case 'indefinitePlayback':
        value = !!value;
        this.params[id] = value;
        this.syncIndefinitePlayback();
        break;
      case 'detectSongEnd':
        this.params[id] = !!value;
        this.resetEndDetector();
        break;
      default:
    }
  }

  stop() {
    this.suspend();
    this.core._gsf_shutdown();
    console.debug('GSFPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
