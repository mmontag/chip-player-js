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

#include "audio.h"

#include <adlmidi.h>
#include <malloc.h>
#include <windows.h>
#include <mmreg.h>
#include <mmsystem.h>

#include <stdio.h>

#define BUFFERS_COUNT 2
static HWAVEOUT     g_waveOut;
static WAVEHDR      g_hdr[BUFFERS_COUNT];
static int          g_started = 0;
static void        *g_audioLock = NULL;
static const char  *g_lastErrorMessage = "";
static UINT8       *g_buffer = NULL;
static size_t       g_bufferSize = 0;

static HANDLE       g_event;
static HANDLE       g_thread;

static AudioOutputCallback g_audioCallback = NULL;

static DWORD WINAPI s_audioThread(PVOID pDataInput)
{
    DWORD ret = 0;
    UINT i;
    (void)pDataInput;

    while(g_started)
    {
        WaitForSingleObject(g_event, INFINITE);
        for(i = 0; i < BUFFERS_COUNT; i++)
        {
            if(g_hdr[i].dwFlags & WHDR_DONE)
            {
                g_audioCallback(NULL, (UINT8*)g_hdr[i].lpData, (int)g_hdr[i].dwBufferLength);
                audio_lock();
                waveOutWrite(g_waveOut, &g_hdr[i], sizeof(WAVEHDR));
                audio_unlock();
            }
        }
    }

    waveOutReset(g_waveOut);

    return ret;
}

static void CALLBACK s_waveOutProc(HWAVEOUT waveout, UINT msg, DWORD_PTR userData, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)waveout;
    (void)userData;
    (void)p1;
    (void)p2;

    switch (msg)
    {
    case WOM_DONE:
        SetEvent(g_event);
    }
}

int audio_init(struct AudioOutputSpec *in_spec, struct AudioOutputSpec *out_obtained, AudioOutputCallback callback)
{
    WORD bits = 16;
    WAVEFORMATEX wformat;
    MMRESULT result;

    g_waveOut = NULL;
    g_started = 0;
    memset(g_hdr, 0, sizeof(WAVEHDR) * BUFFERS_COUNT);

    g_audioCallback = callback;

    switch(in_spec->format)
    {
    case ADLMIDI_SampleType_S8:
        bits = 8; break;
    case ADLMIDI_SampleType_U8:
        bits = 8; break;
    case ADLMIDI_SampleType_S16:
        bits = 16; break;
    case ADLMIDI_SampleType_U16:
        bits = 16; break;
    case ADLMIDI_SampleType_S32:
        bits = 32; break;
    case ADLMIDI_SampleType_F32:
        bits = 32; break;
    }

    g_bufferSize = in_spec->samples * (bits / 8) * in_spec->channels;
    g_buffer = (UINT8 *)malloc(g_bufferSize * BUFFERS_COUNT);
    if(!g_buffer)
    {
        g_bufferSize = 0;
        g_lastErrorMessage = "Out of memory";
        return -1;
    }

    out_obtained->channels = in_spec->channels;
    out_obtained->format = in_spec->format;
    out_obtained->freq = in_spec->freq;
    out_obtained->is_msb = in_spec->is_msb;
    out_obtained->samples = in_spec->samples;

    memset(&wformat, 0, sizeof(wformat));
    wformat.cbSize = sizeof(WAVEFORMATEX);
    wformat.nChannels       = (WORD)in_spec->channels;
    wformat.nSamplesPerSec  = (WORD)in_spec->freq;
    wformat.wFormatTag      = WAVE_FORMAT_PCM;
    wformat.wBitsPerSample  = bits;
    wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
    wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;

    result = waveOutOpen(&g_waveOut, WAVE_MAPPER, &wformat, (DWORD_PTR)s_waveOutProc, 0, CALLBACK_FUNCTION);
    if(result != MMSYSERR_NOERROR)
    {
        g_lastErrorMessage = "Could not open the audio device";
        return -1;
    }
    waveOutPause(g_waveOut);
    g_audioLock = audio_mutex_create();

    return 0;
}

void audio_close(void)
{
    audio_stop();
    audio_mutex_lock(g_audioLock);
    if(g_waveOut)
        waveOutClose(g_waveOut);
    free(g_buffer);
    g_waveOut = NULL;
    g_audioCallback = NULL;
    audio_mutex_unlock(g_audioLock);
    audio_mutex_destroy(g_audioLock);
}

const char *audio_get_error(void)
{
    return g_lastErrorMessage;
}

void audio_start(void)
{
    DWORD dwThreadId;
    size_t i = 0;
    if(!g_audioCallback)
        return;
    if(g_started)
        return;

    audio_lock();
    memset(g_buffer, 0, g_bufferSize * BUFFERS_COUNT);
    memset(g_hdr, 0, sizeof(WAVEHDR) * BUFFERS_COUNT);

    for(i = 0; i < BUFFERS_COUNT; i++)
    {
        g_hdr[i].dwBufferLength = (DWORD)g_bufferSize;
        g_hdr[i].lpData = (LPSTR)(g_buffer + (g_bufferSize * i));
        waveOutPrepareHeader(g_waveOut, &g_hdr[i], sizeof(WAVEHDR));
        g_audioCallback(NULL, (UINT8*)g_hdr[i].lpData, (int)g_hdr[i].dwBufferLength);
        waveOutWrite(g_waveOut, &g_hdr[i], sizeof(WAVEHDR));
    }

    waveOutRestart(g_waveOut);

    audio_unlock();

    g_started = 1;

    g_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    g_thread = CreateThread(NULL, 0, s_audioThread, 0, 0, &dwThreadId);
}

void audio_stop(void)
{
    audio_lock();
    g_started = 0;
    if(g_thread)
    {
        SetEvent(g_event);
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_event);
        CloseHandle(g_thread);
    }
    audio_unlock();
}

void audio_lock(void)
{
    audio_mutex_lock(g_audioLock);
}

void audio_unlock(void)
{
    audio_mutex_unlock(g_audioLock);
}

void audio_delay(unsigned int ms)
{
    Sleep(ms);
}

void* audio_mutex_create(void)
{
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(mutex);
    return mutex;
}

void audio_mutex_destroy(void *m)
{
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)m;
    DeleteCriticalSection(mutex);
    free(mutex);
}

void audio_mutex_lock(void *m)
{
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)m;
    EnterCriticalSection(mutex);
}

void audio_mutex_unlock(void* m)
{
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)m;
    LeaveCriticalSection(mutex);
}
