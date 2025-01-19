import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl, pathJoin } from '../util';
import { CATALOG_PREFIX } from '../config';
import path from 'path';
import autoBind from 'auto-bind';

const fileExtensions = [
  'miniusf',
];
const MOUNTPOINT = '/n64';
const INT16_MAX = Math.pow(2, 16) - 1;

export default class N64Player extends Player {
  constructor(...args) {
    super(...args);
    autoBind(this);

    // Initialize N64 filesystem
    this.core.FS.mkdirTree(MOUNTPOINT);
    this.core.FS.mount(this.core.FS.filesystems.IDBFS, {}, MOUNTPOINT);

    this.name = 'N64 Player';
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4); // 2 ch, 16-bit
  }

  loadData(data, filename) {
    // N64Player reads song data from the Emscripten filesystem,
    // rather than loading bytes from memory like other players.
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const miniusfStr = String.fromCharCode.apply(null, data);
    const usflibs = miniusfStr.match(/_lib=([^\n]+)/).slice(1);
    if (usflibs.length === 0) {
      throw new Error(`No .usflib references found`);
    }

    const dir = path.dirname(filename);
    const fsFilename = pathJoin(MOUNTPOINT, filename);
    const promises = [
      ensureEmscFileWithData(this.core, fsFilename, data),
      ...usflibs.map(usflib => {
        const fsFilename = pathJoin(MOUNTPOINT, dir, usflib);
        const url = pathJoin(CATALOG_PREFIX, dir, usflib);
        return ensureEmscFileWithUrl(this.core, fsFilename, url);
      }),
    ];

    return Promise.all(promises)
      .then(([fsFilename]) => {
        this.muteAudioDuringCall(this.audioNode, () => {
          err = this.core.ccall(
            'n64_load_file', 'number',
            ['string', 'number', 'number', 'number'],
            [fsFilename, this.buffer, this.bufferSize, this.sampleRate],
          );

          if (err !== 0) {
            console.error("n64_load_file failed. error code: %d", err);
            throw Error('n64_load_file failed');
          }

          this.metadata = { title: filename };

          this.resume();
          this.emit('playerStateUpdate', {
            ...this.getBasePlayerState(),
            isStopped: false,
          });
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
      this.stop();
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

  seekMs(seekMs) {
    // TODO: timesliced seeking
    this.core._n64_seek_ms(seekMs)
  }

  stop() {
    this.suspend();
    this.core._n64_shutdown();
    console.debug('N64Player.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
