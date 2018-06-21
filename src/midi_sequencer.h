/*
 * BW_Midi_Sequencer - MIDI Sequencer for C++
 *
 * Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifndef BISQUIT_AND_WOHLSTANDS_MIDI_SEQUENCER_HHHH
#define BISQUIT_AND_WOHLSTANDS_MIDI_SEQUENCER_HHHH

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct BW_MidiRtInterface
{
    //! Raw MIDI event hook
    typedef void (*RawEventHook)(void *userdata, uint8_t type, uint8_t subtype, uint8_t channel, const uint8_t *data, size_t len);
    RawEventHook onEvent;
    void         *onEvent_userData;

    //! Library internal debug messages
    typedef void (*DebugMessageHook)(void *userdata, const char *fmt, ...);
    DebugMessageHook onDebugMessage;
    void *onDebugMessage_userData;

    void *rtUserData;

    /* Standard MIDI events. All of them are required! */
    typedef void (*RtNoteOn)(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity);
    RtNoteOn            rt_noteOn;

    typedef void (*RtNoteOff)(void *userdata, uint8_t channel, uint8_t note);
    RtNoteOff           rt_noteOff;

    typedef void (*RtNoteAfterTouch)(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal);
    RtNoteAfterTouch    rt_noteAfterTouch;

    typedef void (*RtChannelAfterTouch)(void *userdata, uint8_t channel, uint8_t atVal);
    RtChannelAfterTouch rt_channelAfterTouch;

    typedef void (*RtControlerChange)(void *userdata, uint8_t channel, uint8_t type, uint8_t value);
    RtControlerChange   rt_controllerChange;

    typedef void (*RtPatchChange)(void *userdata, uint8_t channel, uint8_t patch);
    RtPatchChange       rt_patchChange;

    typedef void (*RtPitchBend)(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb);
    RtPitchBend         rt_pitchBend;

    typedef void (*RtSysEx)(void *userdata, const uint8_t *msg, size_t size);
    RtSysEx             rt_systemExclusive;


    /* NonStandard events. There are optional */
    typedef void (*RtRawOPL)(void *userdata, uint8_t reg, uint8_t value);
    RtRawOPL            rt_rawOPL;

    typedef void (*RtDeviceSwitch)(void *userdata, size_t track, const char *data, size_t length);
    RtDeviceSwitch      rt_deviceSwitch;

    typedef uint64_t (*RtCurrentDevice)(void *userdata, size_t track);
    RtCurrentDevice     rt_currentDevice;
};

#ifdef __cplusplus
}
#endif

#endif /* BISQUIT_AND_WOHLSTANDS_MIDI_SEQUENCER_HHHH */
