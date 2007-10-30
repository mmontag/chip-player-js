/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: win32.c,v 1.9 2007-10-30 11:57:51 cmatsuoka Exp $
 */

/*
 * Based on Tony Million's Win32 driver for mpg123
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <windows.h>

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"

static CRITICAL_SECTION cs;
static HWAVEOUT dev = NULL;
static int nBlocks = 0;
static int MAX_BLOCKS  = 6;

static int init (struct xmp_context *);
static void bufdump (struct xmp_context *, int);
static void deinit();

static void dummy() { }

struct xmp_drv_info drv_win32 = {
    "win32",		/* driver ID */
    "Win32 driver",	/* driver description */
    NULL,		/* help */
    init,		/* init */
    deinit,		/* shutdown */
    xmp_smix_numvoices,	/* numvoices */
    dummy,		/* voicepos */
    xmp_smix_echoback,	/* echoback */
    dummy,		/* setpatch */
    xmp_smix_setvol,	/* setvol */
    dummy,		/* setnote */
    xmp_smix_setpan,	/* setpan */
    dummy,		/* setbend */
    xmp_smix_seteffect,	/* seteffect */
    dummy,		/* starttimer */
    dummy,		/* flush */
    dummy,		/* resetvoices */
    bufdump,		/* bufdump */
    dummy,		/* bufwipe */
    dummy,		/* clearmem */
    dummy,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};


static void CALLBACK wave_callback (HWAVE hWave, UINT uMsg, DWORD dwInstance,
	DWORD dwParam1, DWORD dwParam2)
{
    WAVEHDR *wh;
    HGLOBAL hg;

    if (uMsg == WOM_DONE) {
	EnterCriticalSection (&cs);
	wh = (WAVEHDR *)dwParam1;
	waveOutUnprepareHeader (dev, wh, sizeof (WAVEHDR));

	/* Deallocate the buffer memory */
	hg = GlobalHandle (wh->lpData);
	GlobalUnlock (hg);
	GlobalFree (hg);

	/* Deallocate the header memory */
	hg = GlobalHandle (wh);
	GlobalUnlock (hg);
	GlobalFree (hg);

	/* decrease the number of USED blocks */
	nBlocks--;

	LeaveCriticalSection( &cs );
    }
}

static int init(struct xmp_context *ctx)
{
    struct xmp_options *o = &ctx->o;
    MMRESULT res;
    WAVEFORMATEX outFormatex;

    if (!waveOutGetNumDevs()) {
        MessageBox(NULL, "No audio devices present!", "Error...", MB_OK);
	return XMP_ERR_DINIT;
    }

    outFormatex.wFormatTag = WAVE_FORMAT_PCM;
    outFormatex.wBitsPerSample = o->resol;
    outFormatex.nChannels = o->flags & XMP_FMT_MONO ? 1 : 2;
    outFormatex.nSamplesPerSec = o->freq;
    outFormatex.nAvgBytesPerSec = outFormatex.nSamplesPerSec *
	outFormatex.nChannels * outFormatex.wBitsPerSample / 8;
    outFormatex.nBlockAlign = outFormatex.nChannels *
	outFormatex.wBitsPerSample / 8;

    res = waveOutOpen (&dev, WAVE_MAPPER, &outFormatex,
	(DWORD)wave_callback, 0, CALLBACK_FUNCTION);

    if (res != MMSYSERR_NOERROR) {
	char *msg;

	switch(res) {
	case MMSYSERR_ALLOCATED:
	    msg = "Device is already open";
	    break;
	case MMSYSERR_BADDEVICEID:
	    msg = "Device is out of range";
	    break;
	case MMSYSERR_NODRIVER:
	    msg = "No audio driver in this system";
	    break;
	case MMSYSERR_NOMEM:
	    msg = "Unable to allocate sound memory";
	    break;
	case WAVERR_BADFORMAT:
	    msg = "Audio format not supported";
	    break;
	case WAVERR_SYNC:
	    msg = "The device is synchronous";
	    break;
	default:
	    msg = "Unknown media error";
	    break;
	}
	MessageBox(NULL, msg, "Error...", MB_OK);

	return XMP_ERR_DINIT;
   }

    waveOutReset(dev);
    InitializeCriticalSection(&cs);

    return xmp_smix_on(ctx);
}


static void bufdump(struct xmp_context *ctx, int len)
{
    HGLOBAL hg, hg2;
    LPWAVEHDR wh;
    MMRESULT res;
    void *buf, *b;

    buf = xmp_smix_buffer(ctx);

    /* Wait for a few FREE blocks... */
    while(nBlocks > MAX_BLOCKS)
	Sleep (77);

    /* FIRST allocate some memory for a copy of the buffer! */
    hg2 = GlobalAlloc (GMEM_MOVEABLE, len);
    if(!hg2) {
	MessageBox (NULL, "GlobalAlloc failed!", "Error...",  MB_OK);
	abort();
    }
    b = GlobalLock(hg2);

    /* Here we can call any modification output functions we want */
    CopyMemory (b, buf, len);

    /* now make a header and WRITE IT! */
    hg = GlobalAlloc (GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof (WAVEHDR));
    if(!hg) {
	MessageBox (NULL, "GlobalAlloc failed!", "Error...",  MB_OK);
	abort();
    }
    wh = GlobalLock(hg);
    wh->dwBufferLength = len;
    wh->lpData = b;

    EnterCriticalSection (&cs);

    res = waveOutPrepareHeader (dev, wh, sizeof (WAVEHDR));
    if(res) {
	GlobalUnlock (hg);
	GlobalFree (hg);
	LeaveCriticalSection (&cs);
	return;
    }

    res = waveOutWrite (dev, wh, sizeof (WAVEHDR));
    if (res) {
	GlobalUnlock(hg);
	GlobalFree (hg);
	LeaveCriticalSection (&cs);
	return;
    }

    nBlocks++;

    LeaveCriticalSection (&cs);
}


static void deinit()
{
    xmp_smix_off();

    if(dev) {
	while (nBlocks)
	    Sleep (77);
	waveOutReset(dev);
	waveOutClose(dev);
	dev=NULL;
    }

    DeleteCriticalSection(&cs);
    nBlocks = 0;
}
