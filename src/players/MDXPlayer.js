import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl } from '../util';
import { CATALOG_PREFIX } from '../config';
import path from 'path';

const fileExtensions = [
  'mdx',
];
const MOUNTPOINT = '/mdx';
const INT16_MAX = Math.pow(2, 16) - 1;

export default class MDXPlayer extends Player {
  constructor(...args) {
    super(...args);

    this.loadData = this.loadData.bind(this);

    // Initialize MDX filesystem
    this.core.FS.mkdirTree(MOUNTPOINT);
    this.core.FS.mount(this.core.FS.filesystems.IDBFS, {}, MOUNTPOINT);

    this.name = 'Sharp X68000 MDX Player';
    this.speed = 1;
    this.mdxCtx = this.core._mdx_create_context();
    this.core._mdx_set_rate(this.sampleRate);
    this.core._mdx_set_dir(this.mdxCtx, MOUNTPOINT);
    this.fileExtensions = fileExtensions;
    this.buffer = this.core._malloc(this.bufferSize * 4); // 2 ch, 16-bit
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);
    const dir = path.dirname(filename);
    const mdxFilename = path.join(MOUNTPOINT, filename);
    // Preload PDX sample files into Emscripten filesystem.
    return ensureEmscFileWithData(this.core, mdxFilename, data)
      .then(() => {
        const pdx = this.core.ccall(
          'mdx_get_pdx_filename', 'string',
          ['number', 'string'],
          [this.mdxCtx, mdxFilename],
        );
        if (pdx) {
          const pdxFilename = path.join(MOUNTPOINT, dir, pdx);
          // Force upper case in the URL, as the entire MDX archive is upper case.
          // MDX files were authored on old case-insensitive filesystems, but
          // the music server filesystem (and URLs in general) are case-sensitive.
          const pdxUrl = CATALOG_PREFIX + path.join(dir, pdx.toUpperCase());
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
          this.core._mdx_set_speed(this.mdxCtx, this.speed);

          // Metadata
          const ptr = this.core._malloc(256);
          this.core._mdx_get_title(this.mdxCtx, ptr);
          const buf = this.core.HEAPU8.subarray(ptr, ptr + 256);
          const len = buf.indexOf(0);
          const title = new TextDecoder("shift-jis").decode(buf.subarray(0, len));
          this.metadata = { title: title || path.basename(filename) };

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

    const next = this.core._mdx_calc_sample(this.mdxCtx, this.buffer, this.bufferSize);
    if (next === 0) {
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

  getDurationMs() {
    return this.core._mdx_get_length(this.mdxCtx) * 1000;
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

  getNumVoices() {
    if (this.mdxCtx) return this.core._mdx_get_tracks(this.mdxCtx);
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
    this.muteAudioDuringCall(this.audioNode, () =>
      this.core._mdx_set_position_ms(this.mdxCtx, seekMs)
    );
  }

  stop() {
    this.suspend();
    this.core._mdx_close(this.mdxCtx);
    console.debug('MDXPlayer.stop()');
    this.emit('playerStateUpdate', { isStopped: true });
  }
}
