/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Heatseeker modules based on the format description
 * written by Sylvain Chipaux. Tested with the Party Time module
 * sent by Stuart Caie.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct crb_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct crb_header {
    struct crb_instrument ins[31];
    uint8 len;
    uint8 rst;
    uint8 ord[128];
} PACKED;


int crb_load (FILE *f)
{
    int i, j, k;
    int smp_size;
    struct xxm_event *event;
    struct crb_header xh;
    uint8 xe[4];

    LOAD_INIT ();

    fread (&xh, 1, sizeof (struct crb_header), f);

    if (xh.rst != 0x7f)
	return -1;

    memcpy (xxo, xh.ord, 128);

    for (i = 0; i < 128; i++) {
	if (xxo[i] > 0x7f)
		return -1;
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }
    xxh->pat++;

    xxh->len = xh.len;
    if (xxh->len > 0x7f)
	return -1;

    xxh->trk = xxh->pat * xxh->chn;

    for (smp_size = i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (xh.ins[i].size);
	B_ENDIAN16 (xh.ins[i].loop_start);
	B_ENDIAN16 (xh.ins[i].loop_size);
	smp_size += 2 * xh.ins[i].size;
    }

    if (sizeof (struct crb_header) + smp_size > xmp_ctl->size)
	return -1;

    if (sizeof (struct crb_header) + 0x400 * xxh->pat + smp_size <
	xmp_ctl->size)
	return -1;

    sprintf (xmp_ctl->type, "Heatseeker");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * xh.ins[i].size;
	xxs[i].lps = 2 * xh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * xh.ins[i].loop_size;
	xxs[i].flg = xh.ins[i].loop_size > 2 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)xh.ins[i].finetune << 4;
	xxi[i][0].vol = xh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	if (V (1) && xxs[i].len > 2) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, xh.ins[i].loop_size > 1 ? 'L' : ' ',
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
	for (k = 0; k < 4; k++) {
	    for (j = 0; j < 64; j++) {
		event = &EVENT (i, k, j);
		fread (xe, 4, 1, f);

		switch (xe[0] >> 6) {
		case 0:
		    event->note = period_to_note (((int)LSN(xe[0])<<8) | xe[1]);
		    event->ins = (xe[0] & 0xf0) | MSN(xe[2]);
		    event->fxt = LSN(xe[2]);
		    event->fxp = xe[3];
		    disable_continue_fx (event);
		    break;
		case 2:
		    j += xe[3];
		    break;
		case 3: {
		    int t = ((int)xe[2] << 6) | (xe[3] >> 2);
		    for (j = 0; j < 64; j++)
			memcpy (&EVENT (i, k, j), &EVENT (t >> 2, t % 4, j), 4);
		    }
		    break;
		}
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
