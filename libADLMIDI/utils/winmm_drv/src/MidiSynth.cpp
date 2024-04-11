/* Copyright (C) 2011, 2012 Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"

namespace OPL3Emu
{

#define DRIVER_MODE

static MidiSynth &midiSynth = MidiSynth::getInstance();

static class MidiStream
{
private:
    static const unsigned int maxPos = 1024;
    unsigned int startpos;
    unsigned int endpos;
    DWORD stream[maxPos][2];

public:
    MidiStream()
    {
        startpos = 0;
        endpos = 0;
    }

    DWORD PutMessage(DWORD msg, DWORD timestamp)
    {
        unsigned int newEndpos = endpos;

        newEndpos++;
        if(newEndpos == maxPos)  // check for buffer rolloff
            newEndpos = 0;
        if(startpos == newEndpos)  // check for buffer full
            return -1;
        stream[endpos][0] = msg;    // ok to put data and update endpos
        stream[endpos][1] = timestamp;
        endpos = newEndpos;
        return 0;
    }

    DWORD GetMidiMessage()
    {
        if(startpos == endpos)  // check for buffer empty
            return -1;
        DWORD msg = stream[startpos][0];
        startpos++;
        if(startpos == maxPos)  // check for buffer rolloff
            startpos = 0;
        return msg;
    }

    DWORD PeekMessageTime()
    {
        if(startpos == endpos)  // check for buffer empty
            return (DWORD) -1;
        return stream[startpos][1];
    }

    DWORD PeekMessageTimeAt(unsigned int pos)
    {
        if(startpos == endpos)  // check for buffer empty
            return -1;
        unsigned int peekPos = (startpos + pos) % maxPos;
        return stream[peekPos][1];
    }

    void Clean()
    {
        startpos = 0;
        endpos = 0;
        memset(stream, 0, sizeof(stream));
    }

} midiStream;

static class SynthEventWin32
{
private:
    HANDLE hEvent;

public:
    int Init()
    {
        hEvent = CreateEvent(NULL, false, true, NULL);
        if(hEvent == NULL)
        {
            MessageBoxW(NULL, L"Can't create sync object", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 1;
        }
        return 0;
    }

    void Close()
    {
        CloseHandle(hEvent);
    }

    void Wait()
    {
        WaitForSingleObject(hEvent, INFINITE);
    }

    void Release()
    {
        SetEvent(hEvent);
    }
} synthEvent;

static class WaveOutWin32
{
private:
    HWAVEOUT    hWaveOut;
    WAVEHDR     *WaveHdr;
    HANDLE      hEvent;
    DWORD       chunks;
    DWORD       prevPlayPos;
    DWORD       getPosWraps;
    bool        stopProcessing;

public:
    int Init(Bit16s *buffer, unsigned int bufferSize, unsigned int chunkSize, bool useRingBuffer, unsigned int sampleRate)
    {
        DWORD callbackType = CALLBACK_NULL;
        DWORD_PTR callback = (DWORD_PTR)NULL;
        hEvent = NULL;
        if(!useRingBuffer)
        {
            hEvent = CreateEvent(NULL, false, true, NULL);
            callback = (DWORD_PTR)hEvent;
            callbackType = CALLBACK_EVENT;
        }

        PCMWAVEFORMAT wFormat = {WAVE_FORMAT_PCM, 2, sampleRate, sampleRate * 4, 4, 16};

        // Open waveout device
        int wResult = waveOutOpen(&hWaveOut, WAVE_MAPPER, (LPWAVEFORMATEX)&wFormat, callback, (DWORD_PTR)&midiSynth, callbackType);
        if(wResult != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to open waveform output device", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 2;
        }

        // Prepare headers
        chunks = useRingBuffer ? 1 : bufferSize / chunkSize;
        WaveHdr = new WAVEHDR[chunks];
        LPSTR chunkStart = (LPSTR)buffer;
        DWORD chunkBytes = 4 * chunkSize;
        for(UINT i = 0; i < chunks; i++)
        {
            if(useRingBuffer)
            {
                WaveHdr[i].dwBufferLength = 4 * bufferSize;
                WaveHdr[i].lpData = chunkStart;
                WaveHdr[i].dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
                WaveHdr[i].dwLoops = -1L;
            }
            else
            {
                WaveHdr[i].dwBufferLength = chunkBytes;
                WaveHdr[i].lpData = chunkStart;
                WaveHdr[i].dwFlags = 0L;
                WaveHdr[i].dwLoops = 0L;
                chunkStart += chunkBytes;
            }
            wResult = waveOutPrepareHeader(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR));
            if(wResult != MMSYSERR_NOERROR)
            {
                MessageBoxW(NULL, L"Failed to Prepare Header", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
                return 3;
            }
        }
        stopProcessing = false;
        return 0;
    }

    int Close()
    {
        stopProcessing = true;
        SetEvent(hEvent);
        int wResult = waveOutReset(hWaveOut);
        if(wResult != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to Reset WaveOut", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 8;
        }

        for(UINT i = 0; i < chunks; i++)
        {
            wResult = waveOutUnprepareHeader(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR));
            if(wResult != MMSYSERR_NOERROR)
            {
                MessageBoxW(NULL, L"Failed to Unprepare Wave Header", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
                return 8;
            }
        }
        delete[] WaveHdr;
        WaveHdr = NULL;

        wResult = waveOutClose(hWaveOut);
        if(wResult != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to Close WaveOut", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 8;
        }
        if(hEvent != NULL)
        {
            CloseHandle(hEvent);
            hEvent = NULL;
        }
        return 0;
    }

    int Start()
    {
        getPosWraps = 0;
        prevPlayPos = 0;
        for(UINT i = 0; i < chunks; i++)
        {
            if(waveOutWrite(hWaveOut, &WaveHdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            {
                MessageBoxW(NULL, L"Failed to write block to device", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
                return 4;
            }
        }
        _beginthread(RenderingThread, 16384, this);
        return 0;
    }

    int Pause()
    {
        if(waveOutPause(hWaveOut) != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to Pause wave playback", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 9;
        }
        return 0;
    }

    int Resume()
    {
        if(waveOutRestart(hWaveOut) != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to Resume wave playback", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 9;
        }
        return 0;
    }

    UINT64 GetPos()
    {
        MMTIME mmTime;
        mmTime.wType = TIME_SAMPLES;

        if(waveOutGetPosition(hWaveOut, &mmTime, sizeof(MMTIME)) != MMSYSERR_NOERROR)
        {
            MessageBoxW(NULL, L"Failed to get current playback position", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 10;
        }

        if(mmTime.wType != TIME_SAMPLES)
        {
            MessageBoxW(NULL, L"Failed to get # of samples played", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
            return 10;
        }

        // Deal with waveOutGetPosition() wraparound. For 16-bit stereo output, it equals 2^27,
        // presumably caused by the internal 32-bit counter of bits played.
        // The output of that nasty waveOutGetPosition() isn't monotonically increasing
        // even during 2^27 samples playback, so we have to ensure the difference is big enough...
        int delta = mmTime.u.sample - prevPlayPos;
        if(delta < -(1 << 26))
        {
            std::cout << "OPL3: GetPos() wrap: " << delta << "\n";
            ++getPosWraps;
        }
        prevPlayPos = mmTime.u.sample;
        return mmTime.u.sample + getPosWraps * (1 << 27);
    }

    static void RenderingThread(void *);
} s_waveOut;

void WaveOutWin32::RenderingThread(void *)
{
    if(s_waveOut.chunks == 1)
    {
        // Rendering using single looped ring buffer
        while(!s_waveOut.stopProcessing)
            midiSynth.RenderAvailableSpace();
    }
    else
    {
        while(!s_waveOut.stopProcessing)
        {
            bool allBuffersRendered = true;
            for(UINT i = 0; i < s_waveOut.chunks; i++)
            {
                if(s_waveOut.WaveHdr[i].dwFlags & WHDR_DONE)
                {
                    allBuffersRendered = false;
                    midiSynth.Render((Bit16s *)s_waveOut.WaveHdr[i].lpData, s_waveOut.WaveHdr[i].dwBufferLength / 4);
                    if(waveOutWrite(s_waveOut.hWaveOut, &s_waveOut.WaveHdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                        MessageBoxW(NULL, L"Failed to write block to device", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
                    midiSynth.CheckForSignals();
                }
            }
            if(allBuffersRendered)
                WaitForSingleObject(s_waveOut.hEvent, INFINITE);
        }
    }
}


MidiSynth::MidiSynth() :
    synth(NULL)
{
    m_setupInit = false;
    setupDefault(&m_setup);
    loadSetup();
    ::openSignalListener();
}

MidiSynth::~MidiSynth()
{
    if(synth)
        adl_close(synth);
    synth = NULL;
    ::closeSignalListener();
}

MidiSynth &MidiSynth::getInstance()
{
    static MidiSynth *instance = new MidiSynth;
    return *instance;
}

// Renders all the available space in the single looped ring buffer
void MidiSynth::RenderAvailableSpace()
{
    DWORD playPos = s_waveOut.GetPos() % bufferSize;
    DWORD framesToRender;

    if(playPos < framesRendered)
    {
        // Buffer wrap, render 'till the end of the buffer
        framesToRender = bufferSize - framesRendered;
    }
    else
    {
        framesToRender = playPos - framesRendered;
        if(framesToRender < chunkSize)
        {
            Sleep(1 + (chunkSize - framesToRender) * 1000 / sampleRate);
            return;
        }
    }
    midiSynth.Render(buffer + 2 * framesRendered, framesToRender);
}

// Renders totalFrames frames starting from bufpos
// The number of frames rendered is added to the global counter framesRendered
void MidiSynth::Render(Bit16s *bufpos, DWORD totalFrames)
{
    while(totalFrames > 0)
    {
        DWORD timeStamp;
        // Incoming MIDI messages timestamped with the current audio playback position + midiLatency
        while((timeStamp = midiStream.PeekMessageTime()) == framesRendered)
        {
            DWORD msg = midiStream.GetMidiMessage();

            synthEvent.Wait();

            Bit8u event = msg & 0xFF;
            Bit8u channel = msg & 0x0F;
            Bit8u p1 = (msg >> 8) & 0x7f;
            Bit8u p2 = (msg >> 16) & 0x7f;

            event &= 0xF0;

            if(event == 0xF0)
            {
                switch (channel)
                {
                case 0xF:
                    adl_reset(synth);
                    break;
                }
            }

            switch(event & 0xF0)
            {
            case 0x80:
                adl_rt_noteOff(synth, channel, p1);
                break;
            case 0x90:
                adl_rt_noteOn(synth, channel, p1, p2);
                break;
            case 0xA0:
                adl_rt_noteAfterTouch(synth, channel, p1, p2);
                break;
            case 0xB0:
                adl_rt_controllerChange(synth, channel, p1, p2);
                break;
            case 0xC0:
                adl_rt_patchChange(synth, channel, p1);
                break;
            case 0xD0:
                adl_rt_channelAfterTouch(synth, channel, p1);
                break;
            case 0xE0:
                adl_rt_pitchBendML(synth, channel, p2, p1);
                break;
            }

            synthEvent.Release();
        }

        // Find out how many frames to render. The value of timeStamp == -1 indicates the MIDI buffer is empty
        DWORD framesToRender = timeStamp - framesRendered;
        if(framesToRender > totalFrames)
        {
            // MIDI message is too far - render the rest of frames
            framesToRender = totalFrames;
        }

        synthEvent.Wait();
        adl_generate(synth, framesToRender * 2, bufpos);
        synthEvent.Release();
        framesRendered += framesToRender;
        bufpos += 2 * framesToRender; // each frame consists of two samples for both the Left and Right channels
        totalFrames -= framesToRender;
    }

    // Wrap framesRendered counter
    if(framesRendered >= bufferSize)
        framesRendered -= bufferSize;
}

void MidiSynth::CheckForSignals()
{
    int cmd = ::hasReloadSetupSignal();

    if(cmd == 0)
        return;

    switch(cmd)
    {
    case 1: // Reload settings on the fly
        this->loadSetup();
        LoadSynthSetup();
        break;

    case 2:
        adl_reset(synth);
        break;

    default:
        break;
    }

    if(cmd > 0)
        ::resetSignal();
}

unsigned int MidiSynth::MillisToFrames(unsigned int millis)
{
    return UINT(sampleRate * millis / 1000.f);
}

void MidiSynth::LoadSettings()
{
    sampleRate = 49716;
    bufferSize = MillisToFrames(100);
    chunkSize = MillisToFrames(10);
    midiLatency = MillisToFrames(0);
    useRingBuffer = false;
    if(!useRingBuffer)
    {
        // Number of chunks should be ceil(bufferSize / chunkSize)
        DWORD chunks = (bufferSize + chunkSize - 1) / chunkSize;
        // Refine bufferSize as chunkSize * number of chunks, no less then the specified value
        bufferSize = chunks * chunkSize;
    }
}

int MidiSynth::Init()
{
    LoadSettings();
    buffer = new Bit16s[2 * bufferSize]; // each frame consists of two samples for both the Left and Right channels

    // Init synth
    if(synthEvent.Init())
        return 1;

    synth = adl_init(49716);
    if(!synth)
    {
        MessageBoxW(NULL, L"Can't open Synth", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    m_setupInit = false;
    LoadSynthSetup();

    UINT wResult = s_waveOut.Init(buffer, bufferSize, chunkSize, useRingBuffer, sampleRate);
    if(wResult) return wResult;

    // Start playing stream
    adl_generate(synth, bufferSize * 2, buffer);
    framesRendered = 0;

    wResult = s_waveOut.Start();
    return wResult;
}

int MidiSynth::Reset()
{
#ifdef DRIVER_MODE
    return 0;
#endif

    UINT wResult = s_waveOut.Pause();
    if(wResult) return wResult;

    synthEvent.Wait();

    if(synth)
        adl_close(synth);

    synth = adl_init(49716);
    if(!synth)
    {
        MessageBoxW(NULL, L"Can't open Synth", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    m_setupInit = false;
    LoadSynthSetup();

    synthEvent.Release();

    wResult = s_waveOut.Resume();
    return wResult;
}

void MidiSynth::ResetSynth()
{
    synthEvent.Wait();
    adl_reset(synth);
    midiStream.Clean();
    synthEvent.Release();
}

void MidiSynth::PanicSynth()
{
    synthEvent.Wait();
    adl_panic(synth);
    synthEvent.Release();
}

void MidiSynth::PushMIDI(DWORD msg)
{
    midiStream.PutMessage(msg, (s_waveOut.GetPos() + midiLatency) % bufferSize);
}

void MidiSynth::PlaySysex(Bit8u *bufpos, DWORD len)
{
    synthEvent.Wait();
    adl_rt_systemExclusive(synth, bufpos, len);
    synthEvent.Release();
}

void MidiSynth::loadSetup()
{
    ::loadSetup(&m_setup);
}

void MidiSynth::LoadSynthSetup()
{
    if(!m_setupInit || m_setupCurrent.emulatorId != m_setup.emulatorId)
    {
        adl_switchEmulator(synth, m_setup.emulatorId);
        m_setupCurrent.emulatorId = m_setup.emulatorId;
    }

    if(!m_setupInit || m_setupCurrent.numChips != m_setup.numChips)
    {
        adl_setNumChips(synth, m_setup.numChips);
        m_setupCurrent.numChips = m_setup.numChips;
    }

    if(!m_setupInit || m_setupCurrent.flagDeepTremolo != m_setup.flagDeepTremolo)
    {
        switch(m_setup.flagDeepTremolo)
        {
        case BST_INDETERMINATE:
            adl_setHTremolo(synth, -1);
            break;
        case BST_CHECKED:
            adl_setHTremolo(synth, 1);
            break;
        case BST_UNCHECKED:
            adl_setHTremolo(synth, 0);
            break;
        }
        m_setupCurrent.flagDeepTremolo = m_setup.flagDeepTremolo;
    }

    if(!m_setupInit || m_setupCurrent.flagDeepVibrato != m_setup.flagDeepVibrato)
    {
        switch(m_setup.flagDeepVibrato)
        {
        case BST_INDETERMINATE:
            adl_setHVibrato(synth, -1);
            break;
        case BST_CHECKED:
            adl_setHVibrato(synth, 1);
            break;
        case BST_UNCHECKED:
            adl_setHVibrato(synth, 0);
            break;
        }
        m_setupCurrent.flagDeepVibrato = m_setup.flagDeepVibrato;
    }

    if(!m_setupInit || m_setupCurrent.flagSoftPanning != m_setup.flagSoftPanning)
    {
        adl_setSoftPanEnabled(synth, m_setup.flagSoftPanning);
        m_setupCurrent.flagSoftPanning = m_setup.flagSoftPanning;
    }


    if(!m_setupInit || m_setupCurrent.flagScaleModulators != m_setup.flagScaleModulators)
    {
        adl_setScaleModulators(synth, m_setup.flagScaleModulators);
        m_setupCurrent.flagScaleModulators = m_setup.flagScaleModulators;
    }

    if(!m_setupInit || m_setupCurrent.flagFullBrightness != m_setup.flagFullBrightness)
    {
        adl_setFullRangeBrightness(synth, m_setup.flagFullBrightness);
        m_setupCurrent.flagFullBrightness = m_setup.flagFullBrightness;
    }

    if(!m_setupInit || m_setupCurrent.volumeModel != m_setup.volumeModel)
    {
        adl_setVolumeRangeModel(synth, m_setup.volumeModel);
        m_setupCurrent.volumeModel = m_setup.volumeModel;
    }

    if(!m_setupInit || m_setupCurrent.numChips != m_setup.numChips)
    {
        adl_setNumChips(synth, m_setup.numChips);
        m_setupCurrent.numChips = m_setup.numChips;
    }

    if(!m_setupInit || m_setupCurrent.num4ops != m_setup.num4ops)
    {
        adl_setNumFourOpsChn(synth, m_setup.num4ops);
        m_setupCurrent.num4ops = m_setup.num4ops;
    }

    if(!m_setupInit ||
       m_setupCurrent.useExternalBank != m_setup.useExternalBank ||
       m_setupCurrent.bankId != m_setup.bankId ||
       wcscmp(m_setupCurrent.bankPath, m_setup.bankPath) != 0
    )
    {
        if(m_setup.useExternalBank)
        {
            char pathUtf8[MAX_PATH * 4];
            ZeroMemory(pathUtf8, MAX_PATH * 4);
            int len = WideCharToMultiByte(CP_UTF8, 0, m_setup.bankPath, wcslen(m_setup.bankPath), pathUtf8, MAX_PATH * 4, 0, 0);
            pathUtf8[len] = '\0';
            adl_openBankFile(synth, pathUtf8);
        }
        else
            adl_setBank(synth, m_setup.bankId);

        m_setupCurrent.useExternalBank = m_setup.useExternalBank;
        m_setupCurrent.bankId = m_setup.bankId;
        wcscpy(m_setupCurrent.bankPath, m_setup.bankPath);
    }

    m_setupInit = true;
}

void MidiSynth::Close()
{
    s_waveOut.Pause();
    s_waveOut.Close();
    synthEvent.Wait();
    //synth->close();

    // Cleanup memory
    if(synth)
        adl_close(synth);
    synth = NULL;
    delete buffer;

    synthEvent.Close();
}

}
