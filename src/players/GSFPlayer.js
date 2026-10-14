import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl, pathJoin } from '../util';
import { CATALOG_PREFIX } from '../config';
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
  'DirectSound A',
  'DirectSound B',
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
  }

  loadData(data, filename, persistedSettings) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const decoder = new TextDecoder('latin1');
    const gsfStr = decoder.decode(data);

    // Extract all _lib, _lib2, etc. dependencies if present
    const libMatches = gsfStr.matchAll(/_lib\d*=\s*([^\r\n]+)/gi);
    const gsflibs = [];
    for (const match of libMatches) {
      if (match[1]) {
        gsflibs.push(match[1].trim());
      }
    }

    const dir = pathe.dirname(filename);
    const fsFilename = pathJoin(MOUNTPOINT, filename);
    const filePromises = [
      ensureEmscFileWithData(this.core, fsFilename, data),
      ...gsflibs.map(gsflib => {
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
        this.metadata = { title: pathe.basename(filename) };

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
    let i, ch;

    if (this.paused) {
      for (ch = 0; ch < channels.length; ch++) {
        channels[ch].fill(0);
      }
      return;
    }

    const samplesWritten = this.core._gsf_render_audio(this.buffer, this.bufferSize);
    if (samplesWritten <= 0) {
      this.handleSongEnd();
      return;
    }

    const currentDriverType = this.core._gsf_get_driver_type ? this.core._gsf_get_driver_type() : 0;
    if (currentDriverType !== this.driverType) {
      this.driverType = currentDriverType;
      const numTracks = (currentDriverType === 1 && this.core._gsf_get_track_count)
        ? this.core._gsf_get_track_count()
        : GBA_VOICE_NAMES.length;
      this.numTracks = numTracks;
      this.voiceMask = Array(numTracks).fill(true);
      this.emit('playerStateUpdate', {
        ...this.getBasePlayerState(),
      });
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // signed 16-bit integer
        ) / INT16_MAX;
      }
    }
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

  seekMs(positionMs) {
    this.muteAudioDuringCall(this.audioNode, () => this.core._gsf_seek_ms(positionMs));
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
      return [{
        name: 'Sequencer Tracks',
        icon: true,
        voices: Array.from({ length: numTracks }, (_, i) => ({
          idx: i,
          name: this.getVoiceName(i),
        })),
      }];
    }

    return [
      {
        name: 'Game Boy PSG',
        icon: true,
        voices: [
          { idx: 0, name: 'Square 1' },
          { idx: 1, name: 'Square 2' },
          { idx: 2, name: 'Wave' },
          { idx: 3, name: 'Noise' },
        ],
      },
      {
        name: 'DirectSound',
        icon: true,
        voices: [
          { idx: 4, name: 'DirectSound A' },
          { idx: 5, name: 'DirectSound B' },
        ],
      },
    ];
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
        this.core._gsf_set_indefinite_playback(value);
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
