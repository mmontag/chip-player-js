/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: rad_load.c,v 1.4 2007-08-10 13:58:25 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct rad_instrument {
    uint8 num;			/* Number of instruments that follows */
    uint8 reg[11];		/* Adlib registers */
};

struct rad_file_header {
    uint8 magic[16];		/* 'RAD by REALiTY!!' */
    uint8 version;		/* Version in BCD */
    uint8 flags;		/* bit 7=descr,6=slow-time,4..0=tempo */
};


int rad_load (FILE * f)
{
    int i, j;
    struct rad_file_header rfh;
    struct xxm_event *event;
    uint8 sid[11];
    uint16 ppat[32];
    uint8 b, r, c;

    LOAD_INIT ();

    fread(&rfh.magic, 16, 1, f);
    rfh.version = read8(f);
    rfh.flags = read8(f);
    
    if (strncmp ((char *) rfh.magic, "RAD by REALiTY!!", 16))
	return -1;

    xxh->chn = 9;
    xxh->bpm = 125;
    xxh->tpo = rfh.flags & 0x1f;
    /* FIXME: tempo setting in RAD modules */
    if (xxh->tpo <= 2)
	xxh->tpo = 6;
    xxh->smp = 0;

    sprintf (xmp_ctl->type, "RAD %d.%d",
	MSN (rfh.version), LSN (rfh.version));
    sprintf (tracker_name, "Reality Adlib Tracker");
    MODULE_INFO ();

    /* Read description */
    if (rfh.flags & 0x80) {
	if (V (1))
	    report ("|");
	for (j = 0; fread(&b, 1, 1, f) && b;)
	    if (V (1)) {
		if (!j && (b == 1)) {
		    report ("\n|");
		    j = 1;
		} else if (b < 0x20)
		    for (i = 0; !j && (i < b); i++)
			report (" ");
		else if (b < 0x80)
		    j = 0, report ("%c", b);
		else
		    j = 0, report (".");
	    }
	if (V (1))
	    report ("\n");
    }

    if (V (1)) {
	report (
"               Modulator                       Carrier             Common\n"
"     Char Fr LS OL At De Su Re WS   Char Fr LS OL At De Su Re WS   Fbk Alg\n");
    }

    /* Read instruments */
    for (; fread (&b, 1, 1, f) && b;) {
	if (!b)
	    break;
	xxh->ins = b;
	fread(sid, 1, 11, f);
	xmp_cvt_hsc2sbi((char *)sid);
	if (V (1)) {
	    report ("[%2X] ", b - 1);

	    report ("%c%c%c%c %2d ",
		sid[0] & 0x80 ? 'a' : '-', sid[0] & 0x40 ? 'v' : '-',
		sid[0] & 0x20 ? 's' : '-', sid[0] & 0x10 ? 'e' : '-',
		sid[0] & 0x0f);
	    report ("%2d %2d ", sid[2] >> 6, sid[2] & 0x3f);
	    report ("%2d %2d ", sid[4] >> 4, sid[4] & 0x0f);
	    report ("%2d %2d ", sid[6] >> 4, sid[6] & 0x0f);
	    report ("%2d   ", sid[8]);

	    report ("%c%c%c%c %2d ",
		sid[1] & 0x80 ? 'a' : '-', sid[1] & 0x40 ? 'v' : '-',
		sid[1] & 0x20 ? 's' : '-', sid[1] & 0x10 ? 'e' : '-',
		sid[1] & 0x0f);
	    report ("%2d %2d ", sid[3] >> 6, sid[3] & 0x3f);
	    report ("%2d %2d ", sid[5] >> 4, sid[5] & 0x0f);
	    report ("%2d %2d ", sid[7] >> 4, sid[7] & 0x0f);
	    report ("%2d   ", sid[9]);

	    report ("%2d  %2d\n", sid[10] >> 1, sid[10] & 0x01);
	}
	xmp_drv_loadpatch (f, b - 1, 0, 0, NULL, (char *)sid);
    }

    INSTRUMENT_INIT ();
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = 1;
	xxi[i][0].vol = 0x40;
	xxi[i][0].pan = 0x80;
	xxi[i][0].xpo = -1;
	xxi[i][0].sid = i;
    }

    /* Read orders */
    fread (&b, 1, 1, f);
    for (i = 0, j = xxh->len = b; j--; i++) {
	fread (&xxo[i], 1, 1, f);
	xxo[i] &= 0x7f;		/* FIXME: jump line */
    }

    /* Read pattern pointers */
    for (xxh->pat = i = 0; i < 32; i++) {
	ppat[i] = read16l(f);
	if (ppat[i])
	    xxh->pat++;
    }
    xxh->trk = xxh->pat * xxh->chn;

    if (V (0)) {
	report ("Module length  : %d patterns\n", xxh->len);
	report ("Instruments    : %d\n", xxh->ins);
	report ("Stored patterns: %d ", xxh->pat);
    }
    PATTERN_INIT ();

    /* Read and convert patterns */
    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	fseek (f, ppat[i], SEEK_SET);
	do {
	    fread (&r, 1, 1, f);	/* Row number */
	    if ((r & 0x7f) >= 64)
		report ("** Whoops! row = %d\n", r);
	    do {
		fread (&c, 1, 1, f);	/* Channel number */
		if ((c & 0x7f) >= xxh->chn)
		    report ("** Whoops! channel = %d\n", c);
		event = &EVENT (i, c & 0x7f, r & 0x7f);
		fread (&b, 1, 1, f);	/* Note + Octave + Instrument */
		event->ins = (b & 0x80) >> 3;
		event->note = 13 + (b & 0x0f) + 12 * ((b & 0x70) >> 4);
		fread (&b, 1, 1, f);	/* Instrument + Effect */
		event->ins |= MSN (b);
		if ((event->fxt = LSN (b))) {
		    fread (&b, 1, 1, f);	/* Effect parameter */
		    event->fxp = b;
		    /* FIXME: tempo setting in RAD modules */
		    if (event->fxt == 0x0f && event->fxp <= 2)
			event->fxp = 6;
		}
	    } while (~c & 0x80);
	} while (~r & 0x80);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++) {
	xxc[i].pan = 0x80;
	xxc[i].flg = XXM_CHANNEL_FM;
    }

    return 0;
}
