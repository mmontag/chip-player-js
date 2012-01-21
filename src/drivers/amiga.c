/* Amiga AHI driver for Extended Module Player
 * Copyright (C) 2007 Lorence Lombardo
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
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "driver.h"
#include "mixer.h"
#include "convert.h"

static int fd;

static int init(struct xmp_context *);
static void bufdump(struct xmp_context *, void *, int);
static void shutdown(struct xmp_context *);

static void dummy()
{
}

static char *help[] = {
	"buffer=val", "Audio buffer size",
	NULL
};

struct xmp_drv_info drv_amiga = {
	"ahi",			/* driver ID */
	"Amiga AHI audio",	/* driver description */
	help,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	dummy,			/* starttimer */
	dummy,			/* stoptimer */
	dummy,			/* resetvoices */
	bufdump,		/* bufdump */
	NULL
};

static int init(struct xmp_context *ctx)
{
	struct xmp_options *o = &ctx->o;
	char outfile[256];
	int nch = o->outfmt & XMP_FMT_MONO ? 1 : 2;
	int bsize = o->freq * nch * o->resol / 4;
	char *token, **parm;
	
	parm_init();
	chkparm1("buffer", bsize = strtoul(token, NULL, 0));
	parm_end();

	sprintf(outfile, "AUDIO:B/%d/F/%d/C/%d/BUFFER/%d",
				o->resol, o->freq, nch, bsize);

	fd = open(outfile, O_WRONLY);

	return 0;
}

static void bufdump(struct xmp_context *ctx, void *b, int i)
{
	int j;

	while (i) {
		if ((j = write(fd, b, i)) > 0) {
			i -= j;
			b = (char *)b + j;
		} else
			break;
	}
}

static void shutdown(struct xmp_context *ctx)
{
	if (fd)
		close(fd);
}
