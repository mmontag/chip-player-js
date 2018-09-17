//
// TinyPlayer
// a tiny wrapper for TinySoundFont and TinyMidiLoader.
// Created by Matt Montag on 9/4/18.
//
#define TML_NO_STDIO
#define TML_IMPLEMENTATION

#include "tml.h"

#define TSF_IMPLEMENTATION

#include "tsf.h"

#include "../fluidlite/include/fluidlite.h"

#include <emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

//TODO: Remove debug logging (EM_ASM_)
tsf *g_TSF;                  // instance of TinySoundFont
fluid_synth_t *g_FluidSynth; // instance of FluidSynth
tml_message *g_MidiEvt;      // next message to be played
tml_message *g_FirstEvt;     // first event in current file
double g_MidiTimeMs;         // current playback time
double g_Speed = 1.0;
int g_SampleRate;
unsigned int g_DurationMs = 0;

// TODO: TinySoundFont not implemented
extern void tp_init(int sampleRate) {
  fluid_settings_t *settings = new_fluid_settings();
  fluid_settings_setstr(settings, "synth.reverb.active", "yes");
  fluid_settings_setstr(settings, "synth.chorus.active", "no");
  fluid_settings_setint(settings, "synth.threadsafe-api", 0);
  fluid_settings_setnum(settings, "synth.gain", 0.5);
  g_FluidSynth = new_fluid_synth(settings);
  g_SampleRate = sampleRate;
}

