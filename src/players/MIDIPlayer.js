import Player from './Player';
import GENERAL_MIDI_PATCH_MAP from '../gm-patch-map';

let lib = null;
const MOUNTPOINT = '/soundfonts';
const SOUNDFONT_URL_PATH = 'soundfonts';
const SOUNDFONTS = [
  {
    label: 'Small Soundfonts',
    items: [
      {label: 'GMGSx (4.1 MB)', value: 'gmgsx.sf2'},
      {label: 'Roland SC-55 (3.3 MB)', value: 'Roland SC-55.sf2'},
      {label: 'Roland SCC1 (3.3 MB)', value: 'Scc1t2.sf2'},
      {label: 'Yamaha DB50XG (3.9 MB)', value: 'Yamaha DB50XG.sf2'},
      {label: 'Tim GM (6 MB)', value: 'TimGM6mb.sf2'},
    ],
  },
  {
    label: 'Large Soundfonts',
    items: [
      {label: 'Masquerade 55 v006 (18.4 MB)', value: 'masquerade55v006.sf2'},
      {label: 'GeneralUser GS v1.471 (31.3 MB)', value: 'generaluser.sf2'},
    ],
  },
  {
    label: 'Novelty Soundfonts',
    items: [
      {label: 'PC Beep (31 KB)', value: 'pcbeep.sf2'},
      {label: 'Kirby\'s Dream Land 3 (271 KB)', value: 'Kirby\'s_Dream_Land_3.sf2'},
      {label: 'Vintage Dreams Waves v2 (315 KB)', value: 'Vintage Dreams Waves v2.sf2'},
      {label: 'Setzer\'s SPC Soundfont (1.2 MB)', value: 'Setzer\'s_SPC_Soundfont.sf2'},
      {label: 'Nokia 30 (2.2 MB)', value: 'Nokia_30.sf2'},
      {label: 'Regression FM v1.99g (14.4 MB)', value: 'R_FM_v1.99g-beta.sf2'},
      {label: 'The Ultimate Megadrive Soundfont (63.2 MB)', value: 'The Ultimate Megadrive Soundfont.sf2'},
    ],
  },
  {
    label: 'SF3 Compressed Soundfonts',
    items: [
      {label: 'Fluid R3 Mono SF3 (14.6 MB)', value: 'FluidR3Mono_GM.sf3'},
      {label: 'MuseScore General SF3 (37.7 MB)', value: 'MuseScore_General.sf3'},
    ],
  },
];

const DEFAULT_SOUNDFONT = SOUNDFONTS[0].items[0].value;
const DEFAULT_REVERB = 0.33;
const fileExtensions = [
  'mid',
  'midi',
];

export default class MIDIPlayer extends Player {
  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.setParameter = this.setParameter.bind(this);
    this.getParameter = this.getParameter.bind(this);
    this.getParameters = this.getParameters.bind(this);

    lib = chipCore;
    lib._tp_init(audioCtx.sampleRate);

    lib.FS.mkdir(MOUNTPOINT);
    lib.FS.mount(lib.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    lib.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
      this.setParameter('soundfont', DEFAULT_SOUNDFONT);
      this.setParameter('reverb', DEFAULT_REVERB);
    });

    this.fileExtensions = fileExtensions;
    this.activeChannels = [];
    this.params = {};
    this.bufferSize = 2048;
    this.buffer = lib.allocate(this.bufferSize * 8, 'i32', lib.ALLOC_NORMAL);

    this.setAudioProcess(this.midiAudioProcess);
  }

  midiAudioProcess(e) {
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

    if (lib._tp_write_audio(this.buffer, this.bufferSize)) {
      for (channel = 0; channel < channels.length; channel++) {
        for (i = 0; i < this.bufferSize; i++) {
          channels[channel][i] = lib.getValue(
            this.buffer +    // Interleaved channel format
            i * 4 * 2 +      // frame offset   * bytes per sample * num channels +
            channel * 4,     // channel offset * bytes per sample
            'float'
          );
        }
      }
    } else {
      console.log('MIDI file ended.');
      lib._tp_stop();
      this.disconnect();
      this.onPlayerStateUpdate(true);
    }
  }

  loadData(data, filepath) {
    this.activeChannels = [];
    this.metadata = Player.metadataFromFilepath(filepath);

    lib.ccall('tp_open', 'number', ['array', 'number'], [data, data.byteLength]);
    for(let i = 0; i < 16; i++) {
      if (lib._tp_get_channel_in_use(i)) this.activeChannels.push(i);
    }

    this.connect();
  }

  isPlaying() {
    return lib && !this.paused;
  }

  restart() {
    lib._tp_seek(0);
    this.resume();
  }

  stop() {
    lib._tp_stop();
    this.disconnect();
    this.onPlayerStateUpdate(true);
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

  getNumVoices() {
    return this.activeChannels.length;
  }

  getVoiceName(index) {
    const channel = this.activeChannels[index];
    if (channel === 9) {
      return 'Drums';
    } else {
      return GENERAL_MIDI_PATCH_MAP[lib._tp_get_channel_program(channel)];
    }
  }

  setVoices(voices) {
    voices.forEach((isEnabled, i) => {
      lib._tp_set_channel_mute(this.activeChannels[i], !isEnabled);
    });
  }

  getMetadata() {
    return this.metadata;
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
    if (lib.FS.analyzePath(filename).exists) {
      console.log(`${filename} exists in Emscripten file system.`);
      return Promise.resolve(filename);
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
    Player.muteAudioDuringCall(this.audioNode, () => {
      const err = lib.ccall('tp_load_soundfont', 'number', ['string'], [filename]);
      if (err !== -1) console.log('Loaded soundfont.');
    });
  }
}