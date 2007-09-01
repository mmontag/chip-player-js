/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: bsp.c,v 1.4 2007-09-01 02:04:52 cmatsuoka Exp $
 */

/*
 * Based on the BeOS (BSoundPlayer) mpg123 driver written by
 * Attila Lendvai <101@inf.bme.hu>
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

static int init (struct xmp_control *);
static void bufdump (int);
static void myshutdown ();
static void mysync ();

static void dummy () { }

static char *help[] = {
    "dev=<device_name>", "Audio device name (default is /dev/dsp)",
    "buffer=val", "Audio buffer size (default is 32768)",
    NULL
};

struct xmp_drv_info drv_arts = {
    "beos",		/* driver ID */
    "BeOS PCM audio",	/* driver description */
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
    mysync,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};


static void bufferproc (void *, void *buffer, size_t size, const
	media_raw_audio_format &format)
{
    pcm_sample = (uint8 *)buffer;
    pcm_point = 0;
    need_locking = true;
    release_sem (startsem);
    acquire_sem (endsem);
}

static void beos_replay (struct frame *fr, int startFrame, int numframes,
	bool verbose, bool doublespeed) {
    int frameNum = 0;
    status_t rc;
    media_raw_audio_format fmt = {
        44100.0,
        2,
        media_raw_audio_format :: B_AUDIO_SHORT,
        B_MEDIA_LITTLE_ENDIAN,
        (usebuffer < 4 ? 4 : usebuffer) * 1024
    };

    if ((startsem = create_sem (0, "xmp startsem")) < 0  ||
        (endsem = create_sem(0, "xmp endsem")) < 0) {
            return;
    }
    
    need_locking = false;
    
    BSoundPlayer player (&fmt, "xmp", bufferproc);

    if (player.InitCheck() != B_OK) {
        fprintf(stderr, "FATAL: BSoundPlayer init failed\n");
        exit(1);
    }

    if ((rc = player.Start()) < 0) {
        fprintf(stderr, "BSoundPlayer::Start() returned 0x%lx (%s)\n",
	    (int32)rc, strerror(rc));
        exit(1);
    }
    player.SetHasData(true);

    set_thread_priority (find_thread(0), 80);

    acquire_sem (startsem);

    for (frameNum=0; read_frame(fr) && numframes && !intflag; frameNum++) {
        if (frameNum < startFrame || (doublespeed && (frameNum % doublespeed))) {
            if (fr -> lay == 3)
                set_pointer (512);
            continue;
        }
        numframes--;
        
        if (fr->error_protection) {
            getbits(16); /* crc */
        }
        
        (fr->do_layer) (fr, outmode, (struct audio_info_struct *)0);
    }
    
    player.Stop();
    
    delete_sem(startsem);
    delete_sem(endsem);
}

static int init (struct xmp_control *ctl)
{
    int rc, rate, bits, stereo, bsize;
    char *dev;
    char *token;
    char **parm = ctl->parm;
 
    parm_init ();
    chkparm1 ("dev", dev = token);
    chkparm1 ("buffer", bsize = atoi (token));
    parm_end ();

    rate = ctl->freq;
    bits = ctl->resol;
    stereo = 1;
    bufsize = 32 * 1024;

    return xmp_smix_on (ctl);
}

static void bufdump (int i)
{
    int j;
    void *b;

    b = xmp_smix_buffer ();
    do {
	if ((j = write (fd_audio, b, i)) > 0) {
	    i -= j;
	    b += j;
	} else
	    break;
    } while (i);
}

static void myshutdown ()
{
    xmp_smix_off ();
    close (fd_audio);
}

static void mysync ()
{
    ioctl (fd, SNDCTL_DSP_SYNC, NULL);
}

status_t init_bapp (void *) {
    be_app = new BApplication("application/x-vnd.101-xmp");
    be_app->Run();
    return B_OK;
}

static void quit_bapp ()
{
    be_app->Lock();
    be_app->Quit();
}

