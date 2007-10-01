/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: ice_load.c,v 1.4 2007-10-01 22:03:19 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Soundtracker 2.6/Ice Tracker modules */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_MTN_	MAGIC4('M','T','N',0)
#define MAGIC_IT10	MAGIC4('I','T','1','0')


struct ice_ins {
    char name[22];		/* Instrument name */
    uint16 len;			/* Sample length / 2 */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in file */
    uint16 loop_size;		/* Loop size / 2 */
};

struct ice_header {
    char title[20];
    struct ice_ins ins[31];	/* Instruments */
    uint8 len;			/* Size of the pattern list */
    uint8 trk;			/* Number of tracks */
    uint8 ord[128][4];
    uint32 magic;		/* 'MTN\0', 'IT10' */
};


int ice_load (FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct ice_header ih;
    uint8 ev[4];

    LOAD_INIT ();

    fread(&ih.title, 20, 1, f);
    for (i = 0; i < 31; i++) {
	fread(&ih.ins[i].name, 22, 1, f);
	ih.ins[i].len = read16b(f);
	ih.ins[i].finetune = read8(f);
	ih.ins[i].volume = read8(f);
	ih.ins[i].loop_start = read16b(f);
	ih.ins[i].loop_size = read16b(f);
    }
    ih.len = read8(f);
    ih.trk = read8(f);
    fread(&ih.ord, 128 * 4, 1, f);
    ih.magic = read32b(f);

    if (ih.magic == MAGIC_IT10)
        strcpy(xmp_ctl->type, "IT10 (Ice Tracker)");
    else if (ih.magic == MAGIC_MTN_)
        strcpy(xmp_ctl->type, "MTN (Soundtracker 2.6)");
    else
	return -1;

    xxh->ins = 31;
    xxh->smp = xxh->ins;
    xxh->pat = ih.len;
    xxh->len = ih.len;
    xxh->trk = ih.trk;

    strncpy (xmp_ctl->name, (char *) ih.title, 20);
    MODULE_INFO ();

    INSTRUMENT_INIT ();

    reportv(1, "     Instrument name        Len  LBeg LEnd L Vl Ft\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = 2 * ih.ins[i].len);
	xxs[i].lps = 2 * ih.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * ih.ins[i].loop_size;
	xxs[i].flg = ih.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = ih.ins[i].volume;
	xxi[i][0].fin = ((int16)ih.ins[i].finetune / 0x48) << 4;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	if (V (1) && xxs[i].len > 2)
	    report ("[%2X] %-22.22s %04x %04x %04x %c %02x %+01x\n",
		i, ih.ins[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol,
		xxi[i][0].fin >> 4);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	for (j = 0; j < xxh->chn; j++) {
	    xxp[i]->info[j].index =  ih.ord[i][j];
	}
	xxo[i] = i;

	reportv(0, ".");
    }

    reportv(0, "\nStored tracks  : %d ", xxh->trk);

    for (i = 0; i < xxh->trk; i++) {
	xxt[i] = calloc (sizeof (struct xxm_track) + sizeof
		(struct xxm_event) * 64, 1);
	xxt[i]->rows = 64;
	for (j = 0; j < xxt[i]->rows; j++) {
		event = &xxt[i]->event[j];
		fread (ev, 1, 4, f);
		cvt_pt_event (event, ev);
	}

	if (V (0) && !(i % xxh->chn))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Read samples */

    reportv(0, "\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 4)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	reportv(0, ".");
    }
    reportv(0, "\n");

    return 0;
}

