import MIDIFile from 'midifile';
import MIDIFilePlayer from './MIDIFilePlayer';

import Player from './Player';
import GENERAL_MIDI_PATCH_MAP from '../gm-patch-map';
import { SOUNDFONT_URL_PATH } from '../config';

let lib = null;
const MOUNTPOINT = '/soundfonts';
const SOUNDFONTS = [
  {
    label: 'Small Soundfonts',
    items: [
      {label: 'GMGSx Plus (6.2 MB)', value: 'gmgsx-plus.sf2'},
      {label: 'Roland SC-55/SCC1 (3.3 MB)', value: 'Scc1t2.sf2'},
      {label: 'Yamaha DB50XG (3.9 MB)', value: 'Yamaha DB50XG.sf2'},
      {label: 'Gravis Ultrasound (5.9 MB)', value: 'Gravis_Ultrasound_Classic_PachSet_v1.6.sf2'},
      {label: 'Tim GM (6 MB)', value: 'TimGM6mb.sf2'},
      {label: 'Alan Chan 5MBGMGS (4.9 MB)', value: '5MBGMGS.SF2'},
      {label: 'E-mu 2MBGMGS (2.1 MB)', value: '2MBGMGS.SF2'},
      {label: 'E-mu 8MBGMGS (8.2 MB)', value: '8MBGMGS.SF2'},
    ],
  },
  {
    label: 'Large Soundfonts',
    items: [
      {label: 'Masquerade 55 v006 (18.4 MB)', value: 'masquerade55v006.sf2'},
      {label: 'GeneralUser GS v1.471 (31.3 MB)', value: 'generaluser.sf2'},
      {label: 'Chorium Revision A (28.9 MB)', value: 'choriumreva.sf2'},
      {label: 'Unison (29.3 MB)', value: 'Unison.SF2'},
      {label: 'Creative 28MBGM (29.7 MB)', value: '28MBGM.sf2'},
      {label: 'Musica Theoria 2 (30.5 MB)', value: 'mustheory2.sf2'},
      {label: 'Personal Copy Lite (31.4 MB)', value: 'PCLite.sf2'},
      {label: 'AnotherXG (31.4 MB)', value: 'bennetng_AnotherXG_v2-1.sf2'},
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
];

const dummyMidiOutput = { send: () => {} };

const midiDevices = [
  dummyMidiOutput,
];

const fileExtensions = [
  'mid',
  'midi',
];

const MIDI_ENGINE_LIBFLUIDLITE = 0;
const MIDI_ENGINE_LIBADLMIDI = 1;
const MIDI_ENGINE_WEBMIDI = 2;

export default class MIDIPlayer extends Player {
  paramDefs = [
    {
      id: 'synthengine',
      label: 'Synth Engine',
      type: 'enum',
      options: [{
        label: 'MIDI Synthesis Engine',
        items: [
          {label: 'SoundFont (libFluidLite)', value: MIDI_ENGINE_LIBFLUIDLITE},
          {label: 'Adlib/OPL3 FM (libADLMIDI)', value: MIDI_ENGINE_LIBADLMIDI},
          {label: 'MIDI Device (Web MIDI)', value: MIDI_ENGINE_WEBMIDI},
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
        value: MIDI_ENGINE_LIBFLUIDLITE,
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
        value: MIDI_ENGINE_LIBFLUIDLITE,
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
        value: MIDI_ENGINE_LIBADLMIDI,
      },
    },
    {
      id: 'mididevice',
      label: 'MIDI Device',
      type: 'enum',
      options: [{
        label: 'MIDI Output Devices',
        items: [{ label: 'Dummy device', value: 0 }],
      }],
      defaultValue: 0,
      dependsOn: {
        param: 'synthengine',
        value: MIDI_ENGINE_WEBMIDI,
      },
    },
  ];

  constructor(audioCtx, destNode, chipCore, onPlayerStateUpdate = function() {}) {
    super(audioCtx, destNode, chipCore, onPlayerStateUpdate);
    this.setParameter = this.setParameter.bind(this);
    this.getParameter = this.getParameter.bind(this);
    this.getParamDefs = this.getParamDefs.bind(this);
    this.switchSynthBasedOnFilename = this.switchSynthBasedOnFilename.bind(this);

    lib = chipCore;
    lib._tp_init(audioCtx.sampleRate);
    this.sampleRate = audioCtx.sampleRate;

    // Initialize Soundfont filesystem
    lib.FS.mkdir(MOUNTPOINT);
    lib.FS.mount(lib.FS.filesystems.IDBFS, {}, MOUNTPOINT);
    lib.FS.syncfs(true, (err) => {
      if (err) {
        console.log('Error populating FS from indexeddb.', err);
      }
    });

    this.fileExtensions = fileExtensions;
    this.activeChannels = [];
    this.buffer = lib.allocate(this.bufferSize * 8, 'i32', lib.ALLOC_NORMAL);
    this.filepathMeta = {};
    this.midiFilePlayer = new MIDIFilePlayer({
      output: dummyMidiOutput,
      skipSilence: true,
      synth: {
        noteOn: lib._tp_note_on,
        noteOff: lib._tp_note_off,
        pitchBend: lib._tp_pitch_bend,
        controlChange: lib._tp_control_change,
        programChange: lib._tp_program_change,
        panic: lib._tp_panic,
        panicChannel: lib._tp_panic_channel,
        render: lib._tp_render,
        reset: lib._tp_reset,
      },
    });

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
    this.paramDefs.find(def => def.id === 'opl3bank').options =
      [{ label: 'OPL3 Bank', items: oplBanks }];

    // Initialize MIDI output devices
    if (typeof navigator.requestMIDIAccess === 'function') {
      navigator.requestMIDIAccess({ sysex: true }).then((access) => {
        if (access.outputs.length === 0) {
          console.warn('No MIDI output devices found.');
        } else {
          [...access.outputs.values()].forEach(midiOutput => {
            console.log('MIDI Output:', midiOutput);
            midiDevices.push(midiOutput);
            this.paramDefs.find(def => def.id === 'mididevice').options[0].items.push({
              label: midiOutput.name,
              value: midiDevices.length - 1,
            });
          });

          // TODO: remove if removing Dummy Device
          this.setParameter('mididevice', 1);
        }
      });
    } else {
      console.warn('Web MIDI API not supported. Try Chrome if you want to use external MIDI output devices.');
    }

    // this.midiFilePlayer = new MIDIFilePlayer({ output: dummyMidiOutput });

    // Initialize parameters
    this.params = {};
    this.paramDefs.forEach(param => this.setParameter(param.id, param.defaultValue));

    this.setAudioProcess(this.midiAudioProcess);
  }

  midiAudioProcess(e) {
    let i, channel;
    const channels = [];

    for (channel = 0; channel < e.outputBuffer.numberOfChannels; channel++) {
      channels[channel] = e.outputBuffer.getChannelData(channel);
    }

    const useWebMIDI = this.params['synthengine'] === MIDI_ENGINE_WEBMIDI;

    if (this.midiFilePlayer.paused || useWebMIDI) {
      for (channel = 0; channel < channels.length; channel++) {
        channels[channel].fill(0);
      }
    }

    if (useWebMIDI) {
      this.midiFilePlayer.processPlay();
    } else {
      if (this.midiFilePlayer.processPlaySynth(this.buffer, this.bufferSize)) {
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
    this.filepathMeta = this.metadataFromFilepath(filepath);

    if (this.getParameter('autoengine')) {
      this.switchSynthBasedOnFilename(filepath);
    }

    const midiFile = new MIDIFile(data);
    this.midiFilePlayer.load(midiFile);
    this.midiFilePlayer.play(() => this.onPlayerStateUpdate(true));

    this.activeChannels = [];
    for (let i = 0; i < 16; i++) {
      if (this.midiFilePlayer.getChannelInUse(i)) this.activeChannels.push(i);
    }

    this.connect();
    this.resume();
    this.onPlayerStateUpdate(false);
  }

  switchSynthBasedOnFilename(filepath) {
    // Switch to OPL3 engine if filepath contains 'FM'
    const fp = filepath.toLowerCase().replace('_', ' ');
    if (fp.match(/(\bfm|fm\b)/i)) {
      this.setParameter('synthengine', MIDI_ENGINE_LIBADLMIDI);
    } else {
      // this.setParameter('synthengine', MIDI_ENGINE_LIBFLUIDLITE);
    }

    // Crude bank matching for a few specific games. :D
    const opl3def = this.paramDefs.find(def => def.id === 'opl3bank');
    if (opl3def) {
      const opl3banks = opl3def.options[0].items;
      const findBank = (str) => opl3banks.findIndex(bank => bank.label.indexOf(str) > -1);
      let bankId = 0;
      if (fp.indexOf('[rick]') > -1) {
        bankId = findBank('Descent: Rick');
      } else if (fp.indexOf('[ham]') > -1) {
        bankId = findBank('Descent: Ham');
      } else if (fp.indexOf('[int]') > -1) {
        bankId = findBank('Descent: Int');
      } else if (fp.indexOf('descent 2') > -1) {
        bankId = findBank('Descent 2');
      } else if (fp.indexOf('magic carpet') > -1) {
        bankId = findBank('Magic Carpet');
      } else if (fp.indexOf('wacky wheels') > -1) {
        bankId = findBank('Apogee IMF');
      } else if (fp.indexOf('warcraft 2') > -1) {
        bankId = findBank('Warcraft 2');
      } else if (fp.indexOf('warcraft') > -1) {
        bankId = findBank('Warcraft');
      } else if (fp.indexOf('system shock') > -1) {
        bankId = findBank('System Shock');
      }
      if (bankId > -1) {
        this.setParameter('opl3bank', bankId);
      }
    }
  }

  isPlaying() {
    return !this.midiFilePlayer.paused;
  }

  suspend() {
    super.suspend();
    this.midiFilePlayer.stop();
  }

  stop() {
    this.suspend();
    console.debug('MIDIPlayer.stop()');
    this.onPlayerStateUpdate(true);
  }

  togglePause() {
    return this.midiFilePlayer.togglePause();
  }

  getDurationMs() {
    return this.midiFilePlayer.getDuration();
  }

  getPositionMs() {
    return this.midiFilePlayer.getPosition();
  }

  seekMs(ms) {
    return this.midiFilePlayer.setPosition(ms);
  }

  setTempo(tempo) {
    this.midiFilePlayer.setSpeed(tempo);
  }

  getNumVoices() {
    return this.activeChannels.length;
  }

  getVoiceName(index) {
    const channel = this.activeChannels[index];
    if (channel === 9) {
      return 'Drums';
    } else {
      return GENERAL_MIDI_PATCH_MAP[this.midiFilePlayer.getChannelProgramNum(channel)];
    }
  }

  setVoices(voices) {
    voices.forEach((isEnabled, i) => {
      this.midiFilePlayer.setChannelMute(this.activeChannels[i], !isEnabled);
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
        this.midiFilePlayer.panic();
        if (value === MIDI_ENGINE_WEBMIDI) {
          this.midiFilePlayer.setUseWebMIDI(true);
        } else {
          this.midiFilePlayer.setUseWebMIDI(false);
          lib._tp_set_synth_engine(value);
        }
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
      case 'mididevice':
        this.midiFilePlayer.setOutput(midiDevices[value]);
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
