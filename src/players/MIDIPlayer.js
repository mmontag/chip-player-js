import Player from "./Player";

let lib = null;
let synth = null;
const DEFAULT_SOUNDFONT = 'gmgsx.sf2';
const BUFFER_SIZE = 2048;
const fileExtensions = [
  'mid',
  'midi',
];

export default class MIDIPlayer extends Player {
  constructor(audioContext, emscriptenRuntime) {
    super();
    lib = emscriptenRuntime;

    const settings = lib._new_fluid_settings();
    lib.ccall('fluid_settings_setstr', 'number', ['number', 'string', 'string'], [settings, 'synth.reverb.active', 'yes']);
    lib.ccall('fluid_settings_setstr', 'number', ['number', 'string', 'string'], [settings, 'synth.chorus.active', 'no']);
    lib.ccall('fluid_settings_setint', 'number', ['number', 'string', 'number'], [settings, 'synth.threadsafe-api', 0]);
    lib.ccall('fluid_settings_setnum', 'number', ['number', 'string', 'number'], [settings, 'synth.gain', 0.5]);
    synth = lib._new_fluid_synth(settings);

    console.log('Downloading soundfont...');
    const soundfontFile = DEFAULT_SOUNDFONT;
    fetch('soundfonts/' + soundfontFile).then(response => response.arrayBuffer()).then(buffer => {
      const arr = new Uint8Array(buffer);

      console.log('Writing soundfont to Emscripten file system...');
      lib.FS.writeFile(soundfontFile, arr);

      // console.log('Loading soundfont in TinySoundFont...');
      // tsf = lib.ccall('tsf_load_filename', 'number', ['string'], [soundfontFile]);
      // console.log('Created TinySoundFont instance.', tsf);

      console.log('Loading soundfont in FluidSynth...');
      lib.ccall('fluid_synth_sfload', 'number', ['number', 'string', 'number'], [synth, soundfontFile, 1]);
      console.log('Created FluidSynth instance.');

      this.isReady = true;
    });

    this.isReady = false;
    this.audioCtx = audioContext;
    this.audioNode = null;
    this.paused = false;
    this.fileExtensions = fileExtensions;
  }

  loadData(data, endSongCallback) {
    const buffer = lib.allocate(BUFFER_SIZE * 8, 'i32', lib.ALLOC_NORMAL);

    // console.log('Playing MIDI with TinySoundFont...');
    // lib.ccall('tp_open', 'number', ['number', 'array', 'number', 'number'], [tsf, data, data.byteLength, this.audioCtx.sampleRate]);

    console.log('Playing MIDI with FluidSynth...');
    lib.ccall('tp_open_fluidsynth', 'number', ['number', 'array', 'number', 'number'], [synth, data, data.byteLength, this.audioCtx.sampleRate]);

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
          if (typeof endSongCallback === 'function') endSongCallback();
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

  getParameters() {
    return [
      {
        name: 'Soundfont',
        options: [
          'gmgsx.sf2',
          'generaluser.sf2',
          'masquerade55v006.sf2',
          'R_FM_v1.99g-beta.sf2',
          'Scc1t2.sf2',
          'Vintage Dreams Waves v2.sf2',
          'Kirby\'s_Dream_Land_3.sf2',
          'Nokia_30.sf2',
          'Setzer\'s_SPC_Soundfont.sf2',
        ],
      }
    ];
  }

  setParameter(name, value) {
    switch (name) {
      case "Soundfont":
        const soundfontUrl = 'soundfonts/' + value;
        const soundfontFile = value;
        let fileExists = false;
        try {
          lib.FS.stat(soundfontFile);
          fileExists = true;
        } catch (e) {
        }

        if (fileExists) {
          this._loadSoundfont(soundfontFile);
        } else {
          console.log('Downloading soundfont...');
          fetch(soundfontUrl)
            .then(response => response.arrayBuffer())
            .then(buffer => {
              const arr = new Uint8Array(buffer);

              console.log('Writing soundfont to Emscripten file system...');
              lib.FS.writeFile(soundfontFile, arr);

              this._loadSoundfont(soundfontFile);
            });
        }
        break;
      default:
        console.warn('No parameter named "%s".', name);
    }
  }

  _loadSoundfont(soundfontFile) {
    console.log('Loading soundfont in FluidSynth...');
    lib.ccall('fluid_synth_sfload', 'number', ['number', 'string', 'number'], [synth, soundfontFile, 1]);
    console.log('Loaded soundfont.');
  }
}