#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

/*
  GSF Decoder: for playing Game Boy Advance Sound Format (.GSF/.MINIGSF) files

  Based on ViOGSF (VBA-M audio core) and Christopher Snowhill's psflib.
  Refactored for Chip Player JS (chiptune.app).

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cctype>
#include <vector>
#include <algorithm>
#include <cmath>

#include <psflib.h>
#include "circular_buffer.h"
#include "vbam/gba/GBA.h"
#include "vbam/gba/Sound.h"

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

static const psf_file_callbacks psf_file_system = {
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
static const unsigned int cfg_deffade = 10000;
static const unsigned int cfg_suppressopeningsilence = 1;
static const unsigned int cfg_endsilenceseconds = 3;

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
  if (!state || !name || !value) return 0;

  if (strcasecmp(name, "length") == 0) {
    unsigned long temp = parse_time_crap(value);
    if (temp != BORK_TIME) state->tag_song_ms = (int)temp;
  } else if (strcasecmp(name, "fade") == 0) {
    unsigned long temp = parse_time_crap(value);
    if (temp != BORK_TIME) state->tag_fade_ms = (int)temp;
  }
  return 0;
}

// --- GSF Loading Structure ---

struct gsf_loader_state {
  uint32_t entry_point = 0;
  bool entry_point_set = false;
  std::vector<uint8_t> rom;
};

static inline uint32_t read_u32_le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int gsf_loader(void *context, const uint8_t *exe, size_t exe_size,
                      const uint8_t *reserved, size_t reserved_size) {
  if (!exe || exe_size < 12) return -1;
  auto *state = static_cast<gsf_loader_state *>(context);

  uint32_t entry = read_u32_le(exe);
  uint32_t offset = read_u32_le(exe + 4) & 0x1ffffff;
  uint32_t rom_size = read_u32_le(exe + 8);

  if (!state->entry_point_set) {
    state->entry_point = entry;
    state->entry_point_set = true;
  }

  if (rom_size > exe_size - 12) {
    rom_size = exe_size - 12;
  }

  if (offset + rom_size > state->rom.size()) {
    state->rom.resize(offset + rom_size, 0);
  }

  std::copy(exe + 12, exe + 12 + rom_size, state->rom.data() + offset);
  return 0;
}

// --- Sound Output Receiver ---

struct GSFSoundOut : public GBASoundOut {
  circular_buffer<int16_t> ring_buffer;

  GSFSoundOut() {
    ring_buffer.resize(65536 * 2); // 64k stereo samples
  }

  void write(const void *samples, unsigned long bytes) override {
    unsigned long sample_count = bytes / sizeof(int16_t);
    const int16_t *src = (const int16_t *)samples;
    while (sample_count > 0) {
      unsigned long avail = 0;
      int16_t *dst = ring_buffer.get_write_ptr(avail);
      if (avail == 0) break;
      unsigned long to_write = std::min(sample_count, avail);
      std::copy(src, src + to_write, dst);
      ring_buffer.samples_written(to_write);
      src += to_write;
      sample_count -= to_write;
    }
  }

  void reset() {
    ring_buffer.reset();
  }
};

// --- GSF Player Engine ---

class input_gsf {
  static const int SEEK_BUF_SIZE = 1024;
  int16_t seek_buffer[SEEK_BUF_SIZE * 2];

  bool eof = false;
  GBASystem *gba = nullptr;
  GSFSoundOut sound_out;
  char *m_path = nullptr;

  int data_written = 0;
  int song_len = 0, fade_len = 0;
  int tag_song_ms = 0, tag_fade_ms = 0;
  int fade_start_pos = 0;
  bool song_loops = false;

public:
  volatile int32_t sample_rate = 44100;
  volatile bool indefinite_playback = false;

  input_gsf() = default;
  ~input_gsf() { shutdown(); }

  void shutdown() {
    if (gba) {
      soundShutdown(gba);
      CPUCleanUp(gba);
      delete gba;
      gba = nullptr;
    }
    sound_out.reset();
    if (m_path) {
      free(m_path);
      m_path = nullptr;
    }
    eof = false;
    data_written = 0;
  }

  int open(const char *p_path) {
    if (m_path) free(m_path);
    m_path = (char*)malloc(strlen(p_path) + 1);
    if (!m_path) return -1;
    strcpy(m_path, p_path);

    psf_info_meta_state info_state;
    // 0x22 is the PSF version for GSF
    int ret = psf_load(p_path, &psf_file_system, 0x22, nullptr, nullptr, info_target, &info_state, 0, nullptr, nullptr);
    if (ret < 0) return -1;

    tag_song_ms = info_state.tag_song_ms ? info_state.tag_song_ms : cfg_deflength;
    tag_fade_ms = info_state.tag_fade_ms ? info_state.tag_fade_ms : cfg_deffade;
    return 0;
  }

  int decode_initialize(int16_t *output_buffer, int out_size) {
    if (gba) {
      soundShutdown(gba);
      CPUCleanUp(gba);
      delete gba;
      gba = nullptr;
    }
    sound_out.reset();

    gba = new GBASystem();
    if (!gba) return -1;

    gsf_loader_state loader_state;
    // Load GSF program and companion .gsflib dependencies
    if (psf_load(m_path, &psf_file_system, 0x22, gsf_loader, &loader_state, nullptr, nullptr, 1, nullptr, nullptr) < 0) {
      shutdown();
      return -1;
    }

    if (loader_state.rom.empty()) {
      shutdown();
      return -1;
    }

    gba->cpuIsMultiBoot = ((loader_state.entry_point >> 24) == 2);
    if (!CPULoadRom(gba, loader_state.rom.data(), loader_state.rom.size())) {
      shutdown();
      return -1;
    }

    soundInit(gba, &sound_out);
    soundSetSampleRate(gba, sample_rate);
    soundReset(gba);
    CPUInit(gba);
    CPUReset(gba);

    eof = false;
    data_written = 0;

    calcfade();
    fade_start_pos = song_len;

    // Suppress opening silence if configured
    if (cfg_suppressopeningsilence && output_buffer && out_size > 0) {
      double current_silence = 0;
      double skip_max = (double)cfg_endsilenceseconds;
      while (current_silence < skip_max) {
        while (sound_out.ring_buffer.data_available() < (unsigned long)(out_size * 2)) {
          CPULoop(gba, 250000);
        }

        int avail_samples = (int)(sound_out.ring_buffer.data_available() / 2);
        int to_check = std::min(out_size, avail_samples);
        sound_out.ring_buffer.read(output_buffer, to_check * 2);

        int i;
        for (i = 0; i < to_check; ++i) {
          if (output_buffer[i*2] != 0 || output_buffer[i*2+1] != 0) break;
        }

        current_silence += (double)i / (double)sample_rate;
        if (i < to_check) {
          // Found non-silence: push back non-silent remainder into output_buffer
          int rem = to_check - i;
          memmove(output_buffer, output_buffer + i*2, rem * sizeof(int16_t) * 2);
          sound_out.ring_buffer.reset();
          // Write back remainder to ring buffer
          sound_out.write(output_buffer, rem * sizeof(int16_t) * 2);
          break;
        }
      }
    }

    return 0;
  }

  int decode_run(int16_t *output_buffer, uint16_t size) {
    if (eof || !gba) return -1;
    if (tag_song_ms && sample_rate && data_written >= (fade_start_pos + fade_len)) {
      eof = true;
      return -1;
    }

    // Run emulator until ring buffer has enough stereo samples
    unsigned long required_samples = (unsigned long)size * 2;
    int watchdog = 0;
    while (sound_out.ring_buffer.data_available() < required_samples) {
      CPULoop(gba, 250000);
      watchdog++;
      if (watchdog > 100) break; // Safeguard against runaway CPU loops
    }

    unsigned long avail_samples = sound_out.ring_buffer.data_available() / 2;
    uint32_t written = (uint32_t)std::min((unsigned long)size, avail_samples);
    if (written == 0) return 0;

    sound_out.ring_buffer.read(output_buffer, written * 2);

    int d_start = data_written;
    data_written += (int)written;
    int d_end = data_written;

    if (data_written < song_len) {
      fade_start_pos = song_len;
    } else if (indefinite_playback && song_loops) {
      fade_start_pos = data_written;
    }

    // Apply fadeout
    if (tag_song_ms && d_end > fade_start_pos) {
      for (int n = d_start; n < d_end; ++n) {
        if (n > fade_start_pos) {
          int offset = (n - d_start) * 2;
          if (n >= fade_start_pos + fade_len) {
            output_buffer[offset] = output_buffer[offset + 1] = 0;
          } else {
            double factor = (double)(fade_start_pos + fade_len - n) / (double)fade_len;
            output_buffer[offset] = (int16_t)(output_buffer[offset] * factor);
            output_buffer[offset + 1] = (int16_t)(output_buffer[offset + 1] * factor);
          }
        }
      }
    }

    return (int)written;
  }

  void decode_seek(double p_seconds) {
    if (!gba || sample_rate <= 0) return;
    int target_samples = (int)(p_seconds * sample_rate);

    if (target_samples < data_written) {
      // Seek backwards: restart from beginning
      if (decode_initialize(nullptr, 0) < 0) return;
    }

    while (data_written < target_samples && !eof) {
      int needed = target_samples - data_written;
      int chunk = std::min(needed, SEEK_BUF_SIZE);
      int ret = decode_run(seek_buffer, chunk);
      if (ret <= 0) break;
    }
  }

  void set_channel_mask(int mask) {
    if (gba) soundSetEnable(gba, mask);
  }

  int get_channel_mask() const {
    return gba ? soundGetEnable(gba) : 0x30f;
  }

  int32_t getSamplesToPlay() const { return song_len + fade_len; }
  int32_t getDataWritten() const { return data_written; }

private:
  void calcfade() {
    if (sample_rate <= 0) return;
    song_len = (int)((double)tag_song_ms * sample_rate / 1000.0);
    fade_len = (int)((double)tag_fade_ms * sample_rate / 1000.0);
    song_loops = (tag_fade_ms > 0);
  }
};

static input_gsf g_input_gsf;

extern "C" {
int32_t gsf_load_file(const char *uri, int16_t *output_buffer, uint16_t outSize, int32_t samp_rate) {
  if (g_input_gsf.open(uri) < 0) return -1;
  g_input_gsf.sample_rate = samp_rate;
  return g_input_gsf.decode_initialize(output_buffer, outSize);
}

int32_t gsf_get_duration_ms() {
  if (g_input_gsf.sample_rate == 0) return 0;
  return (int32_t)(1000.0 * g_input_gsf.getSamplesToPlay() / g_input_gsf.sample_rate);
}

int32_t gsf_get_position_ms() {
  if (g_input_gsf.sample_rate == 0) return 0;
  return (int32_t)(1000.0 * g_input_gsf.getDataWritten() / g_input_gsf.sample_rate);
}

int32_t gsf_render_audio(int16_t *output_buffer, uint16_t outSize) {
  return g_input_gsf.decode_run(output_buffer, outSize);
}

void gsf_seek_ms(int msec) {
  g_input_gsf.decode_seek((double)msec / 1000.0);
}

void gsf_set_indefinite_playback(bool enabled) {
  g_input_gsf.indefinite_playback = enabled;
}

bool gsf_get_indefinite_playback() {
  return g_input_gsf.indefinite_playback;
}

void gsf_set_channel_mask(int mask) {
  g_input_gsf.set_channel_mask(mask);
}

int gsf_get_channel_mask() {
  return g_input_gsf.get_channel_mask();
}

void gsf_shutdown() {
  g_input_gsf.shutdown();
}
}

#pragma clang diagnostic pop
