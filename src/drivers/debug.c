/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: debug.c,v 1.5 2007-10-20 13:35:08 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include "xmpi.h"
#include "driver.h"

static int numvoices	(int);
static void voicepos	(int, int);
static void echoback	(int);
static void setpatch	(int, int);
static void setvol	(int, int);
static void setnote	(int, int);
static void setpan	(int, int);
static void setbend	(int, int);
static void seteffect	(int, int, int);
static void starttimer	(void);
static void stoptimer	(void);
static void resetvoices	(void);
static void bufdump	(void);
static void bufwipe	(void);
static void clearmem	(void);
static void seq_sync	(double);
static int writepatch	(struct patch_info *);
static int init		(struct xmp_context *ctx);
static int getmsg	(void);
static void shutdown	(void);


struct xmp_drv_info drv_debug = {
    "debug",		/* driver ID */
    "Driver debugger",	/* driver description */
    NULL,		/* help */
    init,		/* init */
    shutdown,		/* shutdown */
    numvoices,		/* numvoices */
    voicepos,		/* voicepos */
    echoback,		/* echoback */
    setpatch,		/* setpatch */
    setvol,		/* setvol */
    setnote,		/* setnote */
    setpan,		/* setpan */
    setbend,		/* setbend */
    seteffect,		/* seteffect */
    starttimer,		/* settimer */
    stoptimer,		/* stoptimer */
    resetvoices,	/* resetvoices */
    bufdump,		/* bufdump */
    bufwipe,		/* bufwipe */
    clearmem,		/* clearmem */
    seq_sync,		/* sync */
    writepatch,		/* writepatch */
    getmsg,		/* getmsg */
    NULL
};


static int numvoices (int num)
{
    return 32;
}

static void voicepos (int ch, int pos)
{
}

static void echoback (int msg)
{
}

static void setpatch (int ch, int smp)
{
    printf ("[%02d] setpatch: %d\n", ch, smp);
}

static void setvol (int ch, int vol)
{
    printf ("[%02d] setvol: %d\n", ch, vol);
}

static void setnote (int ch, int note)
{
    printf ("[%02d] setvol: %d\n", ch, note);
}


static void seteffect (int ch, int type, int val)
{
    printf ("[%02d] seteffect: %d, %d\n", ch, type, val);
}


static void setpan (int ch, int pan)
{
    printf ("[%02d] setpan: %d\n", ch, pan);
}

static void setbend (int ch, int bend)
{
    printf ("[%02d] setbend: %d\n", ch, bend);
}

static void starttimer ()
{
   printf ("** starttimer\n");
}

static void stoptimer ()
{
   printf ("** stoptimer\n");
}

static void resetvoices ()
{
   printf ("** resetvoices\n");
}

static void bufwipe ()
{
   printf ("** bufwipe\n");
}

static void bufdump ()
{
}

static void clearmem ()
{
   printf ("** clearmem\n");
}

static void seq_sync (double next_time)
{
}

static int writepatch (struct patch_info *patch)
{
    if (!patch) {
	clearmem ();
	return XMP_OK;
    }

    printf ("** writepatch: len = %d\n", patch->len);

    return XMP_OK;
}

static int getmsg ()
{
    return XMP_OK;
}

static int init(struct xmp_context *ctx)
{
    return XMP_OK;
}


static void shutdown ()
{
    printf ("** shutdown\n");
}
