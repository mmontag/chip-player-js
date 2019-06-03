import Player from './Player';
import GENERAL_MIDI_PATCH_MAP from '../gm-patch-map';

let lib = null;
const MOUNTPOINT = '/soundfonts';
const SOUNDFONT_URL_PATH = `${process.env.PUBLIC_URL}/soundfonts`;
const SOUNDFONTS = [
  {
    label: 'Small Soundfonts',
    items: [
      {label: 'GMGSx Plus (6.2 MB)', value: 'gmgsx-plus.sf2'},
      {label: 'Roland SC-55 (3.3 MB)', value: 'Roland SC-55.sf2'},
      {label: 'Roland SCC1 (3.3 MB)', value: 'Scc1t2.sf2'},
      {label: 'Yamaha DB50XG (3.9 MB)', value: 'Yamaha DB50XG.sf2'},
      {label: 'Tim GM (6 MB)', value: 'TimGM6mb.sf2'},
      {label: '2MBGMGS (2.1 MB)', value: '2MBGMGS.SF2'},
      {label: '5MBGMGS (4.9 MB)', value: '5MBGMGS.SF2'},
      {label: '8MBGMGS (8.2 MB)', value: '8MBGMGS.SF2'},
    ],
  },
  {
    label: 'Large Soundfonts',
    items: [
      {label: 'Masquerade 55 v006 (18.4 MB)', value: 'masquerade55v006.sf2'},
      {label: 'GeneralUser GS v1.471 (31.3 MB)', value: 'generaluser.sf2'},
      {label: 'Chorium Revision A (28.9 MB)', value: 'choriumreva.sf2'},
      {label: 'Unison (29.3 MB)', value: 'Unison.SF2'},
      {label: 'Musica Theoria 2 (30.5 MB)', value: 'mustheory2.sf2'},
      {label: 'Personal Copy Lite (31.4 MB)', value: 'PCLite.sf2'},
      {label: 'NTONYX 32Mb GM Stereo (32.5 MB)', value: '32MbGMStereo.sf2'},
      {label: 'Weeds GM 3 (54.9 MB)', value: 'weedsgm3.sf2'},
    ],
  },
  {
    label: 'Novelty Soundfonts',
    items: [
      {label: 'PC Beep (31 KB)', value: 'pcbeep.sf2'},
      {label: 'Nokia 6230i (227 KB)', value: 'Nokia_6230i_RM-72_.sf2'},
      {label: 'Kirby\'s Dream Land (271 KB)', value: 'Kirby\'s_Dream_Land_3.sf2'},
      {label: 'Vintage Waves v2 (315 KB)', value: 'Vintage Dreams Waves v2.sf2'},
      {label: 'Setzer\'s SPC Soundfont (1.2 MB)', value: 'Setzer\'s_SPC_Soundfont.sf2'},
      {label: 'SNES GM (1.9 MB)', value: 'Super_Nintendo_Unofficial_update.sf2'},
      {label: 'Nokia 30 (2.2 MB)', value: 'Nokia_30.sf2'},
      {label: 'LG Wink/Motorola ROKR (3.3 MB)', value: 'LG_Wink_Style_T310_Soundfont.sf2'},
      {label: 'Kururin Paradise GM (7.6 MB)', value: 'Kururin_Paradise_GM_Soundfont.sf2'},
      {label: 'Diddy Kong Racing DS (13.7 MB)', value: 'Diddy_Kong_Racing_DS_Soundfont.sf2'},
      {label: 'Regression FM v1.99g (14.4 MB)', value: 'R_FM_v1.99g-beta.sf2'},
      {label: 'Ultimate Megadrive (63.2 MB)', value: 'The Ultimate Megadrive Soundfont.sf2'},
    ],
  },
  {
    label: 'SF3 Compressed Soundfonts',
    items: [
      {label: 'Fluid R3 Mono SF3 (14.6 MB)', value: 'FluidR3Mono_GM.sf3'},
      {label: 'MuseScore SF3 (37.7 MB)', value: 'MuseScore_General.sf3'},
    ],
  },
];

const fileExtensions = [
  'mid',
  'midi',
];

