/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: alm_load.c,v 1.2 2006-02-13 12:50:34 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* ALM (Aley's Module) is a module format used on 8bit computers. It was
 * designed to be usable on Sam Coupe (CPU Z80 6MHz) and PC XT. The ALM file
 * format is very simple and it have no special effects, so every computer
 * can play the ALMs. [Yes, even Unix workstations with GUS]
 *
 * Technically speaking, Aley's Modules are not modules -- they don't
 * pack the sequencing information and the sound samples in a single
 * file. xmp's module loading mechanism was not designed to load samples
 * from different files so I kludged char *module into a global variable.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#ifdef __EMX__
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <unistd.h>


struct alm_file_header {
    uint8 id[7];		/* "ALEY MO" or "ALEYMOD" */
    uint8 speed;		/* Only in versions 1.1 and 1.2 */
    uint8 length;		/* Length of module */
    uint8 restart;		/* Restart position */
    uint8 order[128];		/* Pattern sequence */
};


int alm_load (FILE *f)
{
    int i, j;
    struct alm_file_header afh;
    struct xxm_event *event;
    struct stat stat;
    uint8 b;
    uint16 w;
    char *basename;
    char filename[80];
    char modulename[80];
    FILE *s;

    LOAD_INIT ();

    strcpy (modulename, xmp_ctl->filename);
    basename = strtok (modulename, ".");

    fread(&afh.id, 7, 1, f);

    if (!strncmp ((char *) afh.id, "ALEYMOD", 7))	/* Version 1.0 */
	xxh->tpo = afh.speed / 2;
    else if (strncmp (afh.id, "ALEY MO", 7))	/* Versions 1.1 and 1.2 */
	return -1;

    afh.speed = read8(f);
    afh.length = read8(f);
    afh.restart = read8(f);
    fread(&afh.order, 128, 1, f);

    xxh->len = afh.length;
    xxh->rst = afh.restart;
    memcpy (xxo, afh.order, xxh->len);

    for (xxh->pat = i = 0; i < xxh->len; i++)
	if (xxh->pat < afh.order[i])
	    xxh->pat = afh.order[i];
    xxh->pat++;

    xxh->trk = xxh->pat * xxh->chn;
    xxh->smp = xxh->ins;
    xmp_ctl->c4rate = C4_NTSC_RATE;

    sprintf (xmp_ctl->type, "Aley's Module");

    MODULE_INFO ();

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (&b, 1, 1, f);
	    if (b)
		event->note = (b == 37) ? 0x61 : b + 36;
	    fread (&b, 1, 1, f);
	    event->ins = b;
	}
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */

    if (V (0))
	report ("Loading samples: %d ", xxh->ins);

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	sprintf (filename, "%s.%d", basename, i + 1);
	s = fopen (filename, "rb");
	if (!(xxih[i].nsm = (s != NULL)))
	    continue;
	fstat (fileno (s), &stat);
	fread (&b, 1, 1, s);	/* Get first octet */
	xxs[i].len = stat.st_size - 5 * !b;

	if (!b) {		/* Instrument with header */
	    xxs[i].lps = read16l(f);
	    xxs[i].lpe = read16l(f);
	    xxs[i].flg = xxs[i].lpe > xxs[i].lps ? WAVE_LOOPING : 0;
	} else
	    fseek (s, 0, SEEK_SET);

	xxi[i][0].pan = 0x80;
	xxi[i][0].vol = 0x40;
	xxi[i][0].sid = i;

	if ((V (1)) && (strlen ((char *) xxih[i].name) ||
		(xxs[i].len > 1))) {
	    report ("\n[%2X] %-14.14s %04x %04x %04x %c V%02x ", i,
		filename, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol);
	}

	xmp_drv_loadpatch (s, xxi[i][0].sid, xmp_ctl->c4rate,
	    XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);

	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    /* ALM is LRLR, not LRRL */
    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = (i % 2) * 0xff;

    return 0;
}
