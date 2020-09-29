import Player from "./Player.js";
import { ensureEmscFileWithData, ensureEmscFileWithUrl } from '../util';
import { CATALOG_PREFIX } from '../config';
import path from 'path';

const fileExtensions = [
  'miniusf',
];
const MOUNTPOINT = '/n64';
const INT16_MAX = Math.pow(2, 16) - 1;

export default class N64Player extends Player {
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.loadData = this.loadData.bind(this);

    // Initialize Soundfont filesystem
    chipCore.FS.mkdirTree(MOUNTPOINT);
    chipCore.FS.mount(chipCore.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    chipCore.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
    });

    this.lib = chipCore;
    this.fileExtensions = fileExtensions;
    this.buffer = chipCore._malloc(this.bufferSize * 4); // 2 ch, 16-bit
    this.setAudioProcess(this.n64AudioProcess);
  }

  loadData(data, filename) {
    let err;
    this.filepathMeta = Player.metadataFromFilepath(filename);

    const miniusfStr = String.fromCharCode.apply(null, data);
    const usflibs = miniusfStr.match(/_lib=([^\s]+)/).slice(1);
    if (usflibs.length === 0) {
      throw new Error(`No .usflib references found in ${filename}`);
    }

    const dir = path.dirname(filename);
    const fsFilename = path.join(MOUNTPOINT, filename);
    const promises = [
      ensureEmscFileWithData(this.lib, fsFilename, data),
      ...usflibs.map(usflib => {
        const fsFilename = path.join(MOUNTPOINT, dir, usflib);
        const url = CATALOG_PREFIX + path.join(dir, usflib);
        return ensureEmscFileWithUrl(this.lib, fsFilename, url);
      }),
    ];

    Promise.all(promises)
      .then(([fsFilename]) => {
        this.muteAudioDuringCall(this.audioNode, () => {
          err = this.lib.ccall(
            'n64_load_file', 'number',
            ['string', 'number', 'number', 'number'],
            [fsFilename, this.buffer, this.bufferSize, this.audioCtx.sampleRate],
          );

          if (err !== 0) {
            console.error("n64_load_file failed. error code: %d", err);
            throw Error('Unable to load this file!');
          }

          this.metadata = { title: filename };

          this.connect();
          this.resume();
          this.onPlayerStateUpdate(false);
        });
      });
  }

  n64AudioProcess(e) {
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

    const samplesWritten = this.lib._n64_render_audio(this.buffer, this.bufferSize);
    if (samplesWritten <= 0) {
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

  // setTempo(val) {
  //   return this.lib._v2m_set_speed(val);
  //   // console.error('Unable to set speed for this file format.');
  // }

  getPositionMs() {
    return this.lib._n64_get_position_ms();
  }

  getDurationMs() {
    return this.lib._n64_get_duration_ms();
  }

  getMetadata() {
    return this.metadata;
  }

  isPlaying() {
    return !this.isPaused();
  }

  seekMs(seekMs) {
    this.muteAudioDuringCall(this.audioNode, () =>
      this.lib._n64_seek_ms(seekMs)
    );
  }

  stop() {
    this.suspend();
    this.lib._n64_shutdown();
    console.debug('N64Player.stop()');
    this.onPlayerStateUpdate(true);
  }
}
