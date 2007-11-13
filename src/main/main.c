/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: main.c,v 1.33 2007-11-13 22:29:31 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Fri, 05 Jun 1998 16:46:13 -0700  "Mark R. Boyns" <boyns@sdsu.edu>
 * I needed to use xmp as a filter and was able to use /dev/fd/0 as an
 * input source but ran into problems with xmp trying to read interactive
 * commands. So, I added a few lines of code to disable command reading
 * with the --nocmd option.
 */

/*
 * Wed, 10 Jun 1998 15:42:25 -0500 Douglas Carmichael <dcarmich@dcarmichael.net>
 * Support for real-time priority in FreeBSD.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>

#ifdef HAVE_SYS_RTPRIO_H
#include <sys/resource.h>
#include <sys/rtprio.h>
#endif

#ifdef __EMX__
#define INCL_BASE
#include <os2.h>
#endif

#ifdef WIN32
#include "conio.h"
#endif

#include "xmp.h"

extern int optind;

/* Options not belonging to libxmp */
int probeonly = 0;
int randomize = 0;
int loadonly = 0;
int nocmd = 0;
#ifdef HAVE_SYS_RTPRIO_H
int rt = 0;
#endif

static struct xmp_module_info mi;

static int verbosity;
#ifdef HAVE_TERMIOS_H
static struct termios term;
#endif
static int background = 0;
#ifdef SIGTSTP
static int stopped = 0;
#endif
static int refresh_status = 0;
static int max_nch = 0;
static double rows, tot_nch;
int skip = 0;

static int sigusr = 0;

void get_options (int, char **, struct xmp_options *, xmp_context);

static xmp_context ctx;


static int set_tty ()
{
#ifdef HAVE_TERMIOS_H
    struct termios t;

    if (background)
	return -1;

    if (tcgetattr (0, &term) < 0)
	return -1;

    t = term;
    t.c_lflag &= ~(ECHO | ICANON | TOSTOP);
    t.c_cc[VMIN] = t.c_cc[VTIME] = 0;

    if (tcsetattr (0, TCSAFLUSH, &t) < 0)
	return -1;
#endif

    return 0;
}


static int reset_tty ()
{
#ifdef HAVE_TERMIOS_H
    if (background)
	return -1;

    if (tcsetattr (0, TCSAFLUSH, &term) < 0) {
	fprintf (stderr, "can't reset terminal!\n");
	return -1;
    }
#endif

    return 0;
}

#ifdef SIGTSTP
static void sigtstp_handler ()
{
    if (!stopped) {
	fprintf (stderr, "] - STOPPED\n");
	xmp_timer_stop(ctx);
	stopped = 1;
    }

    signal (SIGTSTP, SIG_DFL);
    kill (getpid (), SIGTSTP);
}


static void sigcont_handler ()
{
    background = (tcgetpgrp (0) == getppid ());

    if (background)
        reset_tty();
    else
        set_tty();

    if (stopped)
	xmp_timer_restart(ctx);

    stopped = 0;
    refresh_status = 1;

    /* Unlike BSD systems, signals under Linux are reset to their
     * default behavior when raised.
     */
    signal(SIGCONT, sigcont_handler);
    signal(SIGTSTP, sigtstp_handler);
}
#endif

static void sigusr_handler (int i)
{
    signal (sigusr = i, sigusr_handler);
}


static void cleanup (int s)
{
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);

    fprintf (stderr, "\n*** Interrupted: signal %d caught\n", s);
    xmp_stop_module(ctx);
    xmp_close_audio(ctx);

    reset_tty ();

    exit (-2);
}


