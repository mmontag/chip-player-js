/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for ProRunner 2.0 modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Tested with modules sent
 * by Bert Jahn.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct pru2_instrument {
    uint16 size;
    int8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct pru2_header {
    uint8 magic[4];		/* ID 'SNT!' */
    uint32 smpptr;		/* Pointer to the sample data */
    struct pru2_instrument ins[31];
    uint8 len;
    uint8 restart;
    uint8 order[128];
    uint16 unused[128];
    uint16 pointer[64];
} PACKED;


int pru2_load (FILE *f)
{
    int i, j, k;
    struct xxm_event *event, old;
    struct pru2_header ph;
    uint8 x8, y8, z8;

    LOAD_INIT ();

    fread (&ph, 1, sizeof (struct pru2_header), f);

    if (ph.magic[0] != 'S' || ph.magic[1] != 'N' || ph.magic[2] != 'T' ||
	ph.magic[3] != '!')
	return -1;

    sprintf (xmp_ctl->type, "ProRunner v2");

    MODULE_INFO ();

    xxh->len = ph.len;

    for (xxh->pat = i = 0; i < xxh->len; i++) {
	xxo[i] = ph.order[i];
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }
    xxh->pat++;

    xxh->trk = xxh->pat * xxh->chn;

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (ph.ins[i].size);
	B_ENDIAN16 (ph.ins[i].loop_start);
	B_ENDIAN16 (ph.ins[i].loop_size);
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * ph.ins[i].size;
	xxs[i].lps = 2 * ph.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * ph.ins[i].loop_size;
	xxs[i].flg = ph.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8) ph.ins[i].finetune << 4;
	xxi[i][0].vol = ph.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	if (V (1) && (strlen (xxih[i].name) || xxs[i].len > 2)) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		ph.ins[i].loop_size > 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);
		fread (&x8, 1, 1, f);

	        if (x8 & 0x80) {
		    if (x8 & 0x40) {
			event->note = old.note;
			event->ins = old.ins;
			event->fxt = old.fxt;
			event->fxp = old.fxp;
		    }
		    continue;
		}

		/* event format:
		 *
		 * 0000 0000  0000 0000  0000 0000
		 *  \     /\     / \  /  \       /
		 *   note    ins    fx     fxval
		 */

		fread (&y8, 1, 1, f);
		fread (&z8, 1, 1, f);

		if ((event->note = (x8 & 0x3e) >> 1) != 0)
		    event->note += 36;
		old.note = event->note;
		old.ins = event->ins = ((y8 & 0xf0) >> 3) | (x8 & 0x01);
		event->fxt = LSN (y8);
		event->fxp = z8;

		disable_continue_fx (event);

		old.fxt = event->fxt;
		old.fxp = event->fxp;
	    }
	}

	if (V (0))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
