/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "common.h"
#include "driver.h"
#include "convert.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define DATA(x) (((struct data *)drv_wav.data)->x)

struct data {
	int fd;
	uint32 size;
};

static int init(struct xmp_context *);
static void bufdump(struct xmp_context *, void *, int);
static void shutdown(struct xmp_context *);

static void dummy()
{
}

struct xmp_drv_info drv_wav = {
	"wav",			/* driver ID */
	"WAV writer",		/* driver description */
	NULL,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	dummy,			/* starttimer */
	dummy,			/* stoptimer */
	bufdump,		/* bufdump */
};

static void writeval_16l(int fd, uint16 v)
{
	uint8 x;

	x = v & 0xff;
	write(DATA(fd), &x, 1);

	x = v >> 8;
	write(DATA(fd), &x, 1);
}

static void writeval_32l(int fd, uint32 v)
{
	uint8 x;

	x = v & 0xff;
	write(DATA(fd), &x, 1);

	x = (v >> 8) & 0xff;
	write(DATA(fd), &x, 1);

	x = (v >> 16) & 0xff;
	write(DATA(fd), &x, 1);

	x = (v >> 24) & 0xff;
	write(DATA(fd), &x, 1);
}

static int init(struct xmp_context *ctx)
{
	char *buf;
	uint32 len = 0;
	uint16 chan;
	uint32 sampling_rate, bytes_per_second;
	uint16 bytes_per_frame, bits_per_sample;
	char *f, filename[PATH_MAX];
	struct xmp_options *o = &ctx->o;

	drv_wav.data = malloc(sizeof (struct data));
	if (drv_wav.data == NULL)
		return -1;

	if (!o->outfile) {
		if (global_filename) {
			if ((f = strrchr(global_filename, '/')) != NULL)
				strncpy(filename, f + 1, PATH_MAX);
			else
				strncpy(filename, global_filename, PATH_MAX);
		} else {
			strcpy(filename, "xmp");
		}

		strncat(filename, ".wav", PATH_MAX);

		o->outfile = filename;
	}

	if (strcmp(o->outfile, "-")) {
		DATA(fd) = open(o->outfile, O_WRONLY | O_CREAT | O_TRUNC
							| O_BINARY, 0644);
		if (DATA(fd) < 0)
			return -1;
	} else {
		DATA(fd) = 1;
	}

	if (strcmp(o->outfile, "-")) {
		int len = strlen(drv_wav.description) + strlen(o->outfile) + 8;
		if ((buf = malloc(len)) == NULL)
			return -1;
		snprintf(buf, len, "%s: %s", drv_wav.description, o->outfile);
		drv_wav.description = buf;
	} else {
		drv_wav.description = strdup("WAV writer: stdout");
		len = -1;
	}

	write(DATA(fd), "RIFF", 4);
	writeval_32l(DATA(fd), len);
	write(DATA(fd), "WAVE", 4);

	chan = o->outfmt & XMP_FMT_MONO ? 1 : 2;
	sampling_rate = o->freq;
	bits_per_sample = o->resol;
	if (bits_per_sample == 8)
		o->outfmt |= XMP_FMT_UNS;
	bytes_per_frame = chan * bits_per_sample / 8;
	bytes_per_second = sampling_rate * bytes_per_frame;

	write(DATA(fd), "fmt ", 4);
	writeval_32l(DATA(fd), 16);
	writeval_16l(DATA(fd), 1);
	writeval_16l(DATA(fd), chan);
	writeval_32l(DATA(fd), sampling_rate);
	writeval_32l(DATA(fd), bytes_per_second);
	writeval_16l(DATA(fd), bytes_per_frame);
	writeval_16l(DATA(fd), bits_per_sample);

	write(DATA(fd), "data", 4);
	writeval_32l(DATA(fd), len);

	DATA(size) = 0;

	return 0;
}

static void bufdump(struct xmp_context *ctx, void *b, int i)
{
	struct xmp_options *o = &ctx->o;

	if (o->big_endian)
		xmp_cvt_sex(i, b);
	write(DATA(fd), b, i);
	DATA(size) += i;
}

static void shutdown(struct xmp_context *ctx)
{
	lseek(DATA(fd), 40, SEEK_SET);
	writeval_32l(DATA(fd), DATA(size));

	lseek(DATA(fd), 4, SEEK_SET);
	writeval_32l(DATA(fd), DATA(size) + 40);

	if (DATA(fd) > 0)
		close(DATA(fd));

	free(drv_wav.description);
	free(drv_wav.data);
}
