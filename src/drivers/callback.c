/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: callback.c,v 1.7 2007-10-21 03:56:17 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"


static int init (struct xmp_context *);
static void shutdown (void);
static void callback (struct xmp_context *ctx, int);

static void dummy () { }

struct xmp_drv_info drv_callback = {
    "callback",		/* driver ID */
    "callback driver",	/* driver description */
    NULL,		/* help */
    init,		/* init */
    shutdown,		/* shutdown */
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
    callback,		/* bufdump */
    dummy,		/* bufwipe */
    dummy,		/* clearmem */
    dummy,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};

static int init(struct xmp_context *ctx)
{
    return ctx->d.callback ? xmp_smix_on(ctx) : -1;
}


static void callback(struct xmp_context *ctx, int i)
{
    void *b;

    b = xmp_smix_buffer(ctx);
    ctx->d.callback(b, i);
}


static void shutdown()
{
    xmp_smix_off();
}

