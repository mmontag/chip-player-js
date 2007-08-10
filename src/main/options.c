/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: options.c,v 1.10 2007-08-10 16:50:32 cmatsuoka Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "xmp.h"
#include "../prowizard/list.h"
#include "../prowizard/prowiz.h"

extern struct list_head format_list;

extern char *optarg;
static int o, i;
static char *token;

extern int probeonly;
extern int loadonly;
extern int randomize;
extern int nocmd;
#ifdef HAVE_SYS_RTPRIO_H
extern int rt;
#endif

#define OPT_CHORUS	0x100
#define OPT_REVERB	0x101
#define OPT_NOPAN	0x102
#define OPT_LOADONLY	0x103
#define OPT_NORC	0x104
#define OPT_FX9BUG	0x105
#define OPT_PROBEONLY	0x106
#define OPT_BIGENDIAN	0x107
#define OPT_LILENDIAN	0x108
#define OPT_STDOUT	0x109
#define OPT_STEREO	0x10a
#define OPT_NOCMD	0x10b
#define OPT_REALTIME	0x10c
#define OPT_FIXLOOP	0x10d
#define OPT_CRUNCH	0x10e
#define OPT_NOFILTER	0x10f


static void list_wrap (char *s, int l, int r, int v)
{
    int i;
    static int c = 0;
    static int m = 0;
    char *t;

    if (s == NULL) {
	for (i = 0; i < l; i++)
	    printf (" ");
	c = l;
	m = r;
	return;
    } else if (c > l) {
	c++;
	printf (v ? "," : " ");
    }

    t = strtok (s, " ");

    while (t) {
	if ((c + strlen (t) + 1) > m) {
	    c = l;
	    printf ("\n");
	    for (i = 0; i < l; i++) {
		printf (" ");
	    }
	} else if (c > l) {
	    printf (" ");
	}
	c += strlen (t) + 1;
	printf ("%s", t);
	t = strtok (NULL, " ");
    }
}


static void copyright_header ()
{
    printf ("Extended Module Player %s %s\n", xmp_version, xmp_date);
    printf 
    (
"Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr\n"
"Portions Copyright (C) 1996-1997 Takashi Iwai, (C) 1988 Tammo Hinrichs,\n"
"(C) 1989 Rich Gopstein and Harris Corporation, (C) 1997 Bert Jahn, (C) 1998\n"
"Sylvain Chipaux, (C) 1998,2000 Olivier Lapicque, (C) 1999 Tatsuyuki Satoh\n\n"
    );
}


static void license ()
{
    copyright_header ();

    printf 
    (
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n\n"

"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n\n"

"You should have received a copy of the GNU General Public License\n"
"along with this program; if not, write to the Free Software\n"
"Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n"
    );
}


