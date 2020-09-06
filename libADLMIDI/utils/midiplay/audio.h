/*
 * ADLMIDI Player is a free MIDI player based on a libADLMIDI,
 * a Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*AudioOutputCallback)(void *, unsigned char *stream, int len);

struct AudioOutputSpec
{
    unsigned int freq;
    unsigned short format;
    unsigned short is_msb;
    unsigned short samples;
    unsigned char  channels;
};

extern int audio_init(struct AudioOutputSpec *in_spec, struct AudioOutputSpec *out_obtained, AudioOutputCallback callback);

extern void audio_close(void);

extern const char* audio_get_error(void);

extern void audio_start(void);

extern void audio_stop(void);

extern void audio_lock(void);

extern void audio_unlock(void);

extern void audio_delay(unsigned int ms);

extern void* audio_mutex_create(void);
extern void  audio_mutex_destroy(void *m);
extern void  audio_mutex_lock(void *m);
extern void  audio_mutex_unlock(void *m);

#ifdef __cplusplus
}
#endif
