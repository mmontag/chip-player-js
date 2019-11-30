//
// TinyPlayer
//
// A tiny wrapper for TinyMidiLoader.
// Uses synth engines from libFluidSynth and libADLMIDI.
// Created by Matt Montag on 9/4/18.
//
#define TML_NO_STDIO
#define TML_IMPLEMENTATION

#include <math.h>
#include <emscripten.h>
#include "tml.h"
#include "../fluidlite/include/fluidlite.h"
#include "../libADLMIDI/include/adlmidi.h"

#ifdef __cplusplus
extern "C" {
#endif

//TODO: Remove debug logging (EM_ASM_)
fluid_synth_t *g_FluidSynth; // instance of FluidSynth
struct ADL_MIDIPlayer *g_adlSynth;
struct ADLMIDI_AudioFormat adl_AudioFormat = {
        ADLMIDI_SampleType_F32,
        sizeof(float_t),
        2 * sizeof(float_t),
};
tml_message *g_MidiEvt;      // next message to be played
tml_message *g_FirstEvt;     // first event in current file
double g_MidiTimeMs;         // current playback time
double g_Speed = 1.0;
int g_SampleRate;
unsigned int g_DurationMs = 0;
char g_ChannelsInUse[16];
char g_ChannelsMuted[16];
int g_ChannelProgramNums[16];
short g_shortBuffer[4096];
int tp_set_synth_engine(int);

typedef struct Synth {
  void (*noteOn)(int channel, int key, int velocity);
  void (*noteOff)(int channel, int key);
  void (*programChange)(int channel, int program);
  void (*pitchBend)(int channel, int value);
  void (*controlChange)(int channel, int control, int value);
  void (*channelPressure)(int channel, int value);
  void (*render)(float *buffer, int samples);
  void (*panic)();
  void (*panicChannel)(int channel);
  void (*reset)();
} Synth;

int g_synthId = 0;
static const int NUM_SYNTHS = 2;
Synth g_Synths[NUM_SYNTHS];
Synth g_synth;

// Fluid Synth *********************************************

void fluidNoteOn(int channel, int key, int velocity) {
  fluid_synth_noteon(g_FluidSynth, channel, key, velocity);
}
void fluidNoteOff(int channel, int key) {
  fluid_synth_noteoff(g_FluidSynth, channel, key);
}
void fluidProgramChange(int channel, int program) {
  fluid_synth_program_change(g_FluidSynth, channel, program);
}
void fluidPitchBend(int channel, int pitch) {
  fluid_synth_pitch_bend(g_FluidSynth, channel, pitch);
}
void fluidControlChange(int channel, int control, int value) {
  fluid_synth_cc(g_FluidSynth, channel, control, value);
}
void fluidChannelPressure(int channel, int value) {
  fluid_synth_channel_pressure(g_FluidSynth, channel, value);
}
void fluidRender(float *buffer, int samples) {
  int frames = samples / 2;
  fluid_synth_write_float(g_FluidSynth, frames, buffer, 0, 2, buffer, 1, 2); // interleaved stereo
}
void fluidPanic() {
  for (int i = 0; i < 16; i++)
    fluid_synth_all_notes_off(g_FluidSynth, i);
}
void fluidPanicChannel(int channel) {
  fluid_synth_all_notes_off(g_FluidSynth, channel);
}
void fluidReset() {
  fluid_synth_system_reset(g_FluidSynth);
  fluid_synth_program_reset(g_FluidSynth);
}
Synth fluidSynth = {fluidNoteOn, fluidNoteOff, fluidProgramChange, fluidPitchBend, fluidControlChange,
                    fluidChannelPressure, fluidRender, fluidPanic, fluidPanicChannel, fluidReset};

// ADL OPL3 Synth *********************************************

void adlNoteOn(int channel, int key, int velocity) {
  adl_rt_noteOn(g_adlSynth, channel, key, velocity);
}
void adlNoteOff(int channel, int key) {
  adl_rt_noteOff(g_adlSynth, channel, key);
}
void adlProgramChange(int channel, int program) {
  adl_rt_patchChange(g_adlSynth, channel, program);
}
void adlPitchBend(int channel, int pitch) {
  adl_rt_pitchBend(g_adlSynth, channel, pitch);
}
void adlControlChange(int channel, int control, int value) {
  adl_rt_controllerChange(g_adlSynth, channel, control, value);
}
void adlChannelPressure(int channel, int value) {
  adl_rt_channelAfterTouch(g_adlSynth, channel, value);
}
void adlRender(float *buffer, int samples) {
  adl_generateFormat(g_adlSynth, samples, (ADL_UInt8 *)buffer, (ADL_UInt8 *)(buffer + 1), &adl_AudioFormat);
}
void adlPanic() {
  adl_panic(g_adlSynth);
}
void adlPanicChannel(int channel) {
  // Hack: ADLMIDI doesn't have a channel notes-off method
  for (int i = 0; i < 128; i++)
    adl_rt_noteOff(g_adlSynth, channel, i);
}
void adlReset() {
  adl_rt_resetState(g_adlSynth);
};
Synth adlSynth = {adlNoteOn, adlNoteOff, adlProgramChange, adlPitchBend, adlControlChange,
                  adlChannelPressure, adlRender, adlPanic, adlPanicChannel, adlReset};

// TODO: separate wrapper for each synth?
// Don't want multiple synth C APIs exposed to JavaScript
extern void tp_note_on(int channel, int key, int velocity) {
  g_synth.noteOn(channel, key, velocity);
}
extern void tp_note_off(int channel, int key) {
  g_synth.noteOff(channel, key);
}
extern void tp_program_change(int channel, int program) {
  g_synth.programChange(channel, program);
}
extern void tp_pitch_bend(int channel, int pitch) {
  g_synth.pitchBend(channel, pitch);
}
extern void tp_control_change(int channel, int control, int value) {
  g_synth.controlChange(channel, control, value);
}
extern void tp_channel_pressure(int channel, int value) {
  g_synth.channelPressure(channel, value);
}
extern void tp_render(float *buffer, int samples) {
  g_synth.render(buffer, samples);
}
extern void tp_panic() {
  g_synth.panic();
}
extern void tp_panic_channel(int channel) {
  g_synth.panicChannel(channel);
}
extern void tp_reset() {
  g_synth.reset();
};

// TODO: this will be midi_synth_init
extern void tp_init(int sampleRate) {
  g_SampleRate = sampleRate;

  fluid_settings_t *settings = new_fluid_settings();
  fluid_settings_setstr(settings, "synth.reverb.active", "yes");
  fluid_settings_setstr(settings, "synth.chorus.active", "no");
  fluid_settings_setint(settings, "synth.threadsafe-api", 0);
  fluid_settings_setnum(settings, "synth.gain", 0.5);
  fluid_settings_setnum(settings, "synth.sample-rate", sampleRate);
  g_FluidSynth = new_fluid_synth(settings);
    fluid_synth_set_interp_method(g_FluidSynth, -1, FLUID_INTERP_LINEAR);

  g_adlSynth = adl_init(sampleRate);
  adl_setSoftPanEnabled(g_adlSynth, 1);
  adl_setNumChips(g_adlSynth, 4);

  g_Synths[0] = fluidSynth;
  g_Synths[1] = adlSynth;
  g_synth = g_Synths[0];
}


// Returns the number of bytes written. Value of 0 means the song has ended.
extern int tp_write_audio(float *buffer, int bufferSize) {
  int bytesWritten = 0;
  int batchSize = 128; // Timing of MIDI events will be quantized by the sample batch size.

  double msPerBatch = g_Speed * 1000.0 * (batchSize / (float) g_SampleRate) / 2;
  for (int samplesRemaining = bufferSize * 2; samplesRemaining > 0; samplesRemaining -= batchSize) {
    //We progress the MIDI playback and then process `batchSize` samples at once
    if (batchSize > samplesRemaining) batchSize = samplesRemaining;

    //Loop through all MIDI messages which need to be played up until the current playback time
    for (g_MidiTimeMs += msPerBatch;
         g_MidiEvt && g_MidiTimeMs >= g_MidiEvt->time;
         g_MidiEvt = g_MidiEvt->next) {
      switch (g_MidiEvt->type) {
        case TML_NOTE_ON:
          if (g_ChannelsMuted[g_MidiEvt->channel]) break;
          if (g_MidiEvt->velocity == 0)
            g_synth.noteOff(g_MidiEvt->channel, g_MidiEvt->key);
          else
            g_synth.noteOn(g_MidiEvt->channel, g_MidiEvt->key, g_MidiEvt->velocity);
          break;
        case TML_NOTE_OFF:
          if (g_ChannelsMuted[g_MidiEvt->channel]) break;
          g_synth.noteOff(g_MidiEvt->channel, g_MidiEvt->key);
          break;
        case TML_PROGRAM_CHANGE:
          g_synth.programChange(g_MidiEvt->channel, g_MidiEvt->program);
          break;
        case TML_PITCH_BEND:
          g_synth.pitchBend(g_MidiEvt->channel, g_MidiEvt->pitch_bend);
          break;
        case TML_CONTROL_CHANGE:
          g_synth.controlChange(g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
          break;
        case TML_CHANNEL_PRESSURE:
          g_synth.channelPressure(g_MidiEvt->channel, g_MidiEvt->channel_pressure);
        default:
          break;
      }

    }
    // Render the block of audio samples in float format
    g_synth.render(buffer, batchSize);

    buffer += batchSize;
    bytesWritten += batchSize;
  }

  if (g_MidiEvt == NULL) {
    // Last MIDI event has been processed.
    // Continue synthesis until silence is detected.
    // This allows voices with a long release tail to complete.
    // Crude method: when entire buffer is below threshold, consider it silence.
    int synthStillActive = 0;
    float threshold = 0.05;
    for (int i = 0; i < bufferSize; i++) {
      if (buffer[i * 2] > threshold) { // Check left channel only
        synthStillActive = 1;          // Exit early
        break;
      }
    }
    if (synthStillActive == 0) return 0;
  }

  return bytesWritten;
}

extern unsigned int tp_get_duration_ms() {
  if (g_DurationMs == 0 && g_FirstEvt) {
    tml_get_info(g_FirstEvt, NULL, NULL, NULL, NULL, &g_DurationMs);
  }
  return g_DurationMs;
}

extern void tp_seek(int ms) {
  // It's only possible to seek forward due to the statefulness of the synth.
  // If we need to seek backward, reset to the first event and seek forward from there.
  if (ms < g_MidiTimeMs) {
    g_MidiEvt = g_FirstEvt;
  }

  g_synth.panic();

  while (g_MidiEvt && g_MidiEvt->time < ms) {
    switch (g_MidiEvt->type) {
      // Ignore note on/note off events during seek
      case TML_PROGRAM_CHANGE:
        g_synth.programChange(g_MidiEvt->channel, g_MidiEvt->program);
        break;
      case TML_PITCH_BEND:
        g_synth.pitchBend(g_MidiEvt->channel, g_MidiEvt->pitch_bend);
        break;
      case TML_CONTROL_CHANGE:
        if (g_MidiEvt->control == 91) break; // ignore reverb CC from MIDI files
        g_synth.controlChange(g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
        break;
      default:
        break;
    }
    g_MidiEvt = g_MidiEvt->next;
  }

  g_MidiTimeMs = ms;
}

extern double tp_get_position_ms() {
  return g_MidiTimeMs;
}

extern void tp_set_speed(float speed) {
  g_Speed = fmax(fmin(speed, 10.0), 0.1);
}

extern void tp_stop() {
  g_synth.panic();
  g_MidiEvt = NULL;
}

extern void tp_restart() {
  g_synth.panic();
  g_MidiEvt = g_FirstEvt;
}

extern void tp_open(const void *data, int length) {
  g_synth.reset();
  g_MidiTimeMs = 0;
  g_DurationMs = 0;
  g_MidiEvt = tml_load_memory(data, length);
  g_FirstEvt = g_MidiEvt;
  memset(g_ChannelsInUse, 0, sizeof g_ChannelsInUse);
  memset(g_ChannelsMuted, 0, sizeof g_ChannelsMuted);
  tml_get_channels_in_use_and_initial_programs(g_MidiEvt, g_ChannelsInUse, g_ChannelProgramNums);

  // Skip to first note to eliminate silence
  unsigned int firstNoteTimeMs;
  tml_get_info(g_FirstEvt, NULL, NULL, NULL, &firstNoteTimeMs, NULL);
  g_MidiTimeMs = (double) firstNoteTimeMs;

  EM_ASM_({ console.log('Tiny MIDI Player loaded %d bytes.', $0); }, length);
  EM_ASM_({ console.log('First note appears at %d ms.', $0); }, g_MidiTimeMs);
}

extern void tp_unload_soundfont() {
  if (fluid_synth_sfcount(g_FluidSynth) > 0) {
    fluid_sfont_t *sfont = fluid_synth_get_sfont(g_FluidSynth, 0);
    fluid_synth_remove_sfont(g_FluidSynth, sfont);
    // Causes a crash related to pthreads in Emscripten.
    // fluid_synth_sfunload(g_FluidSynth, (unsigned)g_SoundFontID, 1);
  }
}

extern int tp_load_soundfont(const char *filename) {
  tp_unload_soundfont();
  return fluid_synth_sfload(g_FluidSynth, filename, 1);
}

extern int tp_add_soundfont(const char *filename) {
  return fluid_synth_sfload(g_FluidSynth, filename, 1);
}

extern void tp_set_reverb(double level) {
  // Completely disable reverb at very low levels to improve performance
  fluid_synth_set_reverb_on(g_FluidSynth, level > 0.01);
  fluid_synth_set_reverb(
          g_FluidSynth,
          0.2 + level * 0.8, // roomsize (default: 0.2) 0.2 to 1.0
          0.0 + level * 0.4, // damp     (default: 0.0) 0.0 to 0.4
          1.0,               // width    (default: 0.5) 1.0
          1.0);              // level    (default: 0.9) 1.0
  // Override MIDI channel reverb levels
  for (int i = 0; i < 16; i++)
    fluid_synth_cc(g_FluidSynth, i, 91, 64);
}

extern char tp_get_channel_in_use(int chan) {
  return g_ChannelsInUse[chan] != 0;
}

extern int tp_get_channel_program(int chan) {
  return g_ChannelProgramNums[chan];
}

extern void tp_set_channel_mute(int chan, char isMuted) {
  g_ChannelsMuted[chan] = isMuted;
  if (isMuted) {
    g_synth.panicChannel(chan);
  }
}

extern int tp_set_bank(int bank) {
  return adl_setBank(g_adlSynth, bank);
}

extern int tp_set_synth_engine(int synthId) {
  if (g_synthId == synthId) return 0;
  if (synthId < 0 || synthId > NUM_SYNTHS) return -1;
  g_synth.panic();
  g_synthId = synthId;
  g_synth = g_Synths[g_synthId];
  // restore state
  if (g_MidiEvt != NULL && g_MidiEvt != g_FirstEvt)
    tp_seek((int)tp_get_position_ms() - 1);
  return 0;
}

#ifdef __cplusplus
}
#endif
