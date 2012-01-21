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

#include <artsc.h>
#include "common.h"
#include "driver.h"
#include "mixer.h"

static arts_stream_t as;

static int init(struct xmp_context *);
static void bufdump(struct xmp_context *, void *, int);
static void myshutdown(struct xmp_context *);

static void dummy()
{
}

struct xmp_drv_info drv_arts = {
	"arts",			/* driver ID */
	"aRts client driver",	/* driver description */
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
	int rc, rate, bits, channels;

	rate = o->freq;
	bits = o->resol;
	channels = 2;

	if ((rc = arts_init()) < 0) {
		fprintf(stderr, "%s\n", arts_error_text(rc));
		return XMP_ERR_DINIT;
	}

	if (o->outfmt & XMP_FMT_MONO)
		channels = 1;

	as = arts_play_stream(rate, bits, channels, "xmp");

	return 0;
}

static void bufdump(struct xmp_context *ctx, void *b, int i)
{
	int j;

	do {
		if ((j = arts_write(as, b, i)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	} while (i);
}

static void myshutdown(struct xmp_context *ctx)
{
	arts_close_stream(as);
	arts_free();
}
