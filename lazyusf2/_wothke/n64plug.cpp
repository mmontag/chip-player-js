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
#include <stdexcept>
#include <set>
#include <codecvt>
#include <locale>

#include <emscripten/fetch.h>
#include <psflib.h>

#include "usf/usf.h"
#include "circular_buffer.h"

#define t_int16 signed short
#define stricmp strcasecmp


// FIXME unused: maybe useful to eventually implement utf8 conversions..
std::wstring stringToWstring(const std::string &t_str) {
  //setup converter
  typedef std::codecvt_utf8<wchar_t> convert_type;
  std::wstring_convert<convert_type, wchar_t> converter;

  //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
  return converter.from_bytes(t_str);
}

// implemented on JavaScript side (also see callback.js) for "on-demand" file load:
extern "C" int n64_request_file(const char *filename);

// just some fillers to change kode54's below code as little as possible
class exception_io_data : public std::runtime_error {
public:
  exception_io_data(const char *what = "exception_io_data") : std::runtime_error(what) {}
};

int stricmp_utf8(std::string const &s1, const char *s2) {
  return strcasecmp(s1.c_str(), s2);
}

int stricmp_utf8(const char *s1, const char *s2) {
  return strcasecmp(s1, s2);
}

int stricmp_utf8_partial(std::string const &s1, const char *s2) {
  std::string s1pref = s1.substr(0, strlen(s2));
  return strcasecmp(s1pref.c_str(), s2);
}

# define strdup(s)                                        \
  (__extension__                                          \
    ({                                                    \
      const char *__old = (s);                            \
      size_t __len = strlen (__old) + 1;                  \
      char *__new = (char *) malloc (__len);              \
      (char *) memcpy (__new, __old, __len);              \
    }))

static void* psf_file_fopen(void *context, const char *uri) {
  return fopen(uri, "rb");
}

static size_t psf_file_fread(void *buffer, size_t size, size_t count, void *handle) {
  return fread(buffer, size, count, static_cast<FILE *>(handle));
}

