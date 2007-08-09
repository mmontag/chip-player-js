/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 * CoreAudio helpers (C) 2000 Timothy J. Wood
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: osx.c,v 1.4 2007-08-09 20:45:57 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"


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

struct xmp_drv_info drv_osx = {
	"osx",			/* driver ID */
	"OSX CoreAudio",	/* driver description */
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


/*
 * CoreAudio helpers from mplayer/libao
 */

static AudioUnit au;
static int paused;
static uint8 *buffer;
static int buffer_len;
static int buf_write_pos;
static int buf_read_pos;
static int num_chunks;
static int chunk_size;
static int packet_size;

/* stop playing, keep buffers (for pause) */
static void audio_pause()
{
	/* stop callback */
	AudioOutputUnitStop(au);
        paused = 1;
}

/* resume playing, after audio_pause() */
static void audio_resume()
{
        if (paused) {
                /* start callback */
                AudioOutputUnitStart(au);
                paused = 0;
        }
}

/* set variables and buffer to initial state */
static void reset()
{
        audio_pause();
        /* reset ring-buffer state */
        buf_read_pos = 0;
        buf_write_pos = 0;
}

/* return minimum number of free bytes in buffer, value may change between
 * two immediately following calls, and the real number of free bytes
 * might actually be larger!  */
static int buf_free()
{
	int free = buf_read_pos - buf_write_pos - chunk_size;
	if (free < 0)
		free += buffer_len;
	return free;
}

/* return minimum number of buffered bytes, value may change between
 * two immediately following calls, and the real number of buffered bytes
 * might actually be larger! */
static int buf_used()
{
	int used = buf_write_pos - buf_read_pos;
	if (used < 0)
		used += buffer_len;
	return used;
}

/* add data to ringbuffer */
static int write_buffer(unsigned char *data, int len)
{
	int first_len = buffer_len - buf_write_pos;
	int free = buf_free();
	if (len > free)
		len = free;
	if (first_len > len)
		first_len = len;
	// till end of buffer
	memcpy(&buffer[buf_write_pos], data, first_len);
	if (len > first_len) {	// we have to wrap around
		// remaining part from beginning of buffer
		memcpy(buffer, &data[first_len], len - first_len);
	}
	buf_write_pos = (buf_write_pos + len) % buffer_len;

	return len;
}

/* remove data from ringbuffer */
static int read_buffer(unsigned char *data, int len)
{
	int first_len = buffer_len - buf_read_pos;
	int buffered = buf_used();
	if (len > buffered)
		len = buffered;
	if (first_len > len)
		first_len = len;
	// till end of buffer
	memcpy(data, &buffer[buf_read_pos], first_len);
	if (len > first_len) {	// we have to wrap around
		// remaining part from beginning of buffer
		memcpy(&data[first_len], buffer, len - first_len);
	}
	buf_read_pos = (buf_read_pos + len) % buffer_len;

	return len;
}

OSStatus render_proc(void *inRefCon,
		AudioUnitRenderActionFlags *inActionFlags,
		const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
		UInt32 inNumFrames, AudioBufferList *ioData)
{
	int amt = buf_used();
	int req = (inNumFrames) * packet_size;

	if (amt > req)
		amt = req;

	if (amt)
		read_buffer((unsigned char *)ioData->mBuffers[0].mData, amt);
	else
		audio_pause();

	ioData->mBuffers[0].mDataByteSize = amt;

        return noErr;
}

/*
 * end of CoreAudio helpers
 */


static int init (struct xmp_control *ctl)
{
	AudioStreamBasicDescription ad;
	Component comp;
	ComponentDescription cd;
	AURenderCallbackStruct rc;

	char *token;
	char **parm = ctl->parm;
	int bpf, bpp, fpp;

	bpf = bpp = 0;
	fpp = 1;

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
	ad.mChannelsPerFrame = ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
	ad.mBitsPerChannel = ctl->resol;

	ad.mBytesPerFrame = bpf;
	ad.mBytesPerPacket = bpp;
	ad.mFramesPerPacket = fpp;

        packet_size = ad.mFramesPerPacket * ad.mChannelsPerFrame *
						(ad.mBitsPerChannel / 8);

	cd.componentType = kAudioUnitType_Output;
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;

	if ((comp = FindNextComponent(NULL, &cd)) == NULL)
		return XMP_ERR_DINIT;

	if (OpenAComponent(comp, &au))
		return XMP_ERR_DINIT;

	if (AudioUnitInitialize(au))
		return XMP_ERR_DINIT;

	if (AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Input, 0, &ad, sizeof(ad)))
		return XMP_ERR_DINIT;

        num_chunks = (ctl->freq * bpf + chunk_size - 1) / chunk_size;
        buffer_len = (num_chunks + 1) * chunk_size;
        buffer = calloc(num_chunks + 1, chunk_size);

	rc.inputProc = render_proc;
	rc.inputProcRefCon = 0;

	if (AudioUnitSetProperty(au, kAudioUnitProperty_SetRenderCallback,
			kAudioUnitScope_Input, 0, &rc, sizeof(rc)))
		return XMP_ERR_DINIT;
	
	reset();

	return xmp_smix_on(ctl);
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump(int i)
{
	uint8 *b;
	int j = 0;

	b = xmp_smix_buffer();
	while (i) {
        	if ((j = write_buffer(b, i)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	}

        audio_resume();
}


static void shutdown ()
{
	xmp_smix_off();
        AudioOutputUnitStop(au);
	AudioUnitUninitialize(au);
	CloseComponent(au);
	free(buffer);
}
