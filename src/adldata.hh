/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifdef ADLMIDI_buildAsApp
#include <SDL2/SDL.h>
class MutexType
{
    SDL_mutex* mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType() { SDL_DestroyMutex(mut); }
    void Lock() { SDL_mutexP(mut); }
    void Unlock() { SDL_mutexV(mut); }
};
#endif

extern const struct adldata
{
    unsigned int modulator_E862, carrier_E862;  // See below
    unsigned char modulator_40, carrier_40; // KSL/attenuation settings
    unsigned char feedconn; // Feedback/connection bits for the channel

    signed char finetune;
} adl[];

extern const struct adlinsdata
{
    enum { Flag_Pseudo4op = 0x01, Flag_NoSound = 0x02 };

    unsigned short adlno1, adlno2;
    unsigned char tone;
    unsigned char flags;
    unsigned short ms_sound_kon;  // Number of milliseconds it produces sound;
    unsigned short ms_sound_koff;
} adlins[];
extern const unsigned short banks[][256];
extern const char* const banknames[66];
