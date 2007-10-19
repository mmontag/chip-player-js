/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: file.c,v 1.9 2007-10-19 20:55:00 cmatsuoka Exp $
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

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"
#include "convert.h"

static int fd;
static int endian;

static int init (struct xmp_context *, struct xmp_control *);
static void bufdump (struct xmp_context *, int);
static void shutdown ();

static void dummy () { }

static char *help[] = {
    "big-endian", "Generate big-endian 16-bit samples",
    "little-endian", "Generate little-endian 16-bit samples",
    NULL
};

struct xmp_drv_info drv_file = {
    "file",		/* driver ID */
    "file",		/* driver description */
    help,		/* help */
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
    struct xmp_options *o = &ctx->o;
    char *buf;
    int bsize;
    char *token, **parm;

    endian = 0;
    parm_init ();
    chkparm1 ("big-endian", endian = 1);
    chkparm1 ("little-endian", endian = -1);
    parm_end ();

    if (!o->outfile)
	o->outfile = "xmp.out";

    fd = strcmp (o->outfile, "-") ? creat (o->outfile, 0644) : 1;

    bsize = strlen(drv_file.description) + strlen (o->outfile) + 8;
    buf = malloc(bsize);
    if (strcmp (o->outfile, "-")) {
	snprintf(buf, bsize, "%s: %s", drv_file.description, o->outfile);
	drv_file.description = buf;
    } else {
	drv_file.description = "Output to stdout";
    }

    return xmp_smix_on(ctl);
}


static void bufdump(struct xmp_context *ctx, int i)
{
    int j;
    void *b;

    /* Doesn't work if EINTR -- reported by Ruda Moura <ruda@helllabs.org> */
    /* for (; i -= write (fd, xmp_smix_buffer (), i); ); */

    b = xmp_smix_buffer(ctx);
    if ((ctx->big_endian && endian == -1) || (!ctx->big_endian && endian == 1))
	xmp_cvt_sex(i, b);

    while (i) {
	if ((j = write (fd, b, i)) > 0) {
	    i -= j;
	    b = (char *)b + j;
	} else
	    break;
    }
}


static void shutdown()
{
    xmp_smix_off();

    if (fd)
	close(fd);
}
