/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: beos.c,v 1.1 2007-10-06 11:12:39 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Application.h>
#include <SoundPlayer.h>
#include "xmpi.h"
#include "driver.h"
#include "mixer.h"

static sem_id startsem, endsem;
static bool need_locking;

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

static int init(struct xmp_control *ctl)
{
	int rc, rate, bits, stereo, bsize;
	char *dev;
	char *token;
	char **parm = ctl->parm;

	parm_init();
	chkparm1("dev", dev = token);
	chkparm1("buffer", bsize = atoi(token));
	parm_end();

	rate = ctl->freq;
	bits = ctl->resol;
	stereo = 1;
	bufsize = 32 * 1024;

	return xmp_smix_on(ctl);
}

static void bufdump(int i)
{
#if 0
	int j;
	void *b;

	b = xmp_smix_buffer();
	do {
		if ((j = write(fd_audio, b, i)) > 0) {
			i -= j;
			b += j;
		} else
			break;
	} while (i);
#endif
}

static void myshutdown()
{
	xmp_smix_off();
}

status_t init_bapp(void *)
{
	be_app = new BApplication("application/x-vnd.101-xmp");
	be_app->Run();
	return B_OK;
}

static void quit_bapp()
{
	be_app->Lock();
	be_app->Quit();
}