static void usage (char *s, struct xmp_control *opt)
{
    struct xmp_fmt_info *f, *fmt;
    struct xmp_drv_info *d, *drv;
    struct list_head *tmp;
    struct pw_format *format;
    char **hlp, buf[80];
    int i;

    copyright_header ();
    printf ("%s\n", xmp_build); 

    printf ("Usage: %s [options] [modules]\n", s);

    printf ("\nSupported module formats:\n");
    xmp_get_fmt_info (&fmt);
    list_wrap (NULL, 3, 78, 1);
    for (i = 0, f = fmt; f; i++, f = f->next) {
        snprintf(buf, 80, "%s (%s)", f->suffix, f->tracker);
        list_wrap(buf, 3, 0, 1);
    }

    list_for_each(tmp, &format_list) {
	format = list_entry(tmp, struct pw_format, list);
        snprintf(buf, 80, "%s (%s)", format->id, format->name);
        list_wrap(buf, 3, 0, 1);
	i++;
    }

    snprintf(buf, 80, "[%d known formats]", i);
    list_wrap(buf, 3, 0, 0);
    printf ("\n");

    printf("\nAvailable drivers:\n");

    xmp_get_drv_info (&drv);
    list_wrap (NULL, 3, 78, 1);
    for (d = drv; d; d = d->next) {
        snprintf(buf, 80, "%s (%s)", d->id, d->description);
        list_wrap(buf, 3, 0, 1);
    }

    printf("\n");

    for (d = drv; d; d = d->next) {
	if (d->help)
	    printf ("\n%s options:\n", d->description);
	for (hlp = d->help; hlp && *hlp; hlp += 2)
	    printf ("   -D%-20.20s %s\n", hlp[0], hlp[1]);
    }

    printf (
"\nPlayer control options:\n"
"   -D parameter[=val]     Pass configuration parameter to the output driver\n"
"   -d --driver name       Force output to the specified device\n"
"   --fix-sample-loops     Use sample loop start /2 in MOD/UNIC/NP3\n" 
"   --offset-bug-emulation Emulate Protracker 2.x bug in effect 9\n"
"   -l --loop              Enable module looping\n"
"   -M --mute ch-list      Mute the specified channels\n"
"   --modrange             Limit the octave range to 3 octaves in MOD files\n"
"   --nocmd                Disable interactive commands\n"
"   --norc                 Don't read /etc/xmp/xmp.conf or $HOME/.xmp/xmp.conf\n"
"   -R --random            Random order playing\n"
#ifdef HAVE_SYS_RTPRIO_H
"   --realtime             Run in real-time priority\n" 
#endif
"   -S --solo ch-list      Set channels to solo mode\n"
"   -s --start num         Start from the specified order\n"
"   -T --tempo num         Initial tempo (default 6)\n"
"   -t --time num          Maximum playing time in seconds\n"

"\nPlayer sound options:\n"
"   -8 --8bit              Convert 16 bit samples to 8 bit\n"
"   --chorus num           Chorus depth (if supported)\n"
"   -m --mono              Mono output\n"
"   --nofilter             Disable IT filter\n"
"   --nopan                Disable dynamic panning\n"
"   -P --pan pan           Percentual pan amplitude (default %d%%)\n"
"   -r --reverse           Reverse left/right stereo channels\n"
"   --reverb num           Reverb depth (if supported)\n"
"   --stereo               Stereo output\n"

"\nSoftware mixer options:\n"
"   --big-endian           Use big-endian 16 bit samples\n"
"   -b --bits {8|16}       Software mixer resolution (8 or 16 bits)\n"
"   -c --stdout            Mix the module to stdout\n"
"   -f --frequency rate    Sampling rate in hertz (default %d Hz)\n"
"   -i --interpolate       Enable/disable interpolation (default %s)\n"
"   --little-endian        Use little-endian 16 bit samples\n"
"   -o --output-file name  Mix the module to file ('-' for stdout)\n"
"   -u --unsigned          Set the mixer to use unsigned samples\n"

"\nInformation options:\n"
"   -h --help              Print a summary of the command line options\n"
"   --load-only            Load module and exit\n"
"   --probe-only           Probe audio device and exit\n"
"   -q --quiet             Quiet mode (verbosity level = 0)\n"
"   -V --version           Print version information\n"
"   -v --verbose           Verbose mode (incremental)\n"
	,opt->mix,
	opt->freq,
	opt->flags & XMP_CTL_ITPT ? "enabled" : "disabled"
    );
}


