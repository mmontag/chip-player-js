/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: arts.c,v 1.4 2007-10-19 12:48:59 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <artsc.h>
#include "xmpi.h"
#include "driver.h"
#include "mixer.h"

static arts_stream_t as;

static int init (struct xmp_context *, struct xmp_control *);
static void bufdump (int, struct xmp_context *);
static void myshutdown ();

static void dummy () { }

struct xmp_drv_info drv_arts = {
    "arts",		/* driver ID */
    "aRts client driver",	/* driver description */
    NULL,		/* help */
    init,		/* init */
    myshutdown,		/* shutdown */
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
    dummy,		/* stctlimer */
    dummy,		/* resetvoices */
    bufdump,		/* bufdump */
    dummy,		/* bufwipe */
    dummy,		/* clearmem */
    dummy,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};

static int init(struct xmp_context *ctx, struct xmp_control *ctl)
{
    int rc, rate, bits, channels;

    rate = ctl->freq;
    bits = ctl->resol;
    channels = 2;

    if ((rc = arts_init()) < 0) {
	fprintf (stderr, "%s\n", arts_error_text (rc));
	return XMP_ERR_DINIT;
    }

    if (ctl->outfmt & XMP_FMT_MONO)
	channels = 1;

    as = arts_play_stream(rate, bits, channels, "xmp");

    return xmp_smix_on(ctl);
}


static void bufdump(int i, struct xmp_context *ctx)
{
    int j;
    void *b;

    b = xmp_smix_buffer(ctx);
    do {
	if ((j = arts_write(as, b, i)) > 0) {
	    i -= j;
	    b += j;
	} else
	    break;
    } while (i);
}


static void myshutdown()
{
    xmp_smix_off();
    arts_close_stream(as);
    arts_free();
}
