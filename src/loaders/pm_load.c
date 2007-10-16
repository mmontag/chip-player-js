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

    m->xxh->len = pm.len;

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (pm.ins[i].len);
	B_ENDIAN16 (pm.ins[i].loop_start);
	B_ENDIAN16 (pm.ins[i].loop_length);
    }

    memcpy (m->xxo, pm.order, m->xxh->len);
    for (m->xxh->pat = i = 0; i < m->xxh->len; i++)
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    m->xxh->pat++;

    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    strcpy (m->type, "Power Music");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxih[i].nsm = !!(m->xxs[i].len = 2 * pm.ins[i].len);
	m->xxs[i].lps = 2 * pm.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * pm.ins[i].loop_length;
	m->xxs[i].flg = pm.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].vol = pm.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	strncpy ((char *) m->xxih[i].name, pm.ins[i].name, 22);
	if ((V (1)) && (strlen ((char *) m->xxih[i].name) || (m->xxs[i].len > 2)))
	    report ("[%2X] %-22.22s %04x %04x %04x %c %02x\n",
		i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', m->xxi[i][0].vol);
    }

    PATTERN_INIT ();

    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	for (j = 0; j < 64 * m->xxh->chn; j++) {
	    event = &EVENT (i, j % m->xxh->chn, j / m->xxh->chn);
	    fread (ev, 1, 4, f);
	    cvt_pt_event (event, ev);
	}
	if (V (0))
	    report (".");
    }

    /* Read samples */

    if (V (0))
	report ("\nStored samples : %d ", m->xxh->smp);

    for (i = 0; i < m->xxh->ins; i++) {
	if (m->xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, i, m->c4rate, XMP_SMP_DIFF, &m->xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    m->xxh->flg |= XXM_FLG_MODRNG;

    return 0;
}
