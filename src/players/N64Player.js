import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl, pathJoin } from '../util';
import { CATALOG_PREFIX } from '../config';
import { parsePSF } from '../psf';
import pathe from 'pathe';
import autoBind from 'auto-bind';

const fileExtensions = [
  'miniusf',
  'usf',
];
const MOUNTPOINT = '/n64';
const INT16_MAX = 32767; // 2^15 - 1

export default class N64Player extends Player {
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

    // Initialize N64 filesystem
    this.core.FS.mkdirTree(MOUNTPOINT);
    this.core.FS.mount(this.core.FS.filesystems.IDBFS, {}, MOUNTPOINT);

    this.playerKey = 'n64';
    this.name = 'N64 Player';
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4); // 2 ch, 16-bit
  }

  loadData(data, filename, persistedSettings, subtune, isCurrent = () => true) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const psf = parsePSF(data, { filename, defaultSystem: 'Nintendo 64' });
    this.metadata = psf;

    const dir = pathe.dirname(filename);
    const fsFilename = pathJoin(MOUNTPOINT, filename);
    const filePromises = [
      ensureEmscFileWithData(this.core, fsFilename, data),
      ...psf.libraries.map(usflib => {
        const fsUsflibFilename = pathJoin(MOUNTPOINT, dir, usflib);
        const url = pathJoin(CATALOG_PREFIX, dir, usflib);
        return ensureEmscFileWithUrl(this.core, fsUsflibFilename, url);
      }),
    ];

    return Promise.all(filePromises)
      .then(() => {
        if (!isCurrent()) return;
        err = this.core.ccall(
          'n64_load_file', 'number',
          ['string', 'number', 'number', 'number'],
          [fsFilename, this.buffer, this.bufferSize, this.sampleRate],
        );

        if (err !== 0) {
          console.error('n64_load_file failed. error code: %d', err);
          throw Error('n64_load_file failed');
        }

        this.resolveParamValues(persistedSettings);

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

    const samplesWritten = this.core._n64_render_audio(this.buffer, this.bufferSize);
    if (samplesWritten <= 0) {
      this.handleSongEnd();
      return;
    }

    for (ch = 0; ch < channels.length; ch++) {
      for (i = 0; i < samplesWritten; i++) {
        channels[ch][i] = this.core.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          ch * 2,                 // channel offset * bytes per sample
          'i16'                   // the sample values are signed 16-bit integers
        ) / INT16_MAX;
      }
      channels[ch].fill(0, samplesWritten);
    }
  }

  getPositionMs() {
    return this.core._n64_get_position_ms();
  }

  getDurationMs() {
    return this.core._n64_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(positionMs) {
    this.muteAudioDuringCall(this.audioNode, () => this.core._n64_seek_ms(positionMs));
  }

  setParameter(id, value) {
    switch (id) {
      case 'indefinitePlayback':
        value = !!value;
        this.params[id] = value;
        this.core._n64_set_indefinite_playback(value);
        break;
      default:
    }
  }

  stop() {
    this.suspend();
    this.core._n64_shutdown();
    console.debug('N64Player.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
