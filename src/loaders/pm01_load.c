/* Extended Module Player - pm01_load.c
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Promizer 0.1 moduless based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by Frank
 * Hulsmann (MC68000/Masque).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"

struct pm01_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct pm01_header {
    struct pm01_instrument ins[31];
    uint16 len;			/* Size of pattern pointers */
    uint32 pptr[128];		/* Pattern pointers */
    uint32 patt_size;		/* Size of the pattern data */
} PACKED;


int pm01_load (FILE *f)
{
    int i, j, k, l;
    struct xxm_event *event;
    struct pm01_header ph;
    int smp_size, pptr[128];
    uint8 pe[4];

    LOAD_INIT ();

    fread (&ph, 1, sizeof (struct pm01_header), f);
    B_ENDIAN32 (ph.patt_size);
    B_ENDIAN16 (ph.len);

    xxh->len = ph.len >> 2;

    if (xxh->len > 128)
	return -1;

    for (i = 0; i < xxh->len; i++)
	B_ENDIAN32 (ph.pptr[i]);

    for (smp_size = i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (ph.ins[i].size);
	B_ENDIAN16 (ph.ins[i].loop_start);
	B_ENDIAN16 (ph.ins[i].loop_size);

	if (ph.ins[i].size > 0x8000 ||
	    ph.ins[i].loop_start > ph.ins[i].size ||
	    (ph.ins[i].loop_start + ph.ins[i].loop_size) > (ph.ins[i].size + 1))
	    return -1;

	smp_size += ph.ins[i].size * 2;
    }

    if (sizeof (struct pm01_header) + ph.patt_size + smp_size != xmp_ctl->size)
	return -1;

    sprintf (xmp_ctl->type, "Promizer 0.1");

    MODULE_INFO ();

    /*
     * Find number of different pattern pointers == number of stored patterns
     */
    pptr[0] = ph.pptr[0];
    for (xxh->pat = i = 0; i < xxh->len; i++) {
	for (k = j = 0; j <= xxh->pat; j++) {
	    if (ph.pptr[i] == pptr[j]) {
		k = 1;
		break;
	    }
	}
	if (!k) {
	    pptr[++xxh->pat] = ph.pptr[i];
	}
    }

    /*
     * Build a pattern pointer table...
     */
    for (l = i = 0; i < xxh->pat; i++) {
	for (k = 0x7fffffff, j = 0; j < xxh->len; j++) {
	    if (ph.pptr[j] < k && ph.pptr[j] > l) {
		k = ph.pptr[j];
	    }
	}
	l = pptr[i] = k;
    }

    /*
     * ...and the order table
     */
    for (i = 0; i < xxh->len; i++) {
	for (j = 0; j < xxh->pat; j++) {
	    if (ph.pptr[i] == pptr[j])
		xxo[i] = j;
	}
    }

    xxh->trk = xxh->pat * xxh->chn;

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
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
	if (xxi[i][0].fin > 48)
	    xxi[i][0].xpo = -1;
	if (xxi[i][0].fin < -48)
	    xxi[i][0].xpo = 1;

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
		fread (pe, 1, 4, f);
		cvt_pt_event (event, pe);
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
