/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: wav.c,v 1.12 2007-10-02 13:16:52 cmatsuoka Exp $
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


static void writeval_16l(int fd, uint16 v)
{
	uint8 x;

	x = v & 0xff;
	write(fd, &x, 1);

	x = v >> 8;
	write(fd, &x, 1);
}

static void writeval_32l(int fd, uint32 v)
{
	uint8 x;

	x = v & 0xff;
	write(fd, &x, 1);

	x = (v >> 8) & 0xff;
	write(fd, &x, 1);

	x = (v >> 16) & 0xff;
	write(fd, &x, 1);

	x = (v >> 24) & 0xff;
	write(fd, &x, 1);
}

static int init (struct xmp_control *ctl)
{
    char *buf;
    uint32 len = 0, flen;
    uint16 u16, chan;
    uint32 sampling_rate, bytes_per_second;
    uint16 bytes_per_sample, bits_per_sample;

    if (!ctl->outfile)
	ctl->outfile = "xmp.wav";

    fd = strcmp (ctl->outfile, "-") ? creat (ctl->outfile, 0644) : 1;

    buf = malloc (strlen (drv_wav.description) + strlen (ctl->outfile) + 8);
    if (strcmp (ctl->outfile, "-")) {
	sprintf (buf, "%s: %s", drv_wav.description, ctl->outfile);
	drv_wav.description = buf;
    } else {
	drv_wav.description = "WAV writer: stdout";
	len = -1;
    }

    write(fd, "RIFF", 4);
    writeval_32l(fd, len);
    write(fd, "WAVE", 4);

    flen = 0x10;
    u16 = 1;
    chan = ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    sampling_rate = ctl->freq;
    bits_per_sample = ctl->resol;
    bytes_per_sample = bits_per_sample / 8;
    bytes_per_second = sampling_rate * chan * bytes_per_sample;

    write(fd, "fmt ", 4);
    writeval_32l(fd, flen);
    writeval_16l(fd, u16);
    writeval_16l(fd, chan);
    writeval_32l(fd, sampling_rate);
    writeval_32l(fd, bytes_per_second);
    writeval_16l(fd, bytes_per_sample);
    writeval_16l(fd, bits_per_sample);

    write(fd, "data", 4);
    writeval_32l(fd, len);

    size = 0;

    return xmp_smix_on (ctl);
}


static void bufdump (int i)
{
    char *b;

    b = xmp_smix_buffer();
    if (big_endian)
	xmp_cvt_sex(i, b);
    write(fd, b, i);
    size += i;
}


static void shutdown ()
{
    xmp_smix_off();

    lseek(fd, 40, SEEK_SET);
    writeval_32l(fd, size);

    lseek(fd, 4, SEEK_SET);
    writeval_32l(fd, size + 40);

    if (fd)
	close (fd);
}
