// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <mmsystem.h>
#ifndef __MINGW32__
#include <mmddk.h>
#endif
#include <mmreg.h>
#include <process.h>
#include <iostream>
#include <adlmidi.h>
#include "MidiSynth.h"

#ifdef __MINGW32__
// Define missing in MinGW macros here

#ifndef EXTERN_C
#ifdef __cplusplus
    #define EXTERN_C       extern "C"
    #define EXTERN_C_START extern "C" {
    #define EXTERN_C_END   }
#else
    #define EXTERN_C       extern
    #define EXTERN_C_START
    #define EXTERN_C_END
#endif
#endif

#ifndef STDAPICALLTYPE
#define STDAPICALLTYPE  __stdcall
#endif

#ifndef STDAPI_
#define STDAPI_(type)   EXTERN_C type STDAPICALLTYPE
#endif

#define DRV_PNPINSTALL  (DRV_RESERVED + 11)

#ifndef MMNOMIDI

typedef struct midiopenstrmid_tag
{
    DWORD dwStreamID;
    UINT uDeviceID;
} MIDIOPENSTRMID;

typedef struct midiopendesc_tag {
    HMIDI hMidi;
    DWORD_PTR dwCallback;
    DWORD_PTR dwInstance;
    DWORD_PTR dnDevNode;
    DWORD cIds;
    MIDIOPENSTRMID rgIds[1];
} MIDIOPENDESC;

typedef MIDIOPENDESC FAR *LPMIDIOPENDESC;

#endif // MMNOMIDI

#define MODM_GETNUMDEVS     1
#define MODM_GETDEVCAPS     2
#define MODM_OPEN           3
#define MODM_CLOSE          4
#define MODM_PREPARE        5
#define MODM_UNPREPARE      6
#define MODM_DATA           7
#define MODM_LONGDATA       8
#define MODM_RESET          9
#define MODM_GETVOLUME      10
#define MODM_SETVOLUME      11
#define MODM_CACHEPATCHES       12
#define MODM_CACHEDRUMPATCHES   13

#endif // __MINGW32__
