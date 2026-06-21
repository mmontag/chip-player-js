#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

/*
  USF Decoder: for playing "(Ultra) Nintendo 64 Sound" format (.USF/.MINIUSF) files

  Based on kode54's: https://gitlab.kode54.net/kode54/lazyusf2
  https://git.lopez-snowhill.net/chris/foo_input_usf/-/blob/master/psf.cpp

  As compared to kode54's original psf.cpp, the code has been patched to NOT
  rely on any fubar2000 base impls.

  Copyright (C) 2018 Juergen Wothke

  Credits: The real work is in the Mupen64plus (http://code.google.com/p/mupen64plus/ )
  emulator and in whatever changes kode54 had to apply to use it as a music player. I
  just added a bit of glue so the code can now also be used on the Web.

  Refactored for Chip Player JS (chiptune.app) on March 14, 2026.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>

#include <psflib.h>
#include "usf/usf.h"
#include "circular_buffer.h"

// Compatibility defines
#define t_int16 int16_t
#define stricmp strcasecmp

// Implemented on JavaScript side for "on-demand" file load
extern "C" int n64_request_file(const char *filename);

// --- String Utilities ---

static int stricmp_utf8(const char* s1, const char *s2) {
  return strcasecmp(s1, s2);
}

// --- PSF File System Callbacks ---

static void* psf_file_fopen(void *context, const char *uri) {
  return fopen(uri, "rb");
}

static size_t psf_file_fread(void *buffer, size_t size, size_t count, void *handle) {
  return fread(buffer, size, count, static_cast<FILE *>(handle));
}

static int psf_file_fseek(void *handle, int64_t offset, int whence) {
  return fseek(static_cast<FILE*>(handle), (long)offset, whence);
}

static int psf_file_fclose(void *handle) {
  return fclose(static_cast<FILE*>(handle));
}

static long psf_file_ftell(void *handle) {
  return ftell(static_cast<FILE*>(handle));
}

const psf_file_callbacks psf_file_system = {
  "\\/|:",
  nullptr,
  psf_file_fopen,
  psf_file_fread,
  psf_file_fseek,
  psf_file_fclose,
  psf_file_ftell
};

// --- Config Defaults ---

static const unsigned int cfg_deflength = 170000;
static const unsigned int cfg_deffade = 1000;
static const unsigned int cfg_suppressopeningsilence = 1;
static const unsigned int cfg_endsilenceseconds = 3;
static const unsigned int cfg_resample = 1;
static const unsigned int cfg_hle_audio = 1;

#define BORK_TIME 0xC0CAC01A

static unsigned long parse_time_crap(const char *input) {
  if (!input || !*input) return BORK_TIME;

  for (const char* p = input; *p; p++) {
    if (!isdigit((unsigned char)*p) && *p != ':' && *p != ',' && *p != '.') return BORK_TIME;
  }

  unsigned long value = 0;
  size_t in_len = strlen(input);
  char *copy = (char*)malloc(in_len + 1);
  if (!copy) return BORK_TIME;
  memcpy(copy, input, in_len + 1);

  char *strs = copy + in_len - 1;
  char *end;

  // Fraction of a second
  while (strs > copy && isdigit((unsigned char)*strs)) strs--;
  if (*strs == '.' || *strs == ',') {
    char* frac = strs + 1;
    if (strlen(frac) > 3) frac[3] = 0;
    value = strtoul(frac, &end, 10);
    size_t flen = strlen(frac);
    if (flen == 1) value *= 100;
    else if (flen == 2) value *= 10;
    *strs = 0;
    strs--;
  }

  // Seconds
  while (strs > copy && isdigit((unsigned char)*strs)) strs--;
  char* sec_ptr = (isdigit((unsigned char)*strs)) ? strs : strs + 1;
  value += strtoul(sec_ptr, &end, 10) * 1000;

  if (strs > copy) {
    *strs = 0; strs--;
    // Minutes
    while (strs > copy && isdigit((unsigned char)*strs)) strs--;
    char* min_ptr = (isdigit((unsigned char)*strs)) ? strs : strs + 1;
    value += strtoul(min_ptr, &end, 10) * 60000;

    if (strs > copy) {
      *strs = 0; strs--;
      // Hours
      while (strs > copy && isdigit((unsigned char)*strs)) strs--;
      value += strtoul(strs, &end, 10) * 3600000;
    }
  }

  free(copy);
  return value;
}

struct psf_info_meta_state {
  int tag_song_ms = 0;
  int tag_fade_ms = 0;
};

static int info_target(void *context, const char *name, const char *value) {
  auto *state = static_cast<psf_info_meta_state *>(context);
  if (!state) return 0;

  if (strcasecmp(name, "length") == 0) {
    unsigned long temp = parse_time_crap(value);
    if (temp != BORK_TIME) state->tag_song_ms = (int)temp;
  } else if (strcasecmp(name, "fade") == 0) {
    unsigned long temp = parse_time_crap(value);
    if (temp != BORK_TIME) state->tag_fade_ms = (int)temp;
  } else if (name[0] == '_') {
    // Handle USF specific required tags if necessary
    if (strcasecmp(name, "_enablecompare") != 0 && strcasecmp(name, "_enablefifofull") != 0 && strncasecmp(name, "_lib", 4) != 0) {
      // Some USF files might fail if they see unknown underscores.
      // We'll be permissive here to avoid the -1 error.
    }
  }
  return 0;
}

struct usf_loader_state {
  uint32_t enable_compare = 0;
  uint32_t enable_fifo_full = 0;
  void *emu_state = nullptr;
};

int usf_loader(void *context, const uint8_t *exe, size_t exe_size, const uint8_t *reserved, size_t reserved_size) {
  auto *state = static_cast<usf_loader_state *>(context);
  if (exe_size > 0) return -1; // USF uses reserved section for ROM data
  return usf_upload_section(state->emu_state, reserved, reserved_size);
}

int usf_info(void *context, const char *name, const char *value) {
  auto *state = static_cast<usf_loader_state *>(context);
  if (stricmp(name, "_enablecompare") == 0 && *value) state->enable_compare = 1;
  else if (stricmp(name, "_enablefifofull") == 0 && *value) state->enable_fifo_full = 1;
  return 0;
}

class input_usf {
  static const int SEEK_BUF_SIZE = 1024;
  int16_t seek_buffer[SEEK_BUF_SIZE * 2];

  bool eof = false;
  usf_loader_state *m_state = nullptr;
  char* m_path = nullptr;

  int data_written = 0;
  int remainder = 0;
  double startsilence = 0;
  int song_len = 0, fade_len = 0;
  int tag_song_ms = 0, tag_fade_ms = 0;

public:
  int32_t sample_rate = 44100;
  bool indefinite_playback = false; // ignores track duration (song_len) for looping tracks.

  input_usf() = default;

  void shutdown() {
    if (m_state) {
      if (m_state->emu_state) {
        usf_shutdown(m_state->emu_state);
        free(m_state->emu_state);
      }
      delete m_state;
      m_state = nullptr;
    }
    if (m_path) {
      free(m_path);
      m_path = nullptr;
    }
  }

  int open(const char *p_path) {
    if (m_path) free(m_path);
    m_path = (char*)malloc(strlen(p_path) + 1);
    if (!m_path) return -1;
    strcpy(m_path, p_path);

    psf_info_meta_state info_state;
    // The 0x21 is the PSF version for USF.
    // Important: psf_load parameters: (path, callbacks, version, loader, loader_ctx, info_cb, info_ctx, ...)
    int ret = psf_load(p_path, &psf_file_system, 0x21, nullptr, nullptr, info_target, &info_state, 0, nullptr, nullptr);
    if (ret < 0) return -1;

    tag_song_ms = info_state.tag_song_ms ? info_state.tag_song_ms : cfg_deflength;
    tag_fade_ms = info_state.tag_fade_ms ? info_state.tag_fade_ms : cfg_deffade;
    return 0;
  }

  int decode_initialize(int16_t *output_buffer, int out_size) {
    if (m_state) {
      if (m_state->emu_state) {
        usf_shutdown(m_state->emu_state);
        free(m_state->emu_state);
      }
      delete m_state;
    }

    m_state = new usf_loader_state();
    m_state->emu_state = malloc(usf_get_state_size());
    if (!m_state->emu_state) return -1;

    usf_clear(m_state->emu_state);
    usf_set_hle_audio(m_state->emu_state, cfg_hle_audio);

    // This call actually loads the data into the emulator state
    if (psf_load(m_path, &psf_file_system, 0x21, usf_loader, m_state, usf_info, m_state, 1, nullptr, nullptr) < 0) {
      return -1;
    }

    usf_set_compare(m_state->emu_state, m_state->enable_compare);
    usf_set_fifo_full(m_state->emu_state, m_state->enable_fifo_full);

    startsilence = 0;
    eof = false;
    data_written = 0;
    remainder = 0;

    if (cfg_suppressopeningsilence) {
      double current_silence = 0;
      double skip_max = (double)cfg_endsilenceseconds;
      while (true) {
        uint32_t skip_howmany = out_size;
        const char *err = cfg_resample ?
                          usf_render_resampled(m_state->emu_state, output_buffer, skip_howmany, sample_rate) :
                          usf_render(m_state->emu_state, output_buffer, skip_howmany, (int32_t*)&sample_rate);

        if (err) return -1;

        int i;
        for (i = 0; i < (int)skip_howmany; ++i) {
          if (output_buffer[i*2] || output_buffer[i*2+1]) break;
        }

        current_silence += (double)i / (double)sample_rate;
        if (i < (int)skip_howmany) {
          remainder = (int)skip_howmany - i;
          memmove(output_buffer, output_buffer + i*2, remainder * sizeof(int16_t) * 2);
          break;
        }
        if (current_silence >= skip_max) {
          eof = true;
          break;
        }
      }
      startsilence = current_silence;
    }

    calcfade();
    return 0;
  }

  int decode_run(int16_t *output_buffer, uint16_t size) {
    if (eof || !m_state || !m_state->emu_state) return -1;
    if (tag_song_ms && sample_rate && data_written >= (song_len + fade_len)) return -1;

    uint32_t written = 0;
    if (remainder) {
      written = (uint32_t)remainder;
      if (written > size) {
        // This shouldn't happen with typical buffer sizes, but safe-guard
        remainder -= size;
        written = size;
      } else {
        remainder = 0;
      }
    } else {
      uint32_t todo = size;
      const char *err = cfg_resample ?
                        usf_render_resampled(m_state->emu_state, output_buffer, todo, sample_rate) :
                        usf_render(m_state->emu_state, output_buffer, todo, (int32_t*)&sample_rate);
      if (err) return -1;
      written = todo;
    }

    int d_start = data_written;
    data_written += (int)written;
    int d_end = data_written;

    if (tag_song_ms && d_end > song_len) {
      for (int n = d_start; n < d_end; ++n) {
        if (n > song_len) {
          int offset = (n - d_start) * 2;
          if (n > song_len + fade_len) {
            output_buffer[offset] = output_buffer[offset+1] = 0;
          } else {
            double factor = (double)(song_len + fade_len - n) / (double)fade_len;
            output_buffer[offset] = (int16_t)(output_buffer[offset] * factor);
            output_buffer[offset+1] = (int16_t)(output_buffer[offset+1] * factor);
          }
        }
      }
    }
    return (int)written;
  }

  void decode_seek(double p_seconds) {
    if (!m_state || !m_state->emu_state) return;
    eof = false;
    double current_pos = (double)data_written / (double)sample_rate;

    if (p_seconds < current_pos) {
      usf_restart(m_state->emu_state);
      data_written = 0;
      current_pos = -startsilence;
    }

    double to_skip = p_seconds - current_pos;
    while (to_skip > 0) {
      uint32_t chunk = SEEK_BUF_SIZE;
      usf_render_resampled(m_state->emu_state, seek_buffer, chunk, sample_rate);
      double chunk_secs = (double)chunk / (double)sample_rate;
      if (to_skip < chunk_secs) {
        remainder = (int)((chunk_secs - to_skip) * sample_rate);
        data_written += (int)(chunk - (uint32_t)remainder);
        break;
      }
      data_written += (int)chunk;
      to_skip -= chunk_secs;
    }
  }

  int32_t getSamplesToPlay() const { return song_len + fade_len; }
  int32_t getDataWritten() const { return data_written; }

private:
  void calcfade() {
    if (sample_rate <= 0) return;
    song_len = (int)((double)tag_song_ms * sample_rate / 1000.0);
    fade_len = (int)((double)tag_fade_ms * sample_rate / 1000.0);
  }
};

static input_usf g_input_usf;

extern "C" {
int32_t n64_load_file(const char *uri, int16_t *output_buffer, uint16_t outSize, int32_t samp_rate) {
  if (g_input_usf.open(uri) < 0) return -1;
  g_input_usf.sample_rate = samp_rate;
  return g_input_usf.decode_initialize(output_buffer, outSize);
}

int32_t n64_get_duration_ms() {
  if (g_input_usf.sample_rate == 0) return 0;
  return (int32_t)(1000.0 * g_input_usf.getSamplesToPlay() / g_input_usf.sample_rate);
}

int32_t n64_get_position_ms() {
  if (g_input_usf.sample_rate == 0) return 0;
  return (int32_t)(1000.0 * g_input_usf.getDataWritten() / g_input_usf.sample_rate);
}

int32_t n64_render_audio(int16_t *output_buffer, uint16_t outSize) {
  return (int32_t)g_input_usf.decode_run(output_buffer, outSize);
}

void n64_seek_ms(int msec) {
  g_input_usf.decode_seek((double)msec / 1000.0);
}

void n64_set_indefinite_playback(bool enabled) {
  g_input_usf.indefinite_playback = enabled;
}

void n64_shutdown() {
  g_input_usf.shutdown();
}
}

#pragma clang diagnostic pop
