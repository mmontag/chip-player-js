/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: wav.c,v 1.1 2001-11-09 08:24:27 cmatsuoka Exp $
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
static uint32 size;

static int init (struct xmp_control *);
static void bufdump (int);
static void shutdown ();

static void dummy () { }

struct xmp_drv_info drv_wav = {
    "wav",		/* driver ID */
    "WAV writer",	/* driver description */
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
    uint32 len = 0, flen;
    uint16 u16, chan;
    uint32 sampling_rate, bytes_per_second;
    uint16 bytes_per_sample, bits_per_sample;

    if (!ctl->outfile)
	ctl->outfile = "xmp.wav";

    audio_fd = strcmp (ctl->outfile, "-") ? creat (ctl->outfile, 0644) : 1;

    buf = malloc (strlen (drv_wav.description) + strlen (ctl->outfile) + 8);
    if (strcmp (ctl->outfile, "-")) {
	sprintf (buf, "%s: %s", drv_wav.description, ctl->outfile);
	drv_wav.description = buf;
    } else {
	drv_wav.description = "WAV writer: stdout";
	len = -1;
    }

    write (audio_fd, "RIFF", 4);
    write (audio_fd, &len, 4);
    write (audio_fd, "WAVE", 4);

    flen = 0x10;
    u16 = 1;
    chan = ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    sampling_rate = ctl->freq;
    bits_per_sample = ctl->resol;
    bytes_per_sample = bits_per_sample / 8;
    bytes_per_second = sampling_rate * chan * bytes_per_sample;

    L_ENDIAN32(flen);
    L_ENDIAN16(u16);
    L_ENDIAN16(chan);
    L_ENDIAN32(sampling_rate);
    L_ENDIAN32(bytes_per_second);
    L_ENDIAN16(bytes_per_sample);
    L_ENDIAN16(bits_per_sample);

    write (audio_fd, "fmt ", 4);
    write (audio_fd, &flen, 4);
    write (audio_fd, &u16, 2);
    write (audio_fd, &chan, 2);
    write (audio_fd, &sampling_rate, 4);
    write (audio_fd, &bytes_per_second, 4);
    write (audio_fd, &bytes_per_sample, 2);
    write (audio_fd, &bits_per_sample, 2);

    write (audio_fd, "data", 4);
    write (audio_fd, &len, 4);

    size = 0;

    return xmp_smix_on (ctl);
}


static void bufdump (int i)
{
    int j;
    void *b;

    b = xmp_smix_buffer ();
    while (i) {
	if ((j = write (audio_fd, b, i)) > 0) {
	    i -= j;
	    (char *)b += j;
	    size += j;
	} else
	    break;
    }
}


static void shutdown ()
{
    uint32 len;

    xmp_smix_off ();

    len = size;
    L_ENDIAN32(len);
    lseek (audio_fd, 40, SEEK_SET);
    write (audio_fd, &len, 4);

    len = size + 40;
    L_ENDIAN32(len);
    lseek (audio_fd, 4, SEEK_SET);
    write (audio_fd, &len, 4);

    if (audio_fd)
	close (audio_fd);
}

XMP_DRV_REGISTER(drv_wav)
