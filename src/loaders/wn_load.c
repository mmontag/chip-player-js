/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: wn_load.c,v 1.2 2007-09-30 00:08:18 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct wn_ins {
    uint8 name[22];		/* Instrument name */
    uint16 len;			/* Sample length in words */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in bytes */
    uint16 loop_length;		/* Sample loop length in words */
};

struct wn_header {
    uint8 name[20];		/* Title */
    struct wn_ins ins[31];	/* Instruments */
    uint8 len;			/* Song length */
    uint8 rst;			/* 0x7f */
    uint8 order[128];		/* Order list */
    uint8 magic[3];		/* 'W', 'N', 0x00 */
    uint8 pat;			/* Number of patterns */
};


int wn_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct wn_header wn;
    uint8 ev[4];

    LOAD_INIT ();

    fread (&wn, 1, sizeof (wn), f);
    if (wn.magic[0] != 'W' || wn.magic[1] != 'N' || wn.magic[2] != 0x00)
	return -1;

    xxh->len = wn.len;
    xxh->pat = wn.pat;
    xxh->trk = xxh->chn * xxh->pat;

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (wn.ins[i].len);
	B_ENDIAN16 (wn.ins[i].loop_start);
	B_ENDIAN16 (wn.ins[i].loop_length);
    }

    memcpy (xxo, wn.order, xxh->len);

    strncpy (xmp_ctl->name, wn.name, 20);
    strcpy (xmp_ctl->type, "Wanton Packer");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = 2 * wn.ins[i].len);
	xxs[i].lps = 2 * wn.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * wn.ins[i].loop_length;
	xxs[i].flg = wn.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = wn.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, wn.ins[i].name, 22);
	if ((V (1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
	    report ("[%2X] %-22.22s %04x %04x %04x %c %02x\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (ev, 1, 4, f);

	    event->note = ev[0] >> 1;
	    if (event->note)
		event->note += 36;
	    event->ins = ev[1];
	    event->fxt = LSN (ev[2]);
	    event->fxp = ev[3];
	}
	if (V (0))
	    report (".");
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