static int psf_file_fseek(void *handle, int64_t offset, int whence) {
  return fseek(static_cast<FILE*>(handle), offset, whence);
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


// ------------------ stripped down version based on foobar2000 plugin (see psf.cpp)  --------------------

//#define DBG(a) OutputDebugString(a)
#define DBG(a)

static unsigned int cfg_deflength = 170000;
static unsigned int cfg_deffade = 10000;
static unsigned int cfg_suppressopeningsilence = 1;
static unsigned int cfg_suppressendsilence = 1;
static unsigned int cfg_endsilenceseconds = 10;

static unsigned int cfg_resample = 1;

// TODO(montag): expose HLE audio setting
static unsigned int cfg_hle_audio = 1;
// TODO(montag): add sample rate argument to n64_load_file
static unsigned int cfg_resample_hz = 44100;

static const char field_length[] = "usf_length";
static const char field_fade[] = "usf_fade";

#define BORK_TIME 0xC0CAC01A

static unsigned long parse_time_crap(const char *input) {
  if (!input) return BORK_TIME;
  int len = strlen(input);
  if (!len) return BORK_TIME;
  int value = 0;
  {
    int i;
    for (i = len - 1; i >= 0; i--) {
      if ((input[i] < '0' || input[i] > '9') && input[i] != ':' && input[i] != ',' && input[i] != '.') {
        return BORK_TIME;
      }
    }
  }

  char *foo = strdup(input);

  if (!foo)
    return BORK_TIME;

  char *bar = foo;
  char *strs = bar + strlen(foo) - 1;
  char *end;
  while (strs > bar && (*strs >= '0' && *strs <= '9')) {
    strs--;
  }
  if (*strs == '.' || *strs == ',') {
    // fraction of a second
    strs++;
    if (strlen(strs) > 3) strs[3] = 0;
    value = strtoul(strs, &end, 10);
    switch (strlen(strs)) {
      case 1:
        value *= 100;
        break;
      case 2:
        value *= 10;
        break;
    }
    strs--;
    *strs = 0;
    strs--;
  }
  while (strs > bar && (*strs >= '0' && *strs <= '9')) {
    strs--;
  }
  // seconds
  if (*strs < '0' || *strs > '9') strs++;
  value += strtoul(strs, &end, 10) * 1000;
  if (strs > bar) {
    strs--;
    *strs = 0;
    strs--;
    while (strs > bar && (*strs >= '0' && *strs <= '9')) {
      strs--;
    }
    if (*strs < '0' || *strs > '9') strs++;
    value += strtoul(strs, &end, 10) * 60000;
    if (strs > bar) {
      strs--;
      *strs = 0;
      strs--;
      while (strs > bar && (*strs >= '0' && *strs <= '9')) {
        strs--;
      }
      value += strtoul(strs, &end, 10) * 3600000;
    }
  }
  free(foo);
  return value;
}

// hack: dummy impl to replace foobar2000 stuff
const int MAX_INFO_LEN = 10;

class file_info {
  double len;

  const char *sampleRate;
  const char *channels;

public:
  file_info() {
    sampleRate = (const char *) malloc(MAX_INFO_LEN);
    channels = (const char *) malloc(MAX_INFO_LEN);
  }

  ~file_info() {
    free((void *) channels);
    free((void *) sampleRate);
  }

  void info_set_int(const char *tag, int value) {
    if (!stricmp_utf8(tag, "samplerate")) {
      snprintf((char *) sampleRate, MAX_INFO_LEN, "%d", value);
    } else if (!stricmp_utf8(tag, "channels")) {
      snprintf((char *) channels, MAX_INFO_LEN, "%d", value);
    }
    // who cares.. just ignore
  }

  const char *info_get(std::string &t) {
    const char *tag = t.c_str();

    if (!stricmp_utf8(tag, "samplerate")) {
      return sampleRate;
    } else if (!stricmp_utf8(tag, "channels")) {
      return channels;
    }
    return "unavailable";
  }

  void set_length(double l) {
    len = l;
  }

  unsigned int meta_get_count() { return 0; }

  unsigned int meta_enum_value_count(unsigned int i) { return 0; }

  const char *meta_enum_value(unsigned int i, unsigned int k) { return "dummy"; }

  void meta_modify_value(unsigned int i, unsigned int k, const char *value) {}

  unsigned int info_get_count() { return 0; }

  const char *info_enum_name(unsigned int i) { return "dummy"; }

  void info_set(const char *name, const char *value) {}

  void info_set(std::string &name, const char *value) {}

  const char *info_enum_value(unsigned int i) { return "dummy"; }

  void info_set_replaygain(const char *tag, const char *value) {}

  void info_set_replaygain(std::string &tag, const char *value) {}

  void meta_add(const char *tag, const char *value) {}

  void meta_add(std::string &tag, const char *value) {}
};


static void info_meta_ansi(file_info &info) {
/* FIXME eventually migrate original impl

	for ( unsigned i = 0, j = info.meta_get_count(); i < j; i++ )
	{
		for ( unsigned k = 0, l = info.meta_enum_value_count( i ); k < l; k++ )
		{
			const char * value = info.meta_enum_value( i, k );
			info.meta_modify_value( i, k, pfc::stringcvt::string_utf8_from_ansi( value ) );
		}
	}
	for ( unsigned i = 0, j = info.info_get_count(); i < j; i++ )
	{
		const char * name = info.info_enum_name( i );

		if ( name[ 0 ] == '_' )
			info.info_set( std::string( name ), pfc::stringcvt::string_utf8_from_ansi( info.info_enum_value( i ) ) );
	}
*/
}

struct psf_info_meta_state {
  file_info *info;

  std::string name;

  bool utf8;

  int tag_song_ms;
  int tag_fade_ms;

  psf_info_meta_state() : info(0), utf8(false), tag_song_ms(0), tag_fade_ms(0) {}
};

static int info_target(void *context, const char *name, const char *value) {
  // typical tags: _lib, _enablecompare(on), _enableFIFOfull(on), fade, volume
  // game, genre, year, copyright, track, title, length(x:x.xxx), artist

  // FIXME: various "_"-settings are currently not used to configure the emulator

  auto *state = (psf_info_meta_state *) context;

  std::string &tag = state->name;

  tag.assign(name);

  // TODO(montag): Handle miniusf song metadata

  if (!stricmp_utf8_partial(tag, "replaygain_"))  // FIXME: how does relate to the "volume"?
  {
    DBG("reading RG info");
    state->info->info_set_replaygain(tag, value);
  } else if (!stricmp_utf8(tag, "length")) {
    DBG("reading length");
    int temp = parse_time_crap(value);
    if (temp != BORK_TIME) {
      state->tag_song_ms = temp;
      state->info->info_set_int(field_length, state->tag_song_ms);
    }
  } else if (!stricmp_utf8(tag, "fade")) {
    DBG("reading fade");
    int temp = parse_time_crap(value);
    if (temp != BORK_TIME) {
      state->tag_fade_ms = temp;
      state->info->info_set_int(field_fade, state->tag_fade_ms);
    }
  } else if (!stricmp_utf8(tag, "volume")) {
    // FIXME: observed loud songs with volume "3.5" and muted songs with "8"
    //		  -> settings might be used to normalize output volume?
  } else if (!stricmp_utf8(tag, "utf8")) {
    state->utf8 = true;
  } else if (!stricmp_utf8_partial(tag, "_lib")) {
    DBG("found _lib");
    // usflib dependencies must be preloaded into emscripten filesystem.
    // state->info->info_set_lib(tag, value);
  } else if (!stricmp_utf8(tag, "_enablecompare")) {
    DBG("found _enablecompare");
    state->info->info_set(tag, value);
  } else if (!stricmp_utf8(tag, "_enablefifofull")) {
    DBG("found _enablefifofull");
    state->info->info_set(tag, value);
  } else if (!stricmp_utf8(tag, "_64thbug1") || !stricmp_utf8(tag, "_64thbug2")) {
    DBG("found _64thbug* tag, ignoring");
    state->info->info_set(tag, value);
  } else if (tag[0] == '_') {
    DBG("found unknown required tag, failing");
    return -1;
  } else {
    state->info->meta_add(tag, value);
  }

  return 0;
}

struct usf_loader_state {
  uint32_t enable_compare;
  uint32_t enable_fifo_full;
  void *emu_state;

  usf_loader_state() : enable_compare(0), enable_fifo_full(0), emu_state(0) {}

  ~usf_loader_state() {
    if (emu_state)
      free(emu_state);
  }
};

int usf_loader(void *context, const uint8_t *exe, size_t exe_size, const uint8_t *reserved, size_t reserved_size) {

  struct usf_loader_state *state = (struct usf_loader_state *) context;
  if (exe_size > 0) return -1;

  return usf_upload_section(state->emu_state, reserved, reserved_size);
}

int usf_info(void *context, const char *name, const char *value) {
  struct usf_loader_state *state = (struct usf_loader_state *) context;

  if (stricmp(name, "_enablecompare") == 0 && strlen(value))
    state->enable_compare = 1;
  else if (stricmp(name, "_enablefifofull") == 0 && strlen(value))
    state->enable_fifo_full = 1;

  return 0;
}

static void print_message(void * context, const char * message)
{
  auto * msgbuf = (std::string *)context;
  msgbuf->append(message);
}

/*
	removed: inheritance, remove_tags(), retag(), etc
*/
class input_usf {
  const static t_int16 SEEK_BUF_SIZE = 1024;
  t_int16 seek_buffer[SEEK_BUF_SIZE * 2];  // discarded sample output during "seek"

  bool no_loop, eof, in_playback, resample;

  usf_loader_state *m_state;

  std::string m_path;

  int32_t sample_rate, last_sample_rate;

  int err;

  int data_written, remainder;

  double startsilence, silence;

  int song_len, fade_len;
  int tag_song_ms, tag_fade_ms;

  file_info m_info;

  bool do_suppressendsilence;

  circular_buffer<t_int16> m_buffer;

public:
  input_usf() : m_state(0) {}

  ~input_usf() {
    shutdown();
  }

  int32_t getDataWritten() { return data_written; }

  int32_t getSamplesToPlay() { return song_len + fade_len; }

  int32_t getSamplesRate() { return sample_rate; }


  std::vector<std::string> splitpath(const std::string &str, const std::set<char> &delimiters) {

    std::vector<std::string> result;

    char const *pch = str.c_str();
    char const *start = pch;
    for (; *pch; ++pch) {
      if (delimiters.find(*pch) != delimiters.end()) {
        if (start != pch) {
          std::string str(start, pch);
          result.push_back(str);
        } else {
          result.push_back("");
        }
        start = pch + 1;
      }
    }
    result.push_back(start);

    return result;
  }


  void shutdown() {
    if (m_state) {
      usf_shutdown(m_state->emu_state);
      delete m_state;
      m_state = 0;
    }
  }

  int open(const char *p_path) {

    m_path = p_path;

    psf_info_meta_state info_state;
    info_state.info = &m_info;

    std::string msgbuf;

    int ret = psf_load(p_path, &psf_file_system, 0x21, 0, NULL, info_target, &info_state, 0, print_message, &msgbuf);

    if (ret <= 0) {
      printf("Not a USF file (%s)\n", p_path);
      throw exception_io_data("Not a USF file");
    }

    if (!info_state.utf8)
      info_meta_ansi(m_info);

    tag_song_ms = info_state.tag_song_ms;
    tag_fade_ms = info_state.tag_fade_ms;

    if (!tag_song_ms) {
      tag_song_ms = cfg_deflength;
      tag_fade_ms = cfg_deffade;
    }

    m_info.set_length((double) (tag_song_ms + tag_fade_ms) * .001);
    m_info.info_set_int("channels", 2);

    return 0;
  }

  void decode_initialize(t_int16 *output_buffer, int out_size) {
    shutdown();

    m_state = new usf_loader_state;

    m_state->emu_state = malloc(usf_get_state_size());

    usf_clear(m_state->emu_state);

    usf_set_hle_audio(m_state->emu_state, cfg_hle_audio);

    if (psf_load(m_path.c_str(), &psf_file_system, 0x21, usf_loader, m_state, usf_info, m_state, 1, 0, NULL) < 0)
      throw exception_io_data("Invalid USF");

    usf_set_compare(m_state->emu_state, m_state->enable_compare);
    usf_set_fifo_full(m_state->emu_state, m_state->enable_fifo_full);

    startsilence = silence = 0;

    eof = 0;
    err = 0;
    data_written = 0;
    remainder = 0;
    no_loop = 1;

    resample = cfg_resample;

    sample_rate = resample ? cfg_resample_hz : 0;
    last_sample_rate = 0;

    do_suppressendsilence = !!cfg_suppressendsilence;

    in_playback = 0;

    double skip_max = double(cfg_endsilenceseconds);

    if (cfg_suppressopeningsilence) // ohcrap
    {
      for (;;) {
        unsigned skip_howmany = out_size;
        const char *err = resample ? usf_render_resampled(m_state->emu_state, output_buffer, skip_howmany, sample_rate)
                                   : usf_render(m_state->emu_state, output_buffer, skip_howmany, &sample_rate);
        if (err != 0)
          throw exception_io_data(err);
        short *foo = (short *) output_buffer;
        unsigned i;
        for (i = 0; i < skip_howmany; ++i) {
          if (foo[0] || foo[1]) break;
          foo += 2;
        }
        silence += double(i) / double(sample_rate);
        if (i < skip_howmany) {
          remainder = skip_howmany - i;
          memmove(output_buffer, foo, remainder * sizeof(short) * 2);
          break;
        }
        if (silence >= skip_max) {
          eof = true;
          break;
        }
      }

      startsilence += silence;
      silence = 0;
    }

    if (do_suppressendsilence) {
      if (!resample && !sample_rate) {
        const char *err = usf_render(m_state->emu_state, 0, 0, &sample_rate);
        if (err != 0)
          throw exception_io_data(err);
      }

      m_buffer.resize(sample_rate * skip_max * 2);  // WTF? 44100*2*2 bytes for silence detect???
      if (remainder) {
        unsigned long count;
        t_int16 *ptr = m_buffer.get_write_ptr(count);
        memcpy(ptr, output_buffer, remainder * sizeof(t_int16) * 2);
        m_buffer.samples_written(remainder * 2);
        remainder = 0;
      }
    }

    if (sample_rate)
      calcfade();
  }

  int decode_run(int16_t *output_buffer, uint16_t size) {
    // note: original impl used a separate output buffer (that needed to be copied later)
    if (eof || err < 0) return -1;

    if (no_loop && tag_song_ms && sample_rate && data_written >= (song_len + fade_len))
      return -1;

    unsigned int written = 0;

    int samples = size;

    short *ptr;

    if (do_suppressendsilence) {
      unsigned long max_fill = no_loop ? ((song_len + fade_len) - data_written) * 2 : 0;

      unsigned long done = 0;
      unsigned long count = 0;  // FIXME was uninitialized
      ptr = m_buffer.get_write_ptr(count);
      if (max_fill && count > max_fill)
        count = max_fill;

      count /= 2;

      while (count) {
        unsigned int todo = size;
        if (todo > count)
          todo = (unsigned int) count;
        const char *err = resample ? usf_render_resampled(m_state->emu_state, ptr, todo, sample_rate) : usf_render(
          m_state->emu_state,
          ptr,
          todo,
          &sample_rate);
        if (err != 0) {
          throw exception_io_data();
        }
        ptr += todo * 2;
        done += todo;
        count -= todo;
      }

      m_buffer.samples_written(done * 2);

      if (m_buffer.test_silence()) {
        eof = true;
        return -1;
      }

      written = m_buffer.data_available() / 2;
      if (written > size)
        written = size;

      m_buffer.read(output_buffer, written * 2);
      ptr = output_buffer;
    } else {
      if (remainder) {
        written = remainder;
        remainder = 0;
      } else {
        const char *err = resample ? usf_render_resampled(m_state->emu_state, output_buffer, size, sample_rate)
                                   : usf_render(m_state->emu_state, output_buffer, size, &sample_rate);
        if (err != 0)
          throw exception_io_data(err);
        written = size;
      }

      ptr = (short *) output_buffer;
    }

    int d_start, d_end;
    d_start = data_written;
    data_written += written;
    d_end = data_written;

    calcfade();

    if (tag_song_ms && d_end > song_len && no_loop) {
      short *foo = output_buffer;
      int n;
      for (n = d_start; n < d_end; ++n) {
        if (n > song_len) {
          if (n > song_len + fade_len) {
            foo[0] = 0;
            foo[1] = 0;
          } else {
            int bleh = song_len + fade_len - n;
            foo[0] = MulDiv(foo[0], bleh, fade_len);
            foo[1] = MulDiv(foo[1], bleh, fade_len);
          }
        }
        foo += 2;
      }
    }

    return written;
  }

  void decode_seek(double p_seconds) {
    eof = false;

    double usfemu_pos = 0.0;

    if (sample_rate) {
      usfemu_pos = double(data_written) / double(sample_rate);

      if (do_suppressendsilence) {
        double buffered_time = (double) (m_buffer.data_available() / 2) / (double) (sample_rate);

        m_buffer.reset();

        usfemu_pos += buffered_time;
      }
    }

    last_sample_rate = 0;

    if (p_seconds < usfemu_pos) {
      usf_restart(m_state->emu_state);
      usfemu_pos = -startsilence;
      data_written = 0;
    }

//usf_set_seeking(m_state->emu_state, true);

    p_seconds -= usfemu_pos;

    // more abortable, and emu doesn't like doing huge numbers of samples per call anyway
    while (p_seconds) {
      const char *err = resample ? usf_render_resampled(m_state->emu_state, seek_buffer, SEEK_BUF_SIZE, sample_rate)
                                 : usf_render(m_state->emu_state, seek_buffer, SEEK_BUF_SIZE, &sample_rate);
      if (err != 0)
        throw exception_io_data(err);

      usfemu_pos += ((double) SEEK_BUF_SIZE) / double(sample_rate);

      data_written += SEEK_BUF_SIZE;

      p_seconds -= ((double) SEEK_BUF_SIZE) / double(sample_rate);

      if (p_seconds < 0) {
        remainder = (unsigned int) (-p_seconds * double(sample_rate));

        data_written -= remainder;

        memmove(seek_buffer, &seek_buffer[(SEEK_BUF_SIZE - remainder) * 2], remainder * 2 * sizeof(int16_t));

        break;
      }
    }

    if (do_suppressendsilence && remainder) {
      unsigned long count;
      t_int16 *ptr = m_buffer.get_write_ptr(count);
      memcpy(ptr, seek_buffer, remainder * sizeof(t_int16) * 2);
      m_buffer.samples_written(remainder * 2);
      remainder = 0;
    }

//usf_set_seeking(m_state->emu_state, false);
  }

private:
  double MulDiv(int ms, int sampleRate, int d) {
    return ((double) ms) * sampleRate / d;
  }

  void calcfade() {
    song_len = MulDiv(tag_song_ms, sample_rate, 1000);
    fade_len = MulDiv(tag_fade_ms, sample_rate, 1000);
  }
};

static input_usf g_input_usf;

#ifdef __cplusplus
extern "C" {
#endif
int32_t n64_load_file(const char *uri, int16_t *output_buffer, uint16_t outSize) {
  try {
    int retVal = g_input_usf.open(uri);
    if (retVal < 0) return retVal;  // trigger retry later

    g_input_usf.decode_initialize(output_buffer, outSize);

    return 0;
  } catch (...) {
    return -1;
  }
}

int32_t n64_get_duration_ms() {
  return 1000.0 * g_input_usf.getSamplesToPlay() / g_input_usf.getSamplesRate();
}

int32_t n64_get_position_ms() {
  return 1000.0 * g_input_usf.getDataWritten() / g_input_usf.getSamplesRate();
}

int32_t n64_render_audio(int16_t *output_buffer, uint16_t outSize) {
  return g_input_usf.decode_run(output_buffer, outSize);
}

void n64_seek_ms(int msec) {
  g_input_usf.decode_seek(((double) msec) / 1000.0);
}

void n64_shutdown() {
  g_input_usf.shutdown();
}

#ifdef __cplusplus
}
#endif


