/* ALSA driver for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 * Based on the ALSA 0.5 driver for xmp, Copyright (C) 2000 Tijs
 * van Bakel and Rob Adamson.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: alsa.c,v 1.2 2005-02-24 12:52:12 cmatsuoka Exp $
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
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "driver.h"
#include "mixer.h"

static int init (struct xmp_control *);
static int prepare_driver (void);
static void dshutdown (void);
static int to_fmt (struct xmp_control *);
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
	"alsa",			/* driver ID */
	"ALSA PCM audio",	/* driver description */
	help,			/* help */
	init,			/* init */
	dshutdown,		/* shutdown */
	xmp_smix_numvoices,	/* numvoices */
	dummy,			/* voicepos */
	xmp_smix_echoback,	/* echoback */
	dummy,			/* setpatch */
	xmp_smix_setvol,	/* setvol */
	dummy,			/* setnote */
	xmp_smix_setpan,	/* setpan */
	dummy,			/* setbend */
	xmp_smix_seteffect,	/* seteffect */
	dummy,			/* starttimer */
	flush,			/* stctlimer */
	dummy,			/* reset */
	bufdump,		/* bufdump */
	bufwipe,		/* bufwipe */
	dummy,			/* clearmem */
	dsync,			/* sync */
	xmp_smix_writepatch,	/* writepatch */
	xmp_smix_getmsg,	/* getmsg */
	NULL
};

static snd_pcm_t *pcm_handle;

static int frag_num = 4;
static size_t frag_size = 4096;
static char *mybuffer = NULL;  /* malloc'd */
static char *mybuffer_nextfree = NULL;


static int init (struct xmp_control *ctl)
{
	snd_pcm_hw_params_t *hwparams;
	int retcode;
	char *token, **parm; /* used by parm_init...chkparm...parm_end */
	unsigned int channels, rate;
	unsigned int btime, ptime;
	char *card_name = "default";

	parm_init();  		/* NB: this is a macro */
	chkparm2("frag", "%d,%d", &frag_num, &frag_size); /* NB: macro */
	if (frag_num > 8) frag_num = 8;
	if (frag_num < 0) frag_num = 4;
	if (frag_size > 65536) frag_size = 65536;
	if (frag_size < 16) frag_size = 16;
	chkparm1("card", card_name = token);	/* NB: macro */
	parm_end();				/* NB: macro */

	if (mybuffer) {
		free(mybuffer),
		mybuffer = mybuffer_nextfree = NULL;
	}
	mybuffer = malloc(frag_size);

	if (mybuffer) {
		mybuffer_nextfree = mybuffer;
	} else {
		printf("Unable to allocate memory for ALSA mixer buffer\n");
		return XMP_ERR_DINIT;
	}

	if ((retcode = snd_pcm_open(&pcm_handle, card_name,
		SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		printf("Unable to initialize ALSA pcm device: %s\n", snd_strerror(retcode));
		return XMP_ERR_DINIT;
	}

	channels = (ctl->outfmt & XMP_FMT_MONO) ? 1 : 2;
	rate = ctl->freq;

	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_hw_params_any(pcm_handle, hwparams);
	snd_pcm_nonblock(pcm_handle, 0);
	snd_pcm_hw_params_set_access(pcm_handle, hwparams,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm_handle, hwparams, to_fmt(ctl));
	snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &rate, 0);
	snd_pcm_hw_params_set_channels_near(pcm_handle, hwparams, &channels);
	snd_pcm_hw_params_set_buffer_time_near(pcm_handle, hwparams, &btime, 0);
	snd_pcm_hw_params_set_period_time_near(pcm_handle, hwparams, &ptime, 0);
	
	if ((retcode = snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
		printf("Unable to set ALSA output parameters: %s\n",
			snd_strerror(retcode));
		return XMP_ERR_DINIT;
	}

	if (prepare_driver() < 0)
		return XMP_ERR_DINIT;
  
	return xmp_smix_on(ctl);
}


static int prepare_driver(void)
{
	int retcode;

	if ((retcode = snd_pcm_prepare(pcm_handle)) < 0 ) {
		printf("Unable to prepare ALSA plugin: %s\n",
			snd_strerror(retcode));
		return retcode;
	}

	return 0;
}


static int to_fmt(struct xmp_control *ctl)
{
	int fmt;

	switch (ctl->resol) {
	case 0:
		return SND_PCM_FORMAT_MU_LAW;
	case 8:
		fmt = SND_PCM_FORMAT_U8 | SND_PCM_FORMAT_S8;
		break;
	default:
		if (ctl->outfmt & XMP_FMT_BIGEND) {
			fmt = SND_PCM_FORMAT_S16_BE | SND_PCM_FORMAT_U16_BE;
		} else {
			fmt = SND_PCM_FORMAT_S16_LE | SND_PCM_FORMAT_U16_LE;
		}
	}

	if (ctl->outfmt & XMP_FMT_UNS) {
      		fmt &= SND_PCM_FORMAT_U8 | SND_PCM_FORMAT_U16_LE |
			SND_PCM_FORMAT_U16_BE;
	} else {
		fmt &= SND_PCM_FORMAT_S8 | SND_PCM_FORMAT_S16_LE |
			SND_PCM_FORMAT_S16_BE;
	}

	return fmt;
}


static void bufwipe (void)
{
	mybuffer_nextfree = mybuffer;
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump (int i)
{
	void *b;

	b = xmp_smix_buffer ();

	while (i>0) {
		size_t f = (frag_size) - (mybuffer_nextfree - mybuffer);
		size_t to_copy = (f<i) ? f : i;
		memcpy(mybuffer_nextfree, b, to_copy);
		b += to_copy;
		mybuffer_nextfree += to_copy;
		f -= to_copy;
		i -= to_copy;
		if (f == 0) {
			int frames;
			frames = snd_pcm_bytes_to_frames(pcm_handle, frag_size);
			snd_pcm_writei(pcm_handle, mybuffer, frames);
			mybuffer_nextfree = mybuffer;
		}
	}
}


static void dsync(double t) {  /* t is number of centiseconds? */
	//printf("sync(%f) ", t);
	//  usleep(5000);
}

static void dshutdown ()
{
	/* fprintf(stderr, "alsa_mix.dshutdown called\n"); */
	xmp_smix_off();
	snd_pcm_close(pcm_handle);
}


static void flush ()
{
#if 0
	snd_pcm_plugin_flush(pcm_handle, SND_PCM_CHANNEL_PLAYBACK);
#endif
	prepare_driver();
}
