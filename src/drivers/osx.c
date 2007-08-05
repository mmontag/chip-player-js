/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: osx.c,v 1.2 2007-08-05 03:02:01 cmatsuoka Exp $
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
	"bpf", "Bytes per frame",
	"bpp", "Bytes per packet",
	"fpp", "Frames per packet",
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
	AudioStreamBasicDescription ad;
	Component comp;
	ComponentDescription cd;
	ComponentInstance ci;
	AURenderCallbackStruct renderCallback;
	OSStatus err;

	char *token;
	char **parm = ctl->parm;
	int bpf, bpp, fpp;

	bpf = bpp = fpp = 0;

	parm_init();
	chkparm1("bpf", bpf = atoi(token));
	chkparm1("bpp", bpp = atoi(token));
	chkparm1("fpp", fpp = atoi(token));
	parm_end();

	ad.mSampleRate = ctl->freq;
	ad.mFormatID = kAudioFormatLinearPCM;
	ad.mFormatFlags = kAudioFormatFlagIsPacked;
	if (~ctl->outfmt & XMP_FMT_UNS)
		ad.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
	if (~ctl->outfmt & XMP_FMT_BIGEND)
		ad.mFormatFlags |= kAudioFormatFlagIsBigEndian;
	ad.mChannelsPerFrame = ctl->outfmy & XMP_FMT_MONO ? 1 : 2;
	ad.mBitsPerChannel = ctl->resol;

	ad.mBytesPerFrame = bpf;
	ad.mBytesPerPacket = bpp;
	ad.mFramesPerPacket = fpp;

	cd.componentType = kAudioUnitType_Output;
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;

	if ((comp = FindNextComponent(NULL, &cd)) == NULL)
		return XMP_ERR_DINIT;

	if (OpenAComponent(comp, &ci))
		return XMP_ERR_DINIT;

	if (AudioUnitInitialize(ci))
		return XMP_ERR_DINIT;

	if (AudioUnitSetProperty(ci, kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Input, 0, &cd, sizeof(cd));
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
