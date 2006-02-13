/* Extended Module Player
 * Copyright (C) 1996-2004 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: file.c,v 1.3 2006-02-13 16:48:20 cmatsuoka Exp $
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

static int audio_fd;

static int init (struct xmp_control *);
static void bufdump (int);
static void shutdown ();

static void dummy () { }

struct xmp_drv_info drv_file = {
    "file",		/* driver ID */
    "file",		/* driver description */
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
    bufdump,		/* bufdump */
    dummy,		/* bufwipe */
    dummy,		/* clearmem */
    dummy,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};

static int init (struct xmp_control *ctl)
{
    char *buf;
    int bsize;

    if (!ctl->outfile)
	ctl->outfile = "xmp.out";

    audio_fd = strcmp (ctl->outfile, "-") ? creat (ctl->outfile, 0644) : 1;

    bsize = strlen(drv_file.description) + strlen (ctl->outfile) + 8;
    buf = malloc(bsize);
    if (strcmp (ctl->outfile, "-")) {
	snprintf(buf, bsize, "%s: %s", drv_file.description, ctl->outfile);
	drv_file.description = buf;
    } else {
	drv_file.description = "Output to stdout";
    }

    return xmp_smix_on (ctl);
}


static void bufdump (int i)
{
    int j;
    void *b;

    /* Doesn't work if EINTR -- reported by Ruda Moura <ruda@helllabs.org> */
    /* for (; i -= write (audio_fd, xmp_smix_buffer (), i); ); */

    b = xmp_smix_buffer ();
    while (i) {
	if ((j = write (audio_fd, b, i)) > 0) {
	    i -= j;
	    b = (char *)b + j;
	} else
	    break;
    }
}


static void shutdown ()
{
    xmp_smix_off ();

    if (audio_fd)
	close (audio_fd);
}