// Returns the number of bytes written. Value of 0 means the song has ended.
extern int tp_write_audio(float *buffer, int bufferSize) {
  if (g_MidiEvt == NULL) return 0;

  int bytesWritten = 0;
  int batchSize = 128; // Timing of MIDI events will be quantized by the sample batch size.
  int samplesRemaining = bufferSize * 2;
  double msPerBatch = g_Speed * 1000.0 * (batchSize / (float) g_SampleRate) / 2;
  for (; samplesRemaining > 0; samplesRemaining -= batchSize) {
    //We progress the MIDI playback and then process `batchSize` samples at once
    if (batchSize > samplesRemaining) batchSize = samplesRemaining;

    //Loop through all MIDI messages which need to be played up until the current playback time
    for (g_MidiTimeMs += msPerBatch;
         g_MidiEvt && g_MidiTimeMs >= g_MidiEvt->time;
         g_MidiEvt = g_MidiEvt->next) {
      switch (g_MidiEvt->type) {
        case TML_NOTE_ON:
          if (g_FluidSynth) {
            fluid_synth_noteon(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->key, g_MidiEvt->velocity);
          } else {
            tsf_channel_note_on(g_TSF, g_MidiEvt->channel, g_MidiEvt->key, g_MidiEvt->velocity / 127.0f);
          }
          break;
        case TML_NOTE_OFF:
          if (g_FluidSynth) {
            fluid_synth_noteoff(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->key);
          } else {
            tsf_channel_note_off(g_TSF, g_MidiEvt->channel, g_MidiEvt->key);
          }
          break;
        case TML_PROGRAM_CHANGE: // program change (special handling for 10th MIDI channel with drums)
          if (g_FluidSynth) {
            fluid_synth_program_change(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->program);
          } else {
            tsf_channel_set_presetnumber(g_TSF, g_MidiEvt->channel, g_MidiEvt->program, g_MidiEvt->channel == 9);
          }
          break;
        case TML_PITCH_BEND:
          if (g_FluidSynth) {
            fluid_synth_pitch_bend(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
          } else {
            tsf_channel_set_pitchwheel(g_TSF, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
          }
          break;
        case TML_CONTROL_CHANGE:
          if (g_FluidSynth) {
            fluid_synth_cc(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
          } else {
            tsf_channel_midi_control(g_TSF, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
          }
          break;
        default:
          break;
      }

    }
    // Render the block of audio samples in float format
    if (g_FluidSynth) {
      fluid_synth_write_float(g_FluidSynth, batchSize / 2, buffer, 0, 2, buffer, 1, 2); // interleaved stereo
    } else {
      tsf_render_float(g_TSF, buffer, batchSize / 2, 0);
    }

    buffer += batchSize;
    bytesWritten += batchSize;
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

  if (g_FluidSynth) {
    for (int i = 0; i < 16; i++)
      fluid_synth_all_notes_off(g_FluidSynth, i);
  } else {
    tsf_note_off_all(g_TSF);
  }

  while (g_MidiEvt && g_MidiEvt->time < ms) {
    switch (g_MidiEvt->type) {
      // Ignore note on/note off events during seek
      case TML_PROGRAM_CHANGE: // program change (special handling for 10th MIDI channel with drums)
        if (g_FluidSynth) {
          fluid_synth_program_change(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->program);
        } else {
          tsf_channel_set_presetnumber(g_TSF, g_MidiEvt->channel, g_MidiEvt->program, g_MidiEvt->channel == 9);
        }
        break;
      case TML_PITCH_BEND:
        if (g_FluidSynth) {
          fluid_synth_pitch_bend(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
        } else {
          tsf_channel_set_pitchwheel(g_TSF, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
        }
        break;
      case TML_CONTROL_CHANGE:
        if (g_FluidSynth) {
          fluid_synth_cc(g_FluidSynth, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
        } else {
          tsf_channel_midi_control(g_TSF, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
        }
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
  if (g_FluidSynth) {
    for (int i = 0; i < 16; i++)
      fluid_synth_all_notes_off(g_FluidSynth, i);
  } else {
    tsf_reset(g_TSF);
  }
  g_MidiEvt = NULL;
}

extern void tp_restart() {
  if (g_FluidSynth) {
    for (int i = 0; i < 16; i++)
      fluid_synth_all_notes_off(g_FluidSynth, i);
  } else {
    tsf_reset(g_TSF);
  }
  g_MidiEvt = g_FirstEvt;
}

// TODO: TinySoundFont not currently used
extern void tp_open_tsf(tsf *t, void *data, int length, int sampleRate) {
  g_MidiTimeMs = 0;
  g_DurationMs = 0;
  g_TSF = t;
  g_FluidSynth = NULL;
  g_SampleRate = sampleRate;

  tsf_set_output(g_TSF, TSF_STEREO_INTERLEAVED, sampleRate, 0.0);
  // Channel 10 uses percussion sound bank
  tsf_channel_set_bank_preset(g_TSF, 9, 128, 0);
  g_MidiEvt = tml_load_memory(data, length);
  g_FirstEvt = g_MidiEvt;

  // Skip to first note to eliminate silence
  unsigned int firstNoteTimeMs;
  tml_get_info(g_FirstEvt, NULL, NULL, NULL, &firstNoteTimeMs, NULL);
  g_MidiTimeMs = (double)firstNoteTimeMs;

  EM_ASM_({ console.log('Tiny MIDI Player loaded %d bytes.', $0); }, length);
  EM_ASM_({ console.log('First note appears at %d ms.', $0); }, g_MidiTimeMs);
}

extern void tp_open(void *data, int length) {
  g_MidiTimeMs = 0;
  g_DurationMs = 0;
  fluid_synth_system_reset(g_FluidSynth);
  g_MidiEvt = tml_load_memory(data, length);
  g_FirstEvt = g_MidiEvt;

  // Skip to first note to eliminate silence
  unsigned int firstNoteTimeMs;
  tml_get_info(g_FirstEvt, NULL, NULL, NULL, &firstNoteTimeMs, NULL);
  g_MidiTimeMs = (double)firstNoteTimeMs;

  EM_ASM_({ console.log('Tiny MIDI Player loaded %d bytes.', $0); }, length);
  EM_ASM_({ console.log('First note appears at %d ms.', $0); }, g_MidiTimeMs);
}

// TODO: TinySoundFont not implemented
extern int tp_load_soundfont(char *filename) {
  return fluid_synth_sfload(g_FluidSynth, filename, 1);
}

#ifdef __cplusplus
}
#endif