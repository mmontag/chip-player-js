/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct ftm_instrument {
    uint8 name[30];			/* Instrument name */
    uint8 unknown[2];
} PACKED;

struct ftm_header {
    uint8 id[4];			/* "FTMN" ID string */
    uint8 ver;				/* Version ?!? (0x03) */
    uint8 nos;				/* Number of samples (?) */
    uint8 unknown[10];
    uint8 title[32];			/* Module title */
    uint8 author[32];			/* Module author */
    uint8 unknown2[2];
} PACKED;


int ftm_load (FILE *f)
{
    int i, j, k;
    struct xxm_event *event;
    struct ftm_header fh;
    struct ftm_instrument si;
    uint8 b1, b2, b3;

    LOAD_INIT ();

    fread (&fh, 1, sizeof (struct ftm_header), f);

    if (fh.id[0] != 'F' || fh.id[1] != 'T' || fh.id[2] != 'M' || fh.id[3] != 'N')
	return -1;

    B_ENDIAN32 (fh.smpaddr);
    B_ENDIAN16 (fh.nos);
    B_ENDIAN16 (fh.len);
    B_ENDIAN16 (fh.pat);

    xxh->len = fh.len;
    xxh->pat = fh.pat;
    xxh->ins = fh.nos;
    xxh->smp = xxh->ins;
    xxh->trk = xxh->pat * xxh->chn;

    for (i = 0; i < xxh->len; i++)
	xxo[i] = fh.order[i];

    sprintf (xmp_ctl->type, "Slamtilt");

    MODULE_INFO ();

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	fseek (f, fh.pataddr[i] + 8, SEEK_SET);

	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 64; k++) {
		event = &EVENT (i, j, k);
		fread (&b1, 1, 1, f);

		if (b1 & 0x80) {
		    k += b1 & 0x7f;
		    continue;
		}

		/* STIM event format:
		 *
		 *     __ Fx __
		 *    /        \
		 *   ||        ||
		 *  0000 0000  0000 0000  0000 0000
		 *  |  |    |    |     |  |       |
		 *  |   \  /      \   /    \     /
		 *  |    smp      note      Fx Val
 		 *  |
		 *  Description bit set to 0.
		 */

		fread (&b2, 1, 1, f);
		fread (&b3, 1, 1, f);

		if ((event->note = b2 & 0x3f) != 0)
		    event->note += 35;
		event->ins = b1 & 0x1f;
		event->fxt = ((b2 >> 4) & 0x0c) | (b1 >> 5);
		event->fxp = b3;

		disable_continue_fx (event);
	    }
	}

	if (V (0))
	    report (".");
    }

    INSTRUMENT_INIT ();

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    fseek (f, fh.smpaddr + xxh->smp * 4, SEEK_SET);

    for (i = 0; i < xxh->smp; i++) {
	fread (&si, sizeof (si), 1, f);

	B_ENDIAN16 (si.size);
	B_ENDIAN16 (si.loop_start);
	B_ENDIAN16 (si.loop_size);

	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * si.size;
	xxs[i].lps = 2 * si.loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * si.loop_size;
	xxs[i].flg = si.loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)(si.finetune << 4);
	xxi[i][0].vol = si.volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	if (V (1) && xxs[i].len > 2) {
	    report ("\n[%2X] %04x %04x %04x %c V%02x %+d ",
		i, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, si.loop_size > 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
	}

	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    xxh->flg |= XXM_FLG_MODRNG;

    return 0;
}