void get_options (int argc, char **argv, struct xmp_control *opt)
{
    int optidx = 0;
#define OPTIONS "8b:cD:d:f:hilLM:mo:P:qRrS:s:T:t:uVv"
    static struct option lopt[] =
    {
	{ "8bit",		 0, 0, '8' },
	{ "big-endian",		 0, 0, OPT_BIGENDIAN },
	{ "bits",		 1, 0, 'b' },
	{ "chorus",		 1, 0, OPT_CHORUS },
	{ "crunch",		 1, 0, OPT_CRUNCH },
	{ "driver",		 1, 0, 'd' },
	{ "fix-sample-loops",	 0, 0, OPT_FIXLOOP },
	{ "frequency",		 1, 0, 'f' },
	{ "offset-bug-emulation",0, 0, OPT_FX9BUG },
	{ "help",		 0, 0, 'h' },
	{ "interpolate",	 0, 0, 'i' },
	{ "little-endian",	 0, 0, OPT_LILENDIAN },
	{ "load-only",	 	 0, 0, OPT_LOADONLY },
	{ "loop",		 0, 0, 'l' },
	{ "license",		 0, 0, 'L' },
	{ "mute",		 1, 0, 'M' },
	{ "mono",		 0, 0, 'm' },
	{ "nocmd",		 0, 0, OPT_NOCMD },
	{ "nofilter",		 0, 0, OPT_NOFILTER },
	{ "nopan",		 0, 0, OPT_NOPAN },
	{ "norc",		 0, 0, OPT_NORC },
	{ "output-file",	 1, 0, 'o' },
	{ "pan",		 1, 0, 'P' },
	{ "probe-only",		 0, 0, OPT_PROBEONLY },
	{ "quiet",		 0, 0, 'q' },
	{ "random",		 0, 0, 'R' },
#ifdef HAVE_SYS_RTPRIO_H
	{ "realtime",		 0, 0, OPT_REALTIME },
#endif
	{ "reverb",		 1, 0, OPT_CHORUS },
	{ "reverse",		 0, 0, 'r' },
	{ "solo",		 1, 0, 'S' },
	{ "start",		 1, 0, 's' },
	{ "stdout",		 0, 0, 'c' },
	{ "stereo",		 0, 0, OPT_STEREO },
	{ "tempo",		 1, 0, 'T' },
	{ "time",		 1, 0, 't' },
	{ "unsigned",		 0, 0, 'u' },
	{ "version",		 0, 0, 'V' },
	{ "verbose",		 0, 0, 'v' },
	{ NULL,	0, 0, 0 }
    };

    i = 0;
    while ((o = getopt_long (argc, argv, OPTIONS, lopt, &optidx)) != -1) {
	switch (o) {
	case '8':
	    opt->flags |= XMP_CTL_8BIT;
	    break;
	case OPT_BIGENDIAN:
	    opt->flags |= XMP_CTL_BIGEND;
	    break;
	case 'b':
	    opt->resol = atoi (optarg);
	    if (opt->resol != 8 && opt->resol != 16)
		opt->resol = 16;
	    break;
	case OPT_CHORUS:
	    opt->chorus = strtoul (optarg, NULL, 0);
	    break;
	case OPT_CRUNCH:
	    opt->crunch = strtoul (optarg, NULL, 0);
	    break;
	case 'c':
	    opt->outfile = "-";
	    break;
	case 'D':
	    xmp_set_driver_parameter (opt, optarg);
	    break;
	case 'd':
	    opt->drv_id = optarg;
	    break;
	case OPT_FIXLOOP:
	    opt->fetch |= XMP_CTL_FIXLOOP;
	    break;
	case OPT_FX9BUG:
	    opt->fetch |= XMP_CTL_FX9BUG;
	    break;
	case 'f':
	    opt->freq = atoi (optarg);
	    break;
	case 'i':
	    opt->flags ^= XMP_CTL_ITPT;
	    break;
	case 'l':
	    opt->flags |= XMP_CTL_LOOP;
	    break;
	case 'L':
	    license ();
	    exit (0);
	case OPT_LILENDIAN:
	    opt->flags &= ~XMP_CTL_BIGEND;
	    break;
	case OPT_LOADONLY:
	    loadonly = 1;
	    break;
	case 'm':
	    opt->outfmt |= XMP_FMT_MONO;
	    break;
	case OPT_NOCMD:
	    nocmd = 1;
	    break;
	case OPT_NOFILTER:
	    opt->flags &= ~XMP_CTL_FILTER;
	    break;
	case OPT_NOPAN:
	    opt->flags &= ~XMP_CTL_DYNPAN;
	    break;
	case OPT_NORC:
	    break;
	case 'o':
	    opt->outfile = optarg;
	    break;
	case 'P':
	    opt->mix = atoi (optarg);
	    if (opt->mix < 0)
		opt->mix = 0;
	    if (opt->mix > 100)
		opt->mix = 100;
	    break;
	case OPT_PROBEONLY:
	    probeonly = 1;
	    opt->verbose = 0;
	    break;
	case 'q':
	    opt->verbose = 0;
	    break;
#ifdef HAVE_SYS_RTPRIO_H
	case OPT_REALTIME:
	    rt = 1;
	    break;
#endif
	case OPT_REVERB:
	    opt->reverb = strtoul (optarg, NULL, 0);
	    break;
	case 'R':
	    randomize = 1;
	    break;
	case 'r':
	    opt->flags |= XMP_CTL_REVERSE;
	    break;
	case 'M':
	case 'S':
	    if (o == 'S')
		xmp_channel_mute (0, 64, 1);
	    token = strtok (optarg, ",");
	    while (token) {
		int a, b;
		char buf[40];
		if (strchr (token, '-')) {
		    b = strcspn (token, "-");
		    strncpy (buf, token, b);
		    a = atoi (buf);
		    strncpy (buf, token + b + 1,
			strlen (token) - b - 1);
		    b = atoi (buf);
		} else
		    a = b = atoi (token);
		for (; b >= a; b--) {
		    if (b < 64)
			xmp_channel_mute (b, 1, (o == 'M'));
		}
		token = strtok (NULL, ",");
	    }
	    break;
	case 's':
	    opt->start = strtoul (optarg, NULL, 0);
	    break;
	case OPT_STEREO:
	    opt->outfmt &= ~XMP_FMT_MONO;
	    break;
	case 'T':
	    opt->tempo = strtoul (optarg, NULL, 0);
	    break;
	case 't':
	    opt->time = strtoul (optarg, NULL, 0);
	    break;
	case 'u':
	    opt->outfmt |= XMP_FMT_UNS;
	    break;
	case 'V':
	    printf ("Extended Module Player %s\n", xmp_version);
	    exit (0);
	case 'v':
	    opt->verbose++;
	    break;
	case 'h':
	    usage (argv[0], opt);
	default:
	    exit (-1);
	}
    }

    /* Set limits */
    if (opt->freq < 1000)
	opt->freq = 1000;	/* Min. rate 1 kHz */
    if (opt->freq > 48000)
	opt->freq = 48000;	/* Max. rate 48 kHz */

    /* Update fetch control */
    opt->fetch = opt->flags;
}
