/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: osx.c,v 1.1 2007-08-04 23:34:53 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <CoreAudio/CoreAudio.h>

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"

static int audio_fd;

static int init(struct xmp_control *);
static void bufdump(int);
static void shutdown(void);

static void dummy () { }

static char *help[] = {
	NULL
};

struct xmp_drv_info drv_bsd = {
	"osx",			/* driver ID */
	"OSX audio",		/* driver description */
	help,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	xmp_smix_numvoices,	/* numvoices */
	dummy,			/* voicepos */
	xmp_smix_echoback,	/* echoback */
	dummy,			/* setpatch */
	xmp_smix_setvol,	/* setvol */
	dummy,			/* setnote */
	xmp_smix_setpan,	/* setpan */
	dummy,			/* setbend */
	dummy,			/* seteffect */
	dummy,			/* starttimer */
	dummy,			/* stctlimer */
	dummy,			/* reset */
	bufdump,		/* bufdump */
	dummy,			/* bufwipe */
	dummy,			/* clearmem */
	dummy,			/* sync */
	xmp_smix_writepatch,	/* writepatch */
	xmp_smix_getmsg,	/* getmsg */
	NULL
};


static int init (struct xmp_control *ctl)
{
	AudioStreamBasicDescription a;
	//char *token;
	//char **parm = ctl->parm;

	parm_init();
	//chkparm1("gain", gain = atoi (token));
	//chkparm1("buffer", bsize = atoi (token));
	parm_end();

	a.mSampleRate = ctl->freq;
	a.mFormatID = kAudioFormatLinearPCM;
	a.mFormatFlags = kAudioFormatFlagIsPacked;
	if (~ctl->outfmt & XMP_FMT_UNS)
		a.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
	if (~ctl->outfmt & XMP_FMT_BIGEND)
		a.mFormatFlags |= kAudioFormatFlagIsBigEndian;
	a.mChannelsPerFrame = ctl->outfmy & XMP_FMT_MONO ? 1 : 2;
	a.mBitsPerChannel = ctl->resol;

	a.mBytesPerFrame    = 0;
	a.mBytesPerPacket   = 0;
	a.mFramesPerPacket  = 0;

	return xmp_smix_on(ctl);
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump (int i)
{
#if 0
	int j;
	void *b;

	b = xmp_smix_buffer ();
	while (i) {
		if ((j = write (audio_fd, b, i)) > 0) {
		i -= j;
		(char *)b += j;
	} else
		break;
    }
#endif
}


static void shutdown ()
{
	xmp_smix_off();
	//close(audio_fd);
}
