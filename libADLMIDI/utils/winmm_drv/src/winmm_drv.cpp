/* Copyright (C) 2003, 2004, 2005 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011, 2012 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
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

#define MAX_DRIVERS 8
#define MAX_CLIENTS 8 // Per driver

#ifdef __MINGW32__

#if !defined(MM_MPU401_MIDIOUT) && !defined(MM_MPU401_MIDIOUT)
typedef struct tagMIDIOUTCAPS2A {
    WORD wMid;
    WORD wPid;
    MMVERSION vDriverVersion;
    CHAR szPname[MAXPNAMELEN];
    WORD wTechnology;
    WORD wVoices;
    WORD wNotes;
    WORD wChannelMask;
    DWORD dwSupport;
    GUID ManufacturerGuid;
    GUID ProductGuid;
    GUID NameGuid;
} MIDIOUTCAPS2A,*PMIDIOUTCAPS2A,*NPMIDIOUTCAPS2A,*LPMIDIOUTCAPS2A;

typedef struct tagMIDIOUTCAPS2W {
    WORD wMid;
    WORD wPid;
    MMVERSION vDriverVersion;
    WCHAR szPname[MAXPNAMELEN];
    WORD wTechnology;
    WORD wVoices;
    WORD wNotes;
    WORD wChannelMask;
    DWORD dwSupport;
    GUID ManufacturerGuid;
    GUID ProductGuid;
    GUID NameGuid;
} MIDIOUTCAPS2W,*PMIDIOUTCAPS2W,*NPMIDIOUTCAPS2W,*LPMIDIOUTCAPS2W;
#endif

#ifndef MM_UNMAPPED
#define MM_UNMAPPED 0xffff
#endif
#ifndef MM_MPU401_MIDIOUT
#define MM_MPU401_MIDIOUT 10
#endif

//BOOL APIENTRY DriverCallback(DWORD_PTR dwCallback,
//                             DWORD dwFlags,
//                             HDRVR hDevice,
//                             DWORD dwMsg,
//                             DWORD_PTR dwUser,
//                             DWORD_PTR dwParam1,
//                             DWORD_PTR dwParam2);

typedef BOOL (APIENTRY *DriverCallbackPtr)(DWORD_PTR dwCallback,
                                           DWORD dwFlags,
                                           HDRVR hDevice,
                                           DWORD dwMsg,
                                           DWORD_PTR dwUser,
                                           DWORD_PTR dwParam1,
                                           DWORD_PTR dwParam2);

static HMODULE           s_winmm_dll = NULL;
static DriverCallbackPtr s_DriverCallback = NULL;

#define DriverCallback s_DriverCallback

static void initWorkarounds()
{
    s_winmm_dll = LoadLibraryW(L"winmm.dll");
    if(s_winmm_dll)
    {
        s_DriverCallback = (DriverCallbackPtr)GetProcAddress(s_winmm_dll, "DriverCallback");
        if(!s_DriverCallback)
            MessageBoxW(NULL, L"Failed to get a workaround for DriverCallback", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
    }
    else
    {
        MessageBoxW(NULL, L"Failed to open a library for a workaround for DriverCallback", L"libADLMIDI", MB_OK | MB_ICONEXCLAMATION);
    }
}

static void freeWorkarounds()
{
    if(s_winmm_dll)
        FreeLibrary(s_winmm_dll);
    s_winmm_dll = NULL;
    s_DriverCallback = NULL;
}
#endif

static OPL3Emu::MidiSynth &midiSynth = OPL3Emu::MidiSynth::getInstance();
static bool synthOpened = false;
//static HWND hwnd = NULL;
static int driverCount;

struct Driver
{
    bool open;
    int clientCount;
    HDRVR hdrvr;
    struct Client
    {
        bool allocated;
        DWORD_PTR instance;
        DWORD flags;
        DWORD_PTR callback;
        DWORD synth_instance;
    } clients[MAX_CLIENTS];
} drivers[MAX_DRIVERS];

EXTERN_C LONG __declspec(dllexport) __stdcall DriverProc(DWORD dwDriverID, HDRVR hdrvr, UINT wMessage, LONG dwParam1, LONG dwParam2)
{
    (void)dwDriverID; (void)dwParam1; (void)dwParam2;

    switch(wMessage)
    {
    case DRV_LOAD:
#ifdef __MINGW32__
        initWorkarounds();
#endif
        memset(drivers, 0, sizeof(drivers));
        driverCount = 0;
        return DRV_OK;
    case DRV_ENABLE:
        return DRV_OK;
    case DRV_OPEN:
        int driverNum;
        if(driverCount == MAX_DRIVERS)
            return 0;
        else
        {
            for(driverNum = 0; driverNum < MAX_DRIVERS; driverNum++)
            {
                if(!drivers[driverNum].open)
                    break;
                if(driverNum == MAX_DRIVERS)
                    return 0;
            }
        }
        drivers[driverNum].open = true;
        drivers[driverNum].clientCount = 0;
        drivers[driverNum].hdrvr = hdrvr;
        driverCount++;
        return DRV_OK;
    case DRV_INSTALL:
    case DRV_PNPINSTALL:
        return DRV_OK;
    case DRV_QUERYCONFIGURE:
        return 0;
    case DRV_CONFIGURE:
        return DRVCNF_OK;
    case DRV_CLOSE:
        for(int i = 0; i < MAX_DRIVERS; i++)
        {
            if(drivers[i].open && drivers[i].hdrvr == hdrvr)
            {
                drivers[i].open = false;
                --driverCount;
                return DRV_OK;
            }
        }
        return DRV_CANCEL;
    case DRV_DISABLE:
        return DRV_OK;
    case DRV_FREE:
#ifdef __MINGW32__
        freeWorkarounds();
#endif
        return DRV_OK;
    case DRV_REMOVE:
        return DRV_OK;
    }
    return DRV_OK;
}


EXTERN_C HRESULT modGetCaps(PVOID capsPtr, DWORD capsSize)
{
    MIDIOUTCAPSA *myCapsA;
    MIDIOUTCAPSW *myCapsW;
    MIDIOUTCAPS2A *myCaps2A;
    MIDIOUTCAPS2W *myCaps2W;

    CHAR synthName[] = "libADLMIDI synth\0";
    WCHAR synthNameW[] = L"libADLMIDI synth\0";

    switch(capsSize)
    {
    case(sizeof(MIDIOUTCAPSA)):
        myCapsA = (MIDIOUTCAPSA *)capsPtr;
        myCapsA->wMid = MM_UNMAPPED;
        myCapsA->wPid = MM_MPU401_MIDIOUT;
        memcpy(myCapsA->szPname, synthName, sizeof(synthName));
        myCapsA->wTechnology = MOD_MIDIPORT;
        myCapsA->vDriverVersion = 0x0090;
        myCapsA->wVoices = 0;
        myCapsA->wNotes = 0;
        myCapsA->wChannelMask = 0xffff;
        myCapsA->dwSupport = 0;
        return MMSYSERR_NOERROR;

    case(sizeof(MIDIOUTCAPSW)):
        myCapsW = (MIDIOUTCAPSW *)capsPtr;
        myCapsW->wMid = MM_UNMAPPED;
        myCapsW->wPid = MM_MPU401_MIDIOUT;
        memcpy(myCapsW->szPname, synthNameW, sizeof(synthNameW));
        myCapsW->wTechnology = MOD_MIDIPORT;
        myCapsW->vDriverVersion = 0x0090;
        myCapsW->wVoices = 0;
        myCapsW->wNotes = 0;
        myCapsW->wChannelMask = 0xffff;
        myCapsW->dwSupport = 0;
        return MMSYSERR_NOERROR;

    case(sizeof(MIDIOUTCAPS2A)):
        myCaps2A = (MIDIOUTCAPS2A *)capsPtr;
        myCaps2A->wMid = MM_UNMAPPED;
        myCaps2A->wPid = MM_MPU401_MIDIOUT;
        memcpy(myCaps2A->szPname, synthName, sizeof(synthName));
        myCaps2A->wTechnology = MOD_MIDIPORT;
        myCaps2A->vDriverVersion = 0x0090;
        myCaps2A->wVoices = 0;
        myCaps2A->wNotes = 0;
        myCaps2A->wChannelMask = 0xffff;
        myCaps2A->dwSupport = 0;
        return MMSYSERR_NOERROR;

    case(sizeof(MIDIOUTCAPS2W)):
        myCaps2W = (MIDIOUTCAPS2W *)capsPtr;
        myCaps2W->wMid = MM_UNMAPPED;
        myCaps2W->wPid = MM_MPU401_MIDIOUT;
        memcpy(myCaps2W->szPname, synthNameW, sizeof(synthNameW));
        myCaps2W->wTechnology = MOD_MIDIPORT;
        myCaps2W->vDriverVersion = 0x0090;
        myCaps2W->wVoices = 0;
        myCaps2W->wNotes = 0;
        myCaps2W->wChannelMask = 0xffff;
        myCaps2W->dwSupport = 0;
        return MMSYSERR_NOERROR;

    default:
        return MMSYSERR_ERROR;
    }
}

void DoCallback(int driverNum, DWORD_PTR clientNum, DWORD msg, DWORD_PTR param1, DWORD_PTR param2)
{
    Driver::Client *client = &drivers[driverNum].clients[clientNum];
#ifdef __MINGW32__
    if(s_DriverCallback)
        initWorkarounds();
#endif
    DriverCallback(client->callback, client->flags, drivers[driverNum].hdrvr, msg, client->instance, param1, param2);
}

LONG OpenDriver(Driver *driver, UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)uMsg;

    int clientNum;
    if(driver->clientCount == 0)
        clientNum = 0;
    else if(driver->clientCount == MAX_CLIENTS)
        return MMSYSERR_ALLOCATED;
    else
    {
        int i;
        for(i = 0; i < MAX_CLIENTS; i++)
        {
            if(!driver->clients[i].allocated)
                break;
        }
        if(i == MAX_CLIENTS)
            return MMSYSERR_ALLOCATED;
        clientNum = i;
    }

    MIDIOPENDESC *desc = (MIDIOPENDESC *)dwParam1;
    driver->clients[clientNum].allocated = true;
    driver->clients[clientNum].flags = HIWORD(dwParam2);
    driver->clients[clientNum].callback = desc->dwCallback;
    driver->clients[clientNum].instance = desc->dwInstance;
    *(LONG *)dwUser = clientNum;
    driver->clientCount++;
    DoCallback(uDeviceID, clientNum, MOM_OPEN, (DWORD_PTR)NULL, (DWORD_PTR)NULL);
    return MMSYSERR_NOERROR;
}

LONG CloseDriver(Driver *driver, UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    (void)uMsg; (void)dwParam1; (void)dwParam2;

    if(!driver->clients[dwUser].allocated)
        return MMSYSERR_INVALPARAM;
    driver->clients[dwUser].allocated = false;
    driver->clientCount--;
    DoCallback(uDeviceID, dwUser, MOM_CLOSE, (DWORD_PTR)NULL, (DWORD_PTR)NULL);
    return MMSYSERR_NOERROR;
}

EXTERN_C DWORD __declspec(dllexport) __stdcall modMessage(DWORD uDeviceID, DWORD uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    MIDIHDR *midiHdr;
    Driver *driver = &drivers[uDeviceID];
    DWORD instance;
    switch(uMsg)
    {
    case MODM_OPEN:
        if(!synthOpened)
        {
            if(midiSynth.Init() != 0) return MMSYSERR_ERROR;
            synthOpened = true;
        }
        instance = (DWORD)NULL;
        DWORD res;
        res = OpenDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
        driver->clients[*(LONG *)dwUser].synth_instance = instance;
        return res;

    case MODM_CLOSE:
        if(driver->clients[dwUser].allocated == false)
            return MMSYSERR_ERROR;
        if(synthOpened)
        {
            midiSynth.Reset();
            midiSynth.ResetSynth();
        }
        return CloseDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);

    case MODM_PREPARE:
        return MMSYSERR_NOTSUPPORTED;

    case MODM_UNPREPARE:
        return MMSYSERR_NOTSUPPORTED;

    case MODM_RESET:
        if(synthOpened)
            midiSynth.PanicSynth();
        return MMSYSERR_NOERROR;

    case MODM_GETDEVCAPS:
        return modGetCaps((PVOID)dwParam1, (DWORD)dwParam2);

    case MODM_DATA:
        if(driver->clients[dwUser].allocated == false)
            return MMSYSERR_ERROR;
        midiSynth.PushMIDI((DWORD)dwParam1);
        return MMSYSERR_NOERROR;

    case MODM_LONGDATA:
        if(driver->clients[dwUser].allocated == false)
            return MMSYSERR_ERROR;
        midiHdr = (MIDIHDR *)dwParam1;
        if((midiHdr->dwFlags & MHDR_PREPARED) == 0)
            return MIDIERR_UNPREPARED;
        midiSynth.PlaySysex((unsigned char *)midiHdr->lpData, midiHdr->dwBufferLength);
        midiHdr->dwFlags |= MHDR_DONE;
        midiHdr->dwFlags &= ~MHDR_INQUEUE;
        DoCallback(uDeviceID, dwUser, MOM_DONE, dwParam1, (DWORD_PTR)NULL);
        return MMSYSERR_NOERROR;

    case MODM_GETNUMDEVS:
        return 0x1;

    default:
        return MMSYSERR_NOERROR;
        break;
    }
}
