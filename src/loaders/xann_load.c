/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for XANN Packer modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Modules sent by Sylvain
 * Chipaux. Format created by XANN/The Silents.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct xann_instrument {
    uint8 finetune;
    uint8 volume;
    uint32 loop_start;
    uint16 loop_size;
    uint32 ptr;
    uint16 size;
    uint16 unknown;
};

struct xann_header {
    uint32 ptr[128];
    uint8 unknown[6];
    struct xann_instrument ins[31];
    uint8 unknown2[0x46];
};


/* XANN to Protracker/XM effect translation table */
static int fx[] = {
    0xff, 0x00, 0x01, 0x02,
    0x03, 0x03, 0x04, 0x04,
    0x05, 0x05, 0x06, 0x06,
    0x07, 0x07, 0x09, 0x0a,
    0x0a, 0x0b, 0x0c, 0x0d,
    0x0f, 0x0f, 0xff, 0xe1,
    0xe2, 0xe3, 0xe3, 0xe4,
    0xe5, 0xe6, 0xe6, 0xe7,
    0xe9, 0xe9, 0xea, 0xeb,
    0xec, 0xed, 0xee, 0xef
};


int xann_load (FILE *f)
{
    int i, j, k;
    int smp_size;
    struct xxm_event *event;
    struct xann_header xh;
    uint8 xe[4];

    LOAD_INIT ();

    for (i = 0; i < 128; i++) {
	xh.ptr[i] = read32b(f);
    }
    fread(&xh.unknown, 6, 1, f);
    for (i = 0; i < 31; i++) {
	xh.ins[i].finetune = read8(f);
	xh.ins[i].volume = read8(f);
	xh.ins[i].loop_start = read32b(f);
	xh.ins[i].loop_size = read16b(f);
	xh.ins[i].ptr = read32b(f);
	xh.ins[i].size = read16b(f);
	xh.ins[i].unknown = read16b(f);
    }
    fread(&xh.unknown2, 0x46, 1, f);

    for (xxh->pat = xxh->len = i = 0; i < 128; i++) {
	if (!xh.ptr[i])
	    break;
	xxo[i] = (xh.ptr[i] - 0x043c) >> 10;
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    }
    xxh->len = i;
    xxh->pat++;
    xxh->trk = xxh->pat * xxh->chn;

    for (smp_size = i = 0; i < xxh->ins; i++)
	smp_size += 2 * xh.ins[i].size;

    if (1084 /*sizeof(struct xann_header)*/ + 0x400 * xxh->pat + smp_size !=
	xmp_ctl->size)
	return -1;

    sprintf (xmp_ctl->type, "XANN Packer");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * xh.ins[i].size;
	xxs[i].lps = xh.ins[i].loop_start - xh.ins[i].ptr;
	xxs[i].lpe = xxs[i].lps + 2 * xh.ins[i].loop_size;
	xxs[i].flg = xh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8) xh.ins[i].finetune << 4;
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
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);
		fread (xe, 4, 1, f);

		/* XANN event format:
		 *
		 * 0000 0000  0000 0000  0000 0000  0000 0000
		 * \     /    \      /   \     /    \       /
		 *   inst       note      effect      fxval
		 */

		if ((event->note = xe[1] >>= 1) != 0)
		    event->note += 36;
		event->ins = xe[0] >> 3;
		event->fxt = fx[xe[2] >>= 2];
		event->fxp = xe[3];

		if (event->fxt == 0xff) {
		    event->fxt = event->fxp = 0;
		} else if (xe[2] == 0x0f) {
		    event->fxp <<= 4; 
		} else if (event->fxt > 0xe0) {
		    event->fxp = (LSN (event->fxt) << 4) | LSN (xe[3]);
		    event->fxt = 0x0e;
		}

		disable_continue_fx (event);
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
