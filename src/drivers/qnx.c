/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Based on the QNX4 port of nspmod by Mike Gorchak <malva@selena.kherson.ua>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/audio.h>
#include <sys/ioctl.h>
#include "common.h"
#include "driver.h"
#include "mixer.h"

static int fd_audio;

static int init(struct xmp_context *);
static void bufdump(struct xmp_context *, void *, int);
static void myshutdown(struct xmp_context *);
static void mysync();

static void dummy()
{
}

static char *help[] = {
	"dev=<device_name>", "Audio device name (default is /dev/dsp)",
	"buffer=val", "Audio buffer size (default is 32768)",
	NULL
};

struct xmp_drv_info drv_qnx = {
	"QNX",			/* driver ID */
	"QNX PCM audio",	/* driver description */
	NULL,			/* help */
	init,			/* init */
	myshutdown,		/* shutdown */
	dummy,			/* starttimer */
	dummy,			/* flush */
	bufdump,		/* bufdump */
};

static int init(struct xmp_context *ctx)
{
	struct xmp_options *o = &ctx->o;
	int rc, rate, bits, stereo, bsize;
	char *dev;
	char *token, **parm;

	parm_init();
	chkparm1("dev", dev = token);
	chkparm1("buffer", bsize = strtoul(token, NULL, 0));
	parm_end();

	rate = o->freq;
	bits = o->resol;
	stereo = 1;
	bufsize = 32 * 1024;

	fd_audio = open(dev, O_WRONLY);
	if (fd_audio < 0) {
		fprintf(stderr, "can't open audio device\n");
		return XMP_ERR_DINIT;
	}

	if (o->outfmt & XMP_FMT_MONO)
		stereo = 0;

	if (ioctl(fd_audio, SOUND_PCM_WRITE_BITS, &bits) < 0) {
		perror("can't set resolution");
		goto error;
	}

	if (ioctl(fd, SNDCTL_DSP_STEREO, &stereo) < 0) {
		perror("can't set channels");
		goto error;
	}

	if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) < 0) {
		perror("can't set rate");
		goto error;
	}

	if (ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &buf.size) < 0) {
		perror("can't set rate");
		goto error;
	}

	if (ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &bufsize) < 0) {
		perror("can't set buffer");
		goto error;
	}

	return 0;

      error:
	close(fd_audio);
	return XMP_ERR_DINIT;
}

static void bufdump(struct xmp_context *ctx, void *b, int i)
{
	int j;

	do {
		if ((j = write(fd_audio, b, i)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	} while (i);
}

static void myshutdown(struct xmp_context *ctx)
{
	close(fd_audio);
}

static void mysync()
{
	ioctl(fd, SNDCTL_DSP_SYNC, NULL);
}
