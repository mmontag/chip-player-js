/* A simple frontend for xmp */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "xmp.h"

static struct xmp_module_info mi;
static struct xmp_control opt;


static void process_echoback (unsigned long i)
{
    unsigned long msg = i >> 4;
    static int oldpos = -1;
    static int pos, pat;

    switch (i & 0xf) {
    case XMP_ECHO_ORD:
	pos = msg & 0xff;
	pat = msg >> 8;
	break;
    case XMP_ECHO_ROW:
	if (!(msg & 0xff) || pos != oldpos) {
	    fprintf (stderr, "\rOrder %02X/%02X - Pattern %02X/%02X - Row      ",
		pos, mi.len, pat, mi.pat - 1);
	    oldpos = pos;
	}
	fprintf (stderr, "\b\b\b\b\b%02X/%02X", (int)(msg & 0xff), (int)(msg >> 8));
	break;
    }
}


int main (int argc, char **argv)
{
    int t;
    FILE *p;
    char *cmd, *enc = "lame -r -s44.1 -x -h -";

    xmp_init (argc, argv, &opt);
    opt.verbose = 0;
    opt.drv_id = "file";
    opt.outfile = "-";
    opt.freq = 44100;
    opt.resol = 16;
    opt.outfmt = 0;

    xmp_register_event_callback (process_echoback);

    cmd = malloc (strlen (enc) + strlen (argv[1]) + 10);
    sprintf (cmd, "%s %s.mp3", enc, argv[1]);
    p = popen (cmd, "w");
    free (cmd);

    close (STDOUT_FILENO);
    dup2 (fileno (p), STDOUT_FILENO);

    if (xmp_open_audio (&opt) < 0) {
	fprintf (stderr, "%s: can't open audio device", argv[0]);
	return -1;
    }

    if (xmp_load_module (argv[1]) < 0) {
        fprintf (stderr, "%s: error loading %s\n", argv[0], argv[1]);
	return -1;
    }
    xmp_get_module_info (&mi);
    fprintf (stderr, "%s (%s)\n", mi.name, mi.type);
    t = xmp_play_module ();
    fprintf (stderr, "\n");
    xmp_close_audio ();
    pclose (p);

    return 0;
}