export default class MIDIPlayer extends Player {
  paramDefs = [
    {
      id: 'synthengine',
      label: 'Synth Engine',
      type: 'enum',
      options: [{
        label: 'MIDI Synthesis Engine',
        items: [
          {label: 'SoundFont (libFluidLite)', value: 0},
          {label: 'Adlib/OPL3 (libADLMIDI)', value: 1},
        ],
      }],
      defaultValue: 0,
    },
    {
      id: 'autoengine',
      label: 'Auto Synth Engine Switching',
      hint: 'Switch synth engine based on filenames. Files containing "FM" will play through Adlib/OPL3 synth.',
      type: 'toggle',
      defaultValue: true,
    },
    {
      id: 'soundfont',
      label: 'Soundfont',
      type: 'enum',
      options: SOUNDFONTS,
      defaultValue: SOUNDFONTS[0].items[0].value,
      dependsOn: {
        param: 'synthengine',
        value: 0,
      },
    },
    {
      id: 'reverb',
      label: 'Reverb',
      type: 'number',
      min: 0.0,
      max: 1.0,
      step: 0.01,
      defaultValue: 0.33,
      dependsOn: {
        param: 'synthengine',
        value: 0,
      },
    },
    {
      id: 'opl3bank',
      label: 'OPL3 Bank',
      type: 'enum',
      options: [],
      defaultValue: 0,
      dependsOn: {
        param: 'synthengine',
        value: 1,
      },
    },
  ];

  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.setParameter = this.setParameter.bind(this);
    this.getParameter = this.getParameter.bind(this);
    this.getParamDefs = this.getParamDefs.bind(this);

    lib = chipCore;
    lib._tp_init(audioCtx.sampleRate);

    lib.FS.mkdir(MOUNTPOINT);
    lib.FS.mount(lib.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    lib.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
      // Wait until filesystem is mounted to initialize parameters
      this.paramDefs.forEach(param => this.setParameter(param.id, param.defaultValue));
    });

    this.fileExtensions = fileExtensions;
    this.activeChannels = [];
    this.params = {};
    this.buffer = lib.allocate(this.bufferSize * 8, 'i32', lib.ALLOC_NORMAL);
    this.filepathMeta = {};

    // Populate OPL3 banks
    const numBanks = lib._adl_getBanksCount();
    const ptr = lib._adl_getBankNames();
    const oplBanks = [];
    for (let i = 0; i < numBanks; i++) {
      oplBanks.push({
        label: lib.UTF8ToString(lib.getValue(ptr + i * 4, '*')),
        value: i,
      });
    }
    this.paramDefs.find(param => param.id === 'opl3bank').options =
      [{ label: 'OPL3 Bank', items: oplBanks }];

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
      this.stop();
    }
  }

  metadataFromFilepath(filepath) {
    const parts = filepath.split('/');
    const len = parts.length;
    const meta = {};
    if (parts.length >= 3) {
      meta.formatted = {
        title: `${parts[1]} - ${parts[len - 1]}`,
        subtitle: parts[0],
      };
    } else if (parts.length === 2) {
      meta.formatted = {
        title: parts[1],
        subtitle: parts[0],
      }
    } else {
      meta.formatted = {
        title: parts[0],
        subtitle: 'MIDI',
      }
    }
    return meta;
  }

  loadData(data, filepath) {
    this.activeChannels = [];
    this.filepathMeta = this.metadataFromFilepath(filepath);

    lib.ccall('tp_open', 'number', ['array', 'number'], [data, data.byteLength]);
    for(let i = 0; i < 16; i++) {
      if (lib._tp_get_channel_in_use(i)) this.activeChannels.push(i);
    }

    // Switch to OPL3 engine if filepath contains 'FM'
    if (this.getParameter('autoengine')) {
      if (filepath.match(/FM\b/i)) {
        this.setParameter('synthengine', 1);
      } else {
        this.setParameter('synthengine', 0);
      }
    }

    this.connect();
    this.resume();
    this.onPlayerStateUpdate(false);
  }

  isPlaying() {
    return lib && !this.paused;
  }

  restart() {
    lib._tp_seek(0);
    this.resume();
  }

  stop() {
    this.suspend();
    lib._tp_stop();
    console.debug('MIDIPlayer.stop()');
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
    return this.filepathMeta;
  }

  getParameter(id) {
    return this.params[id];
  }

  getParamDefs() {
    return this.paramDefs;
  }

  setParameter(id, value) {
    switch (id) {
      case 'synthengine':
        value = parseInt(value, 10);
        lib._tp_set_synth_engine(value);
        break;
      case 'soundfont':
        const url = `${SOUNDFONT_URL_PATH}/${value}`;
        this._ensureFile(`${MOUNTPOINT}/${value}`, url)
          .then(filename => this._loadSoundfont(filename));
        break;
      case 'reverb':
        value = parseFloat(value);
        lib._tp_set_reverb(value);
        break;
      case 'opl3bank':
        value = parseInt(value, 10);
        lib._tp_set_bank(value);
        break;
      case 'autoengine':
        value = !!value;
        break;
      default:
        console.warn('MIDIPlayer has no parameter with id "%s".', id);
    }
    this.params[id] = value;
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
    this.muteAudioDuringCall(this.audioNode, () => {
      const err = lib.ccall('tp_load_soundfont', 'number', ['string'], [filename]);
      if (err !== -1) console.log('Loaded soundfont.');
    });
  }
}