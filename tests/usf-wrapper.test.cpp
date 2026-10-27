// Exercise the actual wrapper with a deterministic stand-in for the USF emulator.
#include <cassert>
#include <cstdint>
#include <cstring>
#include "../src/bindings/lazyusf2-wrapper.cpp"

extern "C" {
int psf_load(const char*, const psf_file_callbacks*, uint8_t, psf_load_callback, void*,
             psf_info_callback, void*, int, psf_status_callback, void*) { return 0; }
size_t usf_get_state_size() { return sizeof(int); }
void usf_clear(void* state) { *static_cast<int*>(state) = 0; }
void usf_set_hle_audio(void*, int) {}
void usf_set_compare(void*, int) {}
void usf_set_fifo_full(void*, int) {}
int usf_upload_section(void*, const uint8_t*, size_t) { return 0; }
void usf_restart(void* state) { *static_cast<int*>(state) = 0; }
void usf_shutdown(void*) {}
const char* usf_render_resampled(void* state, int16_t* buffer, size_t count, int32_t) {
  int& position = *static_cast<int*>(state);
  for (size_t i = 0; i < count; ++i, ++position) {
    const int16_t value = position < 3 ? 0 : (int16_t)position;
    buffer[i * 2] = value;
    buffer[i * 2 + 1] = -value;
  }
  return nullptr;
}
const char* usf_render(void* state, int16_t* buffer, size_t count, int32_t* rate) {
  return usf_render_resampled(state, buffer, count, *rate);
}
}

static void expect_frames(int16_t* buffer, int first, int count) {
  for (int i = 0; i < count; ++i) {
    assert(buffer[2 * i] == first + i);
    assert(buffer[2 * i + 1] == -(first + i));
  }
}

int main() {
  int16_t output[16] = {};
  assert(n64_load_file("test.miniusf", output, 8, 1000) == 0);
  assert(n64_render_audio(output, 3) == 3);
  expect_frames(output, 3, 3);
  assert(n64_render_audio(output, 7) == 7);
  expect_frames(output, 6, 7);
  n64_seek_ms(15);
  assert(n64_get_position_ms() == 15);
  assert(n64_render_audio(output, 8) == 8);
  expect_frames(output, 18, 8);
  n64_seek_ms(2);
  assert(n64_get_position_ms() == 2);
  assert(n64_render_audio(output, 8) == 8);
  expect_frames(output, 5, 8);
  n64_shutdown();

  assert(n64_load_file("test.miniusf", output, 8, 1000) == 0);
  n64_seek_ms(2); // Seek inside the buffered opening audio.
  assert(n64_render_audio(output, 8) == 8);
  expect_frames(output, 5, 8);
  n64_shutdown();
}
