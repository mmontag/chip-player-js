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
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function () {
  }) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.loadData = this.loadData.bind(this);

    // Initialize MDX filesystem
    chipCore.FS.mkdirTree(MOUNTPOINT);
    chipCore.FS.mount(chipCore.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    chipCore.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
    });

    this.lib = chipCore;
    this.mdxCtx = chipCore._mdx_create_context();
    chipCore._mdx_set_rate(audioCtx.sampleRate);
    chipCore._mdx_set_dir(this.mdxCtx, MOUNTPOINT);
    this.fileExtensions = fileExtensions;
    this.buffer = chipCore._malloc(this.bufferSize * 4); // 2 ch, 16-bit
    this.setAudioProcess(this.mdxAudioProcess);
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);
    const dir = path.dirname(filename);
    const mdxFilename = path.join(MOUNTPOINT, filename);
    // Preload PDX sample files into Emscripten filesystem.
    ensureEmscFileWithData(this.lib, mdxFilename, data)
      .then(() => {
        const pdx = this.lib.ccall(
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
          return ensureEmscFileWithUrl(this.lib, pdxFilename, pdxUrl);
        }
      })
      .then(() => {
        this.muteAudioDuringCall(this.audioNode, () => {
          err = this.lib.ccall(
            'mdx_open', 'number',
            ['number', 'string', 'string'],
            [this.mdxCtx, mdxFilename, null],
          );

          if (err !== 0) {
            console.error("mdx_load_file failed. error code: %d", err);
            throw Error('Unable to load this file!');
          }

          // TODO: MDX metadata
          this.metadata = { title: filename };

          this.connect();
          this.resume();
          this.onPlayerStateUpdate(false);
        });
      });
  }

  mdxAudioProcess(e) {
    let i, channel;
    const channels = [];
    for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
      channels[channel] = e.outputBuffer.getChannelData(channel);
    }

    if (this.paused) {
      for (channel = 0; channel < channels.length; channel++) {
        channels[channel].fill(0);
      }
      return;
    }

    const next = this.lib._mdx_calc_sample(
      this.mdxCtx, this.buffer, this.bufferSize);
    if (next === 0) {
      this.stop();
    }

    for (channel = 0; channel < channels.length; channel++) {
      for (i = 0; i < this.bufferSize; i++) {
        channels[channel][i] = this.lib.getValue(
          this.buffer +           // Interleaved channel format
          i * 2 * 2 +             // frame offset   * bytes per sample * num channels +
          channel * 2,            // channel offset * bytes per sample
          'i16'                   // the sample values are signed 16-bit integers
        ) / INT16_MAX;
      }
    }
  }

  setTempo(val) {
    return this.lib._mdx_set_speed(this.mdxCtx, val);
  }

  getPositionMs() {
    return this.lib._mdx_get_position_ms(this.mdxCtx);
  }

  getDurationMs() {
    return this.lib._mdx_get_length(this.mdxCtx) * 1000;
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  getVoiceName(index) {
    if (this.mdxCtx) return this.lib.UTF8ToString(this.lib._mdx_get_track_name(this.mdxCtx, index));
  }

  getNumVoices() {
    if (this.mdxCtx) return this.lib._mdx_get_tracks(this.mdxCtx);
  }

  setVoices(voices) {
    let mask = 0;
    voices.forEach((isEnabled, i) => {
      if (!isEnabled) {
        mask += 1 << i;
      }
    });
    if (this.mdxCtx) this.lib._mdx_set_track_mask(this.mdxCtx, mask);
  }

  seekMs(seekMs) {
    this.muteAudioDuringCall(this.audioNode, () =>
      this.lib._mdx_set_position_ms(this.mdxCtx, seekMs)
    );
  }

  stop() {
    this.suspend();
    this.lib._mdx_close(this.mdxCtx);
    console.debug('MDXPlayer.stop()');
    this.onPlayerStateUpdate(true);
  }
}
