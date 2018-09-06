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

#include <emscripten.h>

#ifdef __cplusplus
extern "C" {
#endif

//TODO: Remove debug logging (EM_ASM_)

tsf *g_TSF;                  //instance of TinySoundFont
tml_message *g_MidiEvt;      //next message to be played
tml_message *g_FirstEvt;     //first event in current file
double g_MidiTimeMs;         //current playback time
double g_Speed = 1.0;
int g_SampleRate;
unsigned int g_DurationMs = 0;

extern void tp_write_audio(float *buffer, int bufferSize) {
  if (g_MidiEvt == NULL) return;
  int batchSize = 128; // Timing of MIDI events will be quantized by the sample batch size.
  int samplesRemaining = bufferSize * 2;
  double msPerBatch = g_Speed * 1000.0 * (batchSize / (float)g_SampleRate) / 2;
  for (; samplesRemaining > 0; samplesRemaining -= batchSize) {
    //We progress the MIDI playback and then process `batchSize` samples at once
    if (batchSize > samplesRemaining) batchSize = samplesRemaining;

    //Loop through all MIDI messages which need to be played up until the current playback time
    for (g_MidiTimeMs += msPerBatch;
         g_MidiEvt && g_MidiTimeMs >= g_MidiEvt->time;
         g_MidiEvt = g_MidiEvt->next) {
      switch (g_MidiEvt->type) {
        case TML_PROGRAM_CHANGE: // program change (special handling for 10th MIDI channel with drums)
          tsf_channel_set_presetnumber(g_TSF, g_MidiEvt->channel, g_MidiEvt->program, g_MidiEvt->channel == 9);
          if (g_MidiEvt->channel == 9 || g_MidiEvt->channel == 15) {
            EM_ASM_({console.log('Program Change Channel: %d Program: %d', $0, $1);},
                    g_MidiEvt->channel, g_MidiEvt->program);
          }
          break;
        case TML_NOTE_ON:
          tsf_channel_note_on(g_TSF, g_MidiEvt->channel, g_MidiEvt->key, g_MidiEvt->velocity / 127.0f);
          break;
        case TML_NOTE_OFF:
          tsf_channel_note_off(g_TSF, g_MidiEvt->channel, g_MidiEvt->key);
          break;
        case TML_PITCH_BEND:
          tsf_channel_set_pitchwheel(g_TSF, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
          break;
        case TML_CONTROL_CHANGE:
          tsf_channel_midi_control(g_TSF, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
          if (g_MidiEvt->control == 0 || g_MidiEvt->control == 32) {
            EM_ASM_({console.log('Bank Select Channel: %d Controller: %d Value: %d', $0, $1, $2);},
                    g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
          }
          break;
        default:
          break;
      }

    }
    // Render the block of audio samples in float format
    tsf_render_float(g_TSF, buffer, batchSize / 2, 0);
    buffer += batchSize;
  }
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

  tsf_note_off_all(g_TSF);

  while(g_MidiEvt && g_MidiEvt->time < ms) {
    switch (g_MidiEvt->type) {
      // Ignore note on/note off events during seek
      case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
        tsf_channel_set_presetnumber(g_TSF, g_MidiEvt->channel, g_MidiEvt->program, g_MidiEvt->channel == 9);
        break;
      case TML_PITCH_BEND: //pitch wheel modification
        tsf_channel_set_pitchwheel(g_TSF, g_MidiEvt->channel, g_MidiEvt->pitch_bend);
        break;
      case TML_CONTROL_CHANGE: //MIDI controller messages
        tsf_channel_midi_control(g_TSF, g_MidiEvt->channel, g_MidiEvt->control, g_MidiEvt->control_value);
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
  tsf_reset(g_TSF);
  g_MidiEvt = NULL;
}

extern void tp_restart() {
  tsf_reset(g_TSF);
  g_MidiEvt = g_FirstEvt;
}

extern void tp_open(tsf *t, void *data, int length, int sampleRate) {
  g_MidiTimeMs = 0;
  g_DurationMs = 0;
  g_TSF = t;
  g_SampleRate = sampleRate;
  tsf_set_output(g_TSF, TSF_STEREO_INTERLEAVED, sampleRate, 0.0);
  // Channel 10 uses percussion sound bank
  tsf_channel_set_bank_preset(g_TSF, 9, 128, 0);
  g_MidiEvt = tml_load_memory(data, length);
  g_FirstEvt = g_MidiEvt;
  EM_ASM_({console.log('Tiny MIDI Player loaded %d bytes.', $0);}, length);
}

#ifdef __cplusplus
}
#endif