/* ALSA 0.5 driver for xmp
 * Copyright (C) 1996-2000 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * Fixed for ALSA 0.5 by Rob Adamson <R.Adamson@fitz.cam.ac.uk>
 * Sat, 29 Apr 2000 17:10:46 +0100 (BST)
 *
 * $Id: alsa05.c,v 1.1 2005-02-23 17:07:57 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/asoundlib.h>

#include "driver.h"
#include "mixer.h"

static int init (struct xmp_control *);
static int init2 (struct xmp_control *);
static int prepare_driver (void);
static void dshutdown (void);
static int to_fmt (struct xmp_control *);
/*static void from_fmt (struct xmp_control *, int outfmt);*/
static void bufdump (int);
static void bufwipe (void);
static void flush (void);
static void dsync (double);

static void dummy () { }

static char *help[] = {
    "frag=num,size", "Set the number and size (bytes) of fragments",
    "card <name>", "Select sound card to use",
    NULL
};

struct xmp_drv_info drv_alsa_mix = {
    "alsa05",		/* driver ID */
    "ALSA 0.5 PCM audio",/* driver description */
    help,		/* help */
    init,		/* init */
    dshutdown,		/* shutdown */
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
    flush,		/* stctlimer */
    dummy,		/* reset */
    bufdump,		/* bufdump */
    bufwipe,		/* bufwipe */
    dummy,		/* clearmem */
    dsync,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};

static snd_pcm_t *pcm_handle;

static int frag_num = 4;
static size_t frag_size = 4096;
static char *mybuffer = NULL;  /* malloc'd */
static char *mybuffer_nextfree = NULL;
static char *card_name;


static int init (struct xmp_control *ctl)
{
  int r = init2 (ctl);
  if (r<0) return r;
  return xmp_smix_on (ctl);
}


static int init2 (struct xmp_control *ctl)
{
  /* preliminary alsa 0.5 support, Tijs van Bakel, 02-03-2000.
     only default values are supported and music sounds chunky */

  /* Better ALSA 0.5 support, Rob Adamson, 16 Mar 2000.
     Again, hard-wired fragment size & number and sample rate,
     but it plays smoothly now. */

  /* Now uses specified options - Rob Adamson, 20 Mar 2000 */
  
  //  snd_pcm_format_t format;
  snd_pcm_channel_params_t params;
  snd_pcm_channel_setup_t setup;
  
  int card = 0;
  int dev = 0;
  int retcode;

  char *token, **parm; /* used by parm_init...chkparm...parm_end */

  parm_init ();  /* NB: this is a macro */
  chkparm2 ("frag", "%d,%d", &frag_num, &frag_size); /* NB: macro */
  if (frag_num>8) frag_num = 8;
  if (frag_num<0) frag_num = 4;
  if (frag_size>65536) frag_size = 65536;
  if (frag_size<16) frag_size = 16;
  chkparm1 ("card", card_name = token);	 /* NB: macro */
  //  card = snd_card_name (card_name); /* ? */
  //  dev = snd_defaults_pcm_device (); /* ? */
  parm_end ();  /* NB: macro */

#if 0
  fprintf(stderr, "\nalsa_mix arguments: frag_num=%d, frag_size=%d\n", frag_num, frag_size);
#endif

  if (mybuffer) { free(mybuffer), mybuffer = mybuffer_nextfree = NULL; }
  mybuffer = malloc(frag_size);
  if (mybuffer) {
    mybuffer_nextfree = mybuffer;
  } else {
    printf("Unable to allocate memory for ALSA mixer buffer\n");
    return XMP_ERR_DINIT;
  }

  if ( (retcode = snd_pcm_open(&pcm_handle, card, dev, SND_PCM_OPEN_PLAYBACK)) < 0)
    {
      printf("Unable to initialize ALSA pcm device: %s\n", snd_strerror(retcode));
      return XMP_ERR_DINIT;
    }

  memset(&params, 0, sizeof(snd_pcm_channel_params_t));

  params.mode = SND_PCM_MODE_BLOCK;
  params.buf.block.frag_size = frag_size;
  params.buf.block.frags_min = 1;
  params.buf.block.frags_max = frag_num;

  //params.mode = SND_PCM_MODE_STREAM;
  //params.buf.stream.queue_size = 16384;
  //params.buf.stream.fill = SND_PCM_FILL_NONE;
  //params.buf.stream.max_fill = 0;

  params.channel = SND_PCM_CHANNEL_PLAYBACK;
  params.start_mode = SND_PCM_START_FULL;
  params.stop_mode = SND_PCM_STOP_ROLLOVER;

  /* NEED TO GET THIS STUFF FROM ctl */
  //  memset(&format, 0, sizeof(format));
  params.format.interleave = 1;  
  params.format.format = to_fmt (ctl); /* was: SND_PCM_SFMT_S16_LE */
  params.format.rate = ctl->freq;
  params.format.voices = (ctl->outfmt & XMP_FMT_MONO) ? 1 : 2;
  //  memcpy(&params.format, &format, sizeof(format));

#if 0
  fprintf(stderr, "alsa_mix format: %d\n", params.format.format);
  fprintf(stderr, "alsa_mix sample rate: %d\n", params.format.rate);
  fprintf(stderr, "alsa_mix voices: %d\n", params.format.voices);
#endif

  if ( (retcode = snd_pcm_plugin_params(pcm_handle, &params)) < 0 )
    {
      printf("Unable to set ALSA output parameters: %s\n", snd_strerror(retcode));
      return XMP_ERR_DINIT;
    }

  if ( prepare_driver () < 0 ) return XMP_ERR_DINIT;
  
  memset(&setup, 0, sizeof(setup));
  setup.mode = SND_PCM_MODE_STREAM;
  setup.channel = SND_PCM_CHANNEL_PLAYBACK;
  
  if ( (retcode = snd_pcm_channel_setup(pcm_handle, &setup)) < 0 )
    {
      printf("Unable to setup ALSA channel: %s\n", snd_strerror(retcode));
      return XMP_ERR_DINIT;
    }
  
  return 0;
}


static int prepare_driver (void) {
  int retcode;
  if ( (retcode = snd_pcm_plugin_prepare(pcm_handle, SND_PCM_CHANNEL_PLAYBACK)) < 0 )
    {
      printf("Unable to prepare ALSA plugin: %s\n", snd_strerror(retcode));
      return retcode;
    }
  return 0;
}


static int to_fmt (struct xmp_control *ctl)
{
    int fmt;

    if (ctl->resol == 0) {
      /* fprintf(stderr, "to_fmt 1\n"); */
      return SND_PCM_SFMT_MU_LAW;
    }

    if (ctl->resol == 8) {
      /* fprintf(stderr, "to_fmt 2\n"); */
      fmt = SND_PCM_SFMT_U8 | SND_PCM_SFMT_S8;
    } else {
      /* fprintf(stderr, "to_fmt 3\n"); */
      fmt = SND_PCM_SFMT_S16_LE | SND_PCM_SFMT_S16_BE | SND_PCM_SFMT_U16_LE | SND_PCM_SFMT_U16_BE;

      if (ctl->outfmt & XMP_FMT_BIGEND) {
/*	fprintf(stderr, "to_fmt 4\n"); */
	fmt &= SND_PCM_SFMT_S16_BE | SND_PCM_SFMT_U16_BE;
      } else {
/*	fprintf(stderr, "to_fmt 5\n"); */
	fmt &= SND_PCM_SFMT_S16_LE | SND_PCM_SFMT_U16_LE;
      }
    }
    if (ctl->outfmt & XMP_FMT_UNS) {
/*      fprintf(stderr, "to_fmt 6\n"); */
      fmt &= SND_PCM_SFMT_U8 | SND_PCM_SFMT_U16_LE | SND_PCM_SFMT_U16_BE;
    } else {
/*      fprintf(stderr, "to_fmt 7\n"); */
      fmt &= SND_PCM_SFMT_S8 | SND_PCM_SFMT_S16_LE | SND_PCM_SFMT_S16_BE;
    }

/*    fprintf(stderr, "to_fmt - returning %x\n", fmt); */

    return fmt;
}

/*
static void from_fmt (struct xmp_control *ctl, int outfmt)
{
    if (outfmt & AFMT_MU_LAW) {
	ctl->resol = 0;
	return;
    }

    if (outfmt & (AFMT_S16_LE | AFMT_S16_BE | AFMT_U16_LE | AFMT_U16_BE))
	ctl->resol = 16;
    else
	ctl->resol = 8;

    if (outfmt & (AFMT_U8 | AFMT_U16_LE | AFMT_U16_BE))
	ctl->outfmt |= XMP_FMT_UNS;
    else
	ctl->outfmt &= ~XMP_FMT_UNS;

    if (outfmt & (AFMT_S16_BE | AFMT_U16_BE))
	ctl->outfmt |= XMP_FMT_BIGEND;
    else
	ctl->outfmt &= ~XMP_FMT_BIGEND;
}
*/

static void bufwipe (void)
{
  //  fprintf(stderr, "alsa_mix.bufwipe called\n");
  mybuffer_nextfree = mybuffer;
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump (int i)
{
  //    int j;
    void *b;

    //    printf("bufdump(%d) ", i);

    b = xmp_smix_buffer ();
    /* Note this assumes a fragment size of (frag_size) */
    while (i>0) {
      size_t f = (frag_size) - (mybuffer_nextfree - mybuffer);
      size_t to_copy = (f<i) ? f : i;
      //      printf("f=%d,i=%d ", f, i);
      memcpy(mybuffer_nextfree, b, to_copy);
      b += to_copy;
      mybuffer_nextfree += to_copy;
      f -= to_copy;
      i -= to_copy;
      if (f==0) {
	snd_pcm_plugin_write (pcm_handle, mybuffer, frag_size);
	//	do {
	// if ((j = snd_pcm_plugin_write (pcm_handle, mybuffer, frag_size)) > 0) {
	// do nothing
	//  } else {
	//	    usleep(20000);
	//  }
	//printf("%d ", j);
	//	} while (j<0);
	mybuffer_nextfree = mybuffer;
      }
    }
    //    putchar('\n');
}


static void dsync(double t) {  /* t is number of centiseconds? */
  //printf("sync(%f) ", t);
  //  usleep(5000);
}

static void dshutdown ()
{
  //  fprintf(stderr, "alsa_mix.dshutdown called\n");
  xmp_smix_off ();
  snd_pcm_close (pcm_handle);
}


static void flush ()
{
/*  fprintf (stderr, "alsa_mix.flush called\n"); */
  snd_pcm_plugin_flush (pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
  prepare_driver ();
}