static void process_echoback(unsigned long i)
{
    unsigned long msg = i >> 4;
    unsigned char cmd;
    static int _pos = -1;
    static int tpo, bpm, nch, _tpo = -1, _bpm = -1;
    static int pos, pat;
    static int pause = 0;
    int k;

#ifdef SIGUSR1
    if (sigusr == SIGUSR1) {
	skip = 1;
	xmp_mod_stop(ctx);
	if (pause)
	    pause = xmp_mod_pause(ctx);
	sigusr = 0;
    } else if (sigusr == SIGUSR2) {
	skip = -1;
	xmp_mod_stop(ctx);
	if (pause)
	    pause = xmp_mod_pause(ctx);
	sigusr = 0;
    }
#endif

    if (background)
	return;

    if (verbosity) {
	switch (i & 0xf) {
	case XMP_ECHO_BPM:
	    _bpm = bpm;
	    _tpo = tpo;
	    bpm = msg & 0xff;
	    tpo = msg >> 8;
	    break;
	case XMP_ECHO_ORD:
	    pos = msg & 0xff;
	    pat = msg >> 8;
	    break;
	case XMP_ECHO_NCH:
	    nch = msg & 0xff;
	    if (nch > max_nch)
		max_nch = nch;
	    break;
	case XMP_ECHO_ROW:
	    rows += 1;
	    tot_nch += nch;
	    if
(refresh_status || !(msg & 0xff) || pos != _pos || bpm != _bpm || tpo != _tpo) {
	        fprintf (stderr,
"\rTempo[%02X] BPM[%02X] Pos[%02X/%02X] Pat[%02X/%02X] Row[  /  ] Chn[  /  ]\b",
		    tpo, bpm, pos, mi.len - 1, pat, mi.pat - 1);
		_pos = pos;
		if (refresh_status)
		    refresh_status = 0;
	    }
	    fprintf (stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%02X/%02X] Chn[%02X/%02X",
		(int)(msg & 0xff), (int)(msg >> 8), nch, max_nch);
	    break;
	}
    }

    /* Interactive commands */

    if (nocmd)
	return;

#if defined HAVE_TERMIOS_H && !defined WIN32
    k = read(0, &cmd, 1);
#elif defined WIN32
    k = cmd = kbhit() ? getch() : 0;
#else
    /* Amiga CLI */
    if (WaitForChar(Input(), 1)) {
	char c;
	Read(in, &c, 1);
	cmd = k = c;
    }
#endif

    if (k > 0) {
	switch (cmd) {
	case 'q':	/* quit */
	    skip = -2;
	    xmp_mod_stop(ctx);
	    if (pause)
		pause = xmp_mod_pause(ctx);
	    break;
	case 'f':	/* jump to next order */
	    xmp_ord_next(ctx);
	    if (pause)
		pause = xmp_mod_pause(ctx);
	    break;
	case 'b':	/* jump to previous order */
	    xmp_ord_prev(ctx);
	    if (pause)
		pause = xmp_mod_pause(ctx);
	    break;
	case 'n':	/* skip to next module */
	    skip = 1;
	    xmp_mod_stop(ctx);
	    if (pause)
		pause = xmp_mod_pause(ctx);
	    break;
	case 'p':	/* skip to previous module */
	    skip = -1;
	    xmp_mod_stop(ctx);
	    if (pause)
		pause = xmp_mod_pause(ctx);
	    break;
	case ' ':	/* pause module */
	    fprintf (stderr, "%s",  (pause = xmp_mod_pause(ctx))
		? "] - PAUSED\b\b\b\b\b\b\b\b\b\b"
		: "]         \b\b\b\b\b\b\b\b\b\b");
	    break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    xmp_channel_mute(ctx, cmd - '1', 1, -1);
	    break;
	case '0':
	    xmp_channel_mute(ctx, 9, 1, -1);
	    break;
	case '!':
	    xmp_channel_mute(ctx, 0, 8, 0);
	    break;
	}
    }
}


static void shuffle (int argc, char **argv)
{
    int i, j;
    char *x;

    for (i = 1; i < argc; i++) {
	j = 1 + rand () % (argc - 1);
	x = argv[i];
	argv[i] = argv[j];
	argv[j] = x;
    }
}


int main (int argc, char **argv)
{
    int i, t, lf_flag, first, num_mod, verb = 0;
    time_t t0, t1;
    struct timeval tv;
    struct timezone tz;
    struct xmp_options *opt;
#ifdef HAVE_SYS_RTPRIO_H
    struct rtprio rtp;
#endif
    int getprevious = 0, skipprev = 0;
#ifdef __EMX__
    long rc;
#endif

    ctx = xmp_create_context();

    xmp_init(ctx, argc, argv);
    opt = xmp_get_options(ctx);

    opt->verbosity = 1;
    get_options(argc, argv, opt, ctx);
    verbosity = opt->verbosity;

    if (!(probeonly || argv[optind])) {
	fprintf (stderr, "%s: no modules to play\n"
		"Use `%s --help' for more information.\n", argv[0], argv[0]);
	exit (-1);
    }

    gettimeofday(&tv, &tz);
    srand(tv.tv_usec);

    if (randomize)
	shuffle(argc - optind + 1, &argv[optind - 1]);

    if (opt->outfile && (!opt->drv_id || strcmp(opt->drv_id, "wav")))
	opt->drv_id = "file";

    global_filename = argv[optind];

#ifdef HAVE_TERMIOS_H
    if ((background = (tcgetpgrp (0) == getppid ()))) {
	verb = opt->verbosity;
	opt->verbosity = 0;
	i = xmp_open_audio(ctx);
	xmp_verbosity_level(ctx, opt->verbosity = verb);
    } else
#endif
    {
	i = xmp_open_audio(ctx);
    }

    if (i < 0) {
	fprintf (stderr, "%s: ", argv[0]);
	switch (i) {
	case XMP_ERR_DINIT:
	    fprintf (stderr, "can't initialize driver\n");
	    return -1;
	case XMP_ERR_NODRV:
	    fprintf (stderr, "driver does not exist\n");
	    return -2;
	case XMP_ERR_DSPEC:
	    fprintf (stderr, "driver not specified\n");
	    return -3;
	default:
	    fprintf (stderr, "unknown error\n");
	    return -128;
	}
    }

    xmp_register_event_callback(process_echoback);

    if (opt->verbosity) {
	fprintf (stderr, "Extended Module Player %s %s\n"
	"Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr\n",
	    xmp_version, xmp_date);
#ifdef __EMX__
	fprintf(stderr, "OS/2 Port by Kevin Langman (langman@earthling.net)\n" );
#endif
    }

    if (probeonly || (opt->verbosity)) {
	int srate, res, chn, itpt;
	
	xmp_get_driver_cfg(ctx, &srate, &res, &chn, &itpt);
	fprintf(stderr, "Using %s\n", (char*)xmp_get_driver_description(ctx));
	if (srate) {
	    fprintf(stderr, "Mixer set to %dbit, %d Hz, %s%s\n", res, srate,
		itpt ? "interpolated " : "", chn > 1 ? "stereo" : "mono");
	}
    }

    if (probeonly)
	exit(0);

    /*
     * Support for real-time priority by Douglas Carmichael <dcarmich@mcs.com>
     */
#ifdef HAVE_SYS_RTPRIO_H
    if (rt) {
	rtp.type = RTP_PRIO_REALTIME;
	rtp.prio = 0;
	if (rtprio (RTP_SET, 0, &rtp) == -1) {
	    fprintf (stderr, "%s: Can't get realtime priority.\n", argv[0]);
	    exit(1);
	}
    }
#endif

#ifdef __EMX__
    rc = DosSetPriority (PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MINIMUM, 0);
    if (rc != 0) {
	printf ("Error setting Priority. Priority defaulting to the System Default.\n" );
   }
#endif

    signal(SIGTERM, cleanup);
    signal(SIGINT, cleanup);
    signal(SIGFPE, cleanup);
    signal(SIGSEGV, cleanup);
#ifdef SIGQUIT
    signal(SIGQUIT, cleanup);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, sigtstp_handler);
