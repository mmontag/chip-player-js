//
// Created by Matt Montag on 2019-06-20.
//

#include "sounddef.h"
#include "v2mplayer.h"
#include "v2mconv.h"
#include <emscripten.h> // TODO: Remove

#ifdef __cplusplus
extern "C" {
#endif

static V2MPlayer v2m;
const int g_num_channels = 2;
const int g_ticks_per_sec = 1000;
long g_sample_time = 0;
int g_sample_rate = 44100;
unsigned char * g_converted_data = nullptr;

void v2m_convert_data(const uint8_t *data, size_t length, uint8_t **out_data) {
  int version = CheckV2MVersion(data, length);
  if (version < 0) {
    EM_ASM_({ console.log('Error during V2M conversion.', $0); }, version);
    return; // fatal error
  }

  int converted_length;
  ConvertV2M((const uint8_t *)data, length, out_data, &converted_length);
}

int v2m_open(uint8_t *data, int length, int sample_rate) {
  sdInit();
  v2m.Init(g_ticks_per_sec);

  v2m_convert_data(data, length, &g_converted_data);
  if (v2m.Open(g_converted_data, sample_rate)) {
    v2m.Play();
    g_sample_time = 0;
    g_sample_rate = sample_rate;
    return 0;
  }

  return 1;
}

int v2m_write_audio(float *buffer, int buffer_size) {
  g_sample_time += buffer_size;
  return v2m.Render(buffer, buffer_size);
}

float v2m_get_position_ms() {
  return v2m.GetTime();
}

float v2m_get_duration_ms() {
  return 1000 * v2m.Length();
}

void v2m_seek_ms(int position_ms) {
  // works with milliseconds because ticks per sec = 1000
  v2m.Play(position_ms);
  g_sample_time = (position_ms / 1000) * g_sample_rate;
}

void v2m_set_speed(float speed) {
  v2m.SetSpeed(speed);
}

void v2m_close() {
  v2m.Close();
  sdClose();

  if (g_converted_data) {
    delete g_converted_data;
    g_converted_data = nullptr;
  }
}

#ifdef __cplusplus
}
#endif
