/* A simple frontend for xmp */

#include <stdio.h>
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
	    printf ("\rOrder %02X/%02X - Pattern %02X/%02X - Row      ",
		pos, mi.len, pat, mi.pat - 1);
	    oldpos = pos;
	}
	printf ("\b\b\b\b\b%02X/%02X", (int)(msg & 0xff), (int)(msg >> 8));
	break;
    }
}


int main (int argc, char **argv)
{
    int i, t;

    setbuf (stdout, NULL);
    xmp_init (argc, argv, &opt);
    opt.verbose = 0;
    if (xmp_open_audio (&opt) < 0) {
	fprintf (stderr, "%s: can't open audio device", argv[0]);
	return -1;
    }

    xmp_register_event_callback (process_echoback);

    for (i = 1; i < argc; i++) {
	if (xmp_load_module (argv[i]) < 0) {
	    fprintf (stderr, "%s: error loading %s\n", argv[0], argv[i]);
	    continue;
	}
	xmp_get_module_info (&mi);
	printf ("%s (%s)\n", mi.name, mi.type);
	t = xmp_play_module ();
	xmp_release_module();
	printf ("\n");
    }
    xmp_close_audio ();

    return 0;
}
