/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: beos.c,v 1.3 2007-10-06 14:08:13 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Application.h>
#include <SoundPlayer.h>

extern "C" {
#include "xmpi.h"
#include "driver.h"
#include "mixer.h"
}

static int init(struct xmp_control *);
static void bufdump(int);
static void myshutdown();

static void dummy()
{
}

static char *help[] = {
	"dev=<device_name>", "Audio device name (default is /dev/dsp)",
	"buffer=val", "Audio buffer size (default is 32768)",
	NULL
};

struct xmp_drv_info drv_arts = {
	"beos",				/* driver ID */
	"BeOS PCM audio",		/* driver description */
	NULL,				/* help */
	(int (*)())init,		/* init */
	(void (*)())myshutdown,		/* shutdown */
	(void (*)())xmp_smix_numvoices,	/* numvoices */
	dummy,				/* voicepos */
	(void (*)())xmp_smix_echoback,	/* echoback */
	dummy,				/* setpatch */
	(void (*)())xmp_smix_setvol,	/* setvol */
	dummy,				/* setnote */
	(void (*)())xmp_smix_setpan,	/* setpan */
	dummy,				/* setbend */
	(void (*)())xmp_smix_seteffect,	/* seteffect */
	dummy,				/* starttimer */
	dummy,				/* stctlimer */
	dummy,				/* resetvoices */
	(void (*)())bufdump,		/* bufdump */
	dummy,				/* bufwipe */
	dummy,				/* clearmem */
	dummy,				/* sync */
	(int (*)())xmp_smix_writepatch,	/* writepatch */
	(int (*)())xmp_smix_getmsg,	/* getmsg */
	NULL
};

static media_raw_audio_format fmt;
static BSoundPlayer player;


/*
 * CoreAudio helpers from mplayer/libao
 * The player fills a ring buffer, BSP retrieves data from the buffer
 */

static int paused;
static uint8 *buffer;
static int buffer_len;
static int buf_write_pos;
static int buf_read_pos;
static int chunk_size;
static int packet_size;


/* return minimum number of free bytes in buffer, value may change between
 * two immediately following calls, and the real number of free bytes
 * might actually be larger!  */
static int buf_free()
{
	int free = buf_read_pos - buf_write_pos;
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

	/* till end of buffer */
	memcpy(buffer + buf_write_pos, data, first_len);
	if (len > first_len) {	/* we have to wrap around */
		/* remaining part from beginning of buffer */
		memcpy(buffer, data + first_len, len - first_len);
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

	/* till end of buffer */
	memcpy(data, buffer + buf_read_pos, first_len);
	if (len > first_len) {	/* we have to wrap around */
		/* remaining part from beginning of buffer */
		memcpy(data + first_len, buffer, len - first_len);
	}
	buf_read_pos = (buf_read_pos + len) % buffer_len;

	return len;
}

/*
 * end of CoreAudio helpers
 */


void render_proc(void *theCookie, void *buffer, size_t req, 
				const media_raw_audio_format &format)
{ 
        int amt = buf_used();

        read_buffer(buffer, amt > req ? req : amt);
}


static int init(struct xmp_control *ctl)
{
	int bsize;
	char *dev;
	char *token;
	char **parm = ctl->parm;

	be_app = new BApplication("application/x-vnd.101-xmp");
	be_app->Run();

	bsize = 4096;

	parm_init();
	chkparm1("buffer", chunk_size = strtoul(token, NULL, 0));
	parm_end();

	fmt.frame_rate = ctl->freq;
	fmt.channel_count = ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
	fmt.format = ctl->resol > 8 ? B_AUDIO_SHORT : B_AUDIO_CHAR;
	fmt.byte_order = B_HOST_IS_LENDIAN ?
				B_MEDIA_LITTLE_ENDIAN : B_MEDIA_BIG_ENDIAN;
	format.buffer_size = bsize;

	buffer_len = bsize;
	buffer = calloc(1, bsize);
	buf_read_pos = 0;
	buf_write_pos = 0;
	paused = 1;
	
	player = new BSoundPlayer(&fmt, "xmp output", render_proc);

	return xmp_smix_on(ctl);
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump(int i)
{
	uint8 *b;
	int j = 0;

	/* block until we have enough free space in the buffer */
	while (buf_free() < i)
		usleep(100000);

	b = xmp_smix_buffer();

	while (i) {
        	if ((j = write_buffer(b, i)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	}

	if (paused) {
		player.Start(); 
		player.SetHasData(true);
		paused = 0;
	}
}

static void myshutdown()
{
	xmp_smix_off();
	be_app->Lock();
	be_app->Quit();
}