#endif
#ifdef SIGCONT
    signal(SIGCONT, sigcont_handler);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, sigusr_handler);
    signal(SIGUSR2, sigusr_handler);
#endif

    set_tty ();

    time (&t0);

    for (first = optind, num_mod = lf_flag = 0; optind < argc; optind++) {
	if (getprevious) {
	    optind -= 2;
	    if (optind < first) {
		optind += 2;
	    }
	}

	if (opt->verbosity > 0 && !background) {
	    if (lf_flag)
		fprintf (stderr, "\n");
	    lf_flag = fprintf (stderr, "Loading %s... (%d of %d)\n",
		argv[optind], optind - first + 1, argc - first);
	}

	if (background) {
	    verb = xmp_verbosity_level(ctx, 0);
	    t = xmp_load_module(ctx, argv[optind]);
	    xmp_verbosity_level(ctx, verb);
	} else {
	    t = xmp_load_module(ctx, argv[optind]);
	}

	if (t < 0) {
	    switch (t) {
	    case -1:
		fprintf (stderr, "%s: %s: unrecognized file format\n",
		    argv[0], argv[optind]);
		getprevious = skipprev;
		continue;
	    case -2:
		fprintf (stderr, "%s: %s: possibly corrupted file\n",
		    argv[0], argv[optind]);
		getprevious = skipprev;
		continue;
	    case -3: {
		char *line;
		int lsize = strlen(*argv) + strlen(argv[optind]) + 10;
		line = malloc(lsize);
		snprintf(line, lsize, "%s: %s", *argv, argv[optind]);
		perror (line);
		getprevious = skipprev;
		continue; }
	    }
	}

	rows = tot_nch = max_nch = getprevious = skipprev = 0;
        num_mod++;

	xmp_get_module_info(ctx, &mi);

	if (loadonly)
	    goto skip_play;

	t = xmp_play_module(ctx);

	xmp_release_module(ctx);

	if (opt->verbosity && !background) {
	    fprintf (stderr,
"\rElapsed time   : %dmin%02ds %s                                              \n",
	    t / 60, t % 60, skip ? "(SKIPPED)" : " ");

	    fprintf (stderr, "Channels used  : %d/%d", max_nch, mi.chn);
	    if (max_nch)
		fprintf (stderr, ", avg %.2f (%.1f%%)\n",
			tot_nch / rows, 100.0 * tot_nch / rows / mi.chn);
	    else
		fprintf (stderr, "\n");
	}

skip_play:

	if (skip == -1) {
	    optind -= optind > first ? 2 : 1;
	    skipprev = 1;
	}

	if (skip == -2)
	    break;

	skip = 0;
    }

    time (&t1);

    if (!loadonly && opt->verbosity && !background && num_mod > 1) {
	t = difftime (t1, t0);
	fprintf (stderr, "\n\t%d modules played, total time %dh%02dmin%02ds\n",
	     num_mod, t / 3600, (t % 3600) / 60, t % 60); 
    }

    xmp_close_audio(ctx);
    xmp_free_context(ctx);
    reset_tty();

    return 0;
}
