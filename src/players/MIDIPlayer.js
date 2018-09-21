import Player from "./Player";

let lib = null;
const MOUNTPOINT = '/soundfonts';
const SOUNDFONT_URL_PATH = 'soundfonts';
const SOUNDFONTS = [
  'gmgsx.sf2',
  'generaluser.sf2',
  'masquerade55v006.sf2',
  'R_FM_v1.99g-beta.sf2',
  'Scc1t2.sf2',
  'Vintage Dreams Waves v2.sf2',
  'Kirby\'s_Dream_Land_3.sf2',
  'Nokia_30.sf2',
  'Setzer\'s_SPC_Soundfont.sf2',
];
const DEFAULT_SOUNDFONT = SOUNDFONTS[0];
const DEFAULT_REVERB = 0.65;
const BUFFER_SIZE = 2048;
const fileExtensions = [
  'mid',
  'midi',
];

export default class MIDIPlayer extends Player {
  constructor(audioContext, emscriptenRuntime) {
    super();
    lib = emscriptenRuntime;
    lib._tp_init(audioContext.sampleRate);

    lib.FS.mkdir(MOUNTPOINT);
    lib.FS.mount(lib.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    lib.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
      this.setParameter('soundfont', DEFAULT_SOUNDFONT);
      this.setParameter('reverb', DEFAULT_REVERB);
    });

    this.audioCtx = audioContext;
    this.audioNode = null;
    this.paused = false;
    this.fileExtensions = fileExtensions;
    this.params = {};
  }

  loadData(data, endSongCallback = function() {}) {
    const buffer = lib.allocate(BUFFER_SIZE * 8, 'i32', lib.ALLOC_NORMAL);

    console.log('Playing MIDI data...');
    lib.ccall('tp_open', 'number', ['array', 'number'], [data, data.byteLength]);

    if (!this.audioNode) {
      this.audioNode = this.audioCtx.createScriptProcessor(BUFFER_SIZE, 2, 2);
      this.audioNode.connect(this.audioCtx.destination);
      this.audioNode.onaudioprocess = (e) => {
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

        if (lib._tp_write_audio(buffer, BUFFER_SIZE)) {
          for (channel = 0; channel < channels.length; channel++) {
            for (i = 0; i < BUFFER_SIZE; i++) {
              channels[channel][i] = lib.getValue(
                buffer +         // Interleaved channel format
                i * 4 * 2 +      // frame offset   * bytes per sample * num channels +
                channel * 4,     // channel offset * bytes per sample
                'float'
              );
            }
          }
        } else {
          console.log('MIDI file ended.');
          lib._tp_stop();
          this.audioNode.disconnect();
          this.audioNode = null;

          endSongCallback(true);
        }
      };
    }
  }

  isPlaying() {
    return lib && this.audioNode && !this.paused;
  }

  restart() {
    lib._tp_seek(0);
    this.resume();
  }

  stop() {
    lib._tp_stop();
    if (this.audioNode) {
      this.audioNode.disconnect();
      this.audioNode = null;
    }
  }

  getDurationMs() {
    return lib._tp_get_duration_ms();
  }

  getPositionMs() {
    return lib._tp_get_position_ms();
  }

  seekMs(ms) {
    lib._tp_seek(ms);
  }

  setTempo(tempo) {
    lib._tp_set_speed(tempo);
  }

  getParameter(id) {
    return this.params[id];
  }

  getParameters() {
    return [
      {
        id: 'soundfont',
        label: 'Soundfont',
        type: 'enum',
        options: SOUNDFONTS,
        value: DEFAULT_SOUNDFONT,
      },
      {
        id: 'reverb',
        label: 'Reverb',
        type: 'number',
        min: 0.0,
        max: 1.0,
        step: 0.01,
        value: 0.6,
      },
    ];
  }

  setParameter(id, value) {
    switch (id) {
      case 'soundfont':
        const url = `${SOUNDFONT_URL_PATH}/${value}`;
        this._ensureFile(`${MOUNTPOINT}/${value}`, url)
          .then(filename => this._loadSoundfont(filename));
        this.params[id] = value;
        break;
      case 'reverb':
        lib._tp_set_reverb(value);
        this.params[id] = value;
        break;
      default:
        console.warn('MIDIPlayer has no parameter with id "%s".', id);
    }
  }

  _ensureFile(filename, url) {
    let fileExists = false;
    try {
      lib.FS.stat(filename);
      console.log(`${filename} exists in Emscripten file system.`);
      fileExists = true;
    } catch (e) {
    }

    if (fileExists) {
      return Promise.resolve(filename);
      // this._loadSoundfont(filename);
    } else {
      console.log(`Downloading ${filename}...`);
      return fetch(url)
        .then(response => response.arrayBuffer())
        .then(buffer => {
          const arr = new Uint8Array(buffer);
          console.log(`Writing ${filename} to Emscripten file system...`);
          lib.FS.writeFile(filename, arr);
          lib.FS.syncfs(false, (err) => {
            if (err) {
              console.log('Error synchronizing to indexeddb.', err);
            } else {
              console.log(`Synchronized ${filename} to indexeddb.`);
            }
          });
          return filename;
        });
    }
  }

  _loadSoundfont(filename) {
    console.log('Loading soundfont...');
    const err = lib.ccall('tp_load_soundfont', 'number', ['string'], [filename]);
    if (err !== -1) console.log('Loaded soundfont.');
  }
}