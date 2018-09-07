import Player from "./Player";

let lib = null;
let tsf = null;
let synth = null;
let player = null;
// const SOUNDFONT_URL = 'generaluser.sf2';
const SOUNDFONT_URL = 'gmgsx.sf2';
const BUFFER_SIZE = 2048;
// const FLUID_PLAYER_PLAYING = 1;
// const ALL_NOTES_OFF = 0x7B;
const fileExtensions = [
  'mid',
  'midi',
];

export default class MIDIPlayer extends Player {
  constructor(audioContext, emscriptenRuntime) {
    super();
    lib = emscriptenRuntime;

    // const settings = lib._new_fluid_settings();
    // lib.ccall('fluid_settings_setint', 'number', ['number', 'string', 'number'], [settings, 'synth.polyphony', 128]);
    // lib.ccall('fluid_settings_setint', 'number', ['number', 'string', 'number'], [settings, 'synth.reverb.active', 1]);
    // lib.ccall('fluid_settings_setint', 'number', ['number', 'string', 'number'], [settings, 'synth.chorus.active', 0]);
    // lib.ccall('fluid_settings_setint', 'number', ['number', 'string', 'number'], [settings, 'synth.threadsafe-api', 0]);
    // lib.ccall('fluid_settings_setnum', 'number', ['number', 'string', 'number'], [settings, 'synth.gain', 0.5]);
    //
    // synth = lib._new_fluid_synth(settings);
    // player = lib._new_fluid_player(synth);

    console.log('Downloading soundfont...');

    fetch(SOUNDFONT_URL).then(response => response.arrayBuffer()).then(buffer => {
      const arr = new Uint8Array(buffer);

      // tsf = lib.ccall('tsf_load_memory', 'number', ['array', 'number'], [arr, arr.byteLength]);

      console.log('Writing soundfont to FS...');
      const soundfontFile = 'soundfont.sf2';
      lib.FS.writeFile(soundfontFile, arr);
      console.log('Loading soundfont from FS...');
      tsf = lib.ccall('tsf_load_filename', 'number', ['string'], [soundfontFile]);

      console.log('created TinySoundFont', tsf);
      // lib.ccall('fluid_synth_sfload', 'number', ['number', 'string', 'number'], [synth, soundfontFile, 1]);

      this.isReady = true;
    });

    this.isReady = false;
    this.audioCtx = audioContext;
    this.audioNode = null;
    this.paused = false;
    this.bpm = 180;
    this.ppq = 96;
    this.miditempo = 0;
    this.fileExtensions = fileExtensions;
  }

  _fluidMidiPlay(data) {
    // MIDI playback with FluidSynth
    console.log('Writing file.mid to FS...');
    const midiFile = 'file.mid';
    lib.FS.writeFile(midiFile, data);
    console.log('Loading file.mid from FS...');

    // TODO: fix me.
    lib.ccall('fluid_player_add', 'number', ['number', 'string'], [player, midiFile]);

    const metadata = {
      bpm: lib._fluid_player_get_bpm(player),
      ticks: lib._fluid_player_get_total_ticks(player),
      miditempo: lib._fluid_player_get_midi_tempo(player),
    };

    this.miditempo = lib._fluid_player_get_midi_tempo(player);

    console.log('metadata:', metadata);
    this.metadata = metadata;

    lib._fluid_player_play(player);

    if (!this.audioNode) {
      const bufferL = lib.allocate(BUFFER_SIZE * 16, 'float', lib.ALLOC_NORMAL);
      const bufferR = lib.allocate(BUFFER_SIZE * 16, 'float', lib.ALLOC_NORMAL);
      const buffers = [bufferL, bufferR];
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

        // http://fluidsynth.sourceforge.net/api/synth_8h.html#ac86a79a943fc5d5d485ccc5a5fcad03d
        lib._fluid_synth_write_float(synth, BUFFER_SIZE, bufferL, 0, 1, bufferR, 0, 1);

        for (channel = 0; channel < channels.length; channel++) {
          for (i = 0; i < BUFFER_SIZE; i++) {
            channels[channel][i] = lib.getValue(
              buffers[channel] +
              i * 4,                    // frame offset * bytes per sample
              'float'                   // the sample values are floating point
            );
          }
        }
      };
    }
  }

  _tmlMidiPlay(data) {
    // const tml = lib._tml_load_memory(data, data.byteLength);
    const buffer = lib.allocate(BUFFER_SIZE * 8, 'i32', lib.ALLOC_NORMAL);

    // lib._tp_open(tsf, data, data.byteLength);
    lib.ccall('tp_open', 'number', ['number', 'array', 'number', 'number'], [tsf, data, data.byteLength, this.audioCtx.sampleRate]);

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

        lib._tp_write_audio(buffer, BUFFER_SIZE);
        // lib.ccall('tp_write_audio', ['array', 'number'], [buffer, BUFFER_SIZE]);

        for (channel = 0; channel < channels.length; channel++) {
          for (i = 0; i < BUFFER_SIZE; i++) {
            channels[channel][i] = lib.getValue(
              buffer +                // Interleaved channel format
              i * 4 * 2 +             // frame offset   * bytes per sample * num channels +
              channel * 4,            // channel offset * bytes per sample
              'float'
            );
          }
        }
      };
    }
  }

  loadData(data) {
    this._tmlMidiPlay(data);
  }

  isPlaying() {
    // return lib._fluid_player_get_status(player) === FLUID_PLAYER_PLAYING && this.paused === false;
    return lib && this.audioNode && !this.paused;
  }

  _ticksToMs(ticks) {
    const msPerTick = (this.bpm * this.ppq) / 60000;
    return ticks * msPerTick;
  }

  _msToTicks(ms) {
    const ticksPerMs = 60000 / (this.bpm * this.ppq);
    return ms * ticksPerMs;
  }

  _panic() {
    // Broadcast note off events on all channels
    // lib._fluid_synth_all_notes_off(synth, -1);
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

    // lib._fluid_player_stop(player);
    // How is this not part of fluid_player_stop?
    // this._panic();
  }

  getDurationMs() {
    // return this._ticksToMs(lib._fluid_player_get_total_ticks(player));
    return lib._tp_get_duration_ms();
  }

  getPositionMs() {
    // ticks to ms
    // const ticks = lib._fluid_player_get_current_tick(player);
    // return Math.max(0, Math.floor(this._ticksToMs(ticks)));
    return lib._tp_get_position_ms();
  }

  seekMs(ms) {
    // Why isn't this incorporated in fluid_player_seek?
    // this._panic();
    // ms to ticks
    // const ticks = Math.max(0, Math.floor(this._msToTicks(ms)));
    // lib._fluid_player_seek(player, ticks);
    lib._tp_seek(ms);
  }

  setTempo(tempo) {
    lib._tp_set_speed(tempo);

    // lib._fluid_player_set_midi_tempo(player, Math.floor(this.miditempo / tempo));
  }
}