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

#include "period.h"
#include "load.h"


struct pm_ins {
    uint8 name[22];		/* Instrument name */
    uint16 len;			/* Sample length in words */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in bytes */
    uint16 loop_length;		/* Sample loop length in words */
} PACKED;

struct pm_header {
    uint8 title[20];
    struct pm_ins ins[31];	/* Instruments */
    uint8 len;			/* Song length */
    uint8 rst;			/* 0xff */
    uint8 order[128];		/* Order list */
    uint8 magic[4];		/* '!PM!' */
} PACKED;


int pm_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct pm_header pm;
    uint8 ev[4];

    LOAD_INIT ();

    fread (&pm, 1, sizeof (pm), f);
    if (strncmp ((char *) pm.magic, "!PM!", 4))
	return -1;

    xxh->len = pm.len;

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (pm.ins[i].len);
	B_ENDIAN16 (pm.ins[i].loop_start);
	B_ENDIAN16 (pm.ins[i].loop_length);
    }

    memcpy (xxo, pm.order, xxh->len);
    for (xxh->pat = i = 0; i < xxh->len; i++)
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    xxh->pat++;

    xxh->trk = xxh->chn * xxh->pat;

    strcpy (xmp_ctl->type, "Power Music");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = 2 * pm.ins[i].len);
	xxs[i].lps = 2 * pm.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * pm.ins[i].loop_length;
	xxs[i].flg = pm.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = pm.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, pm.ins[i].name, 22);
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
	    cvt_pt_event (event, ev);
	}
	if (V (0))
	    report (".");
    }

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);

    for (i = 0; i < xxh->ins; i++) {
	if (xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, i, xmp_ctl->c4rate, XMP_SMP_DIFF, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    xxh->flg |= XXM_FLG_MODRNG;

    return 0;
}
