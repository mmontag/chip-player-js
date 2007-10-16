/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Module Protector loader based on the desciption written by Sylvain
 * Chipaux. Tested with modules converted with NoiseConverter.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct mp_ins {
    uint16 len;			/* Sample length in words */
    uint8 finetune;		/* Finetune */
    uint8 volume;		/* Volume (0-63) */
    uint16 loop_start;		/* Sample loop start in bytes */
    uint16 loop_length;		/* Sample loop length in words */
} PACKED;

struct mp_header {
    struct mp_ins ins[31];	/* Instruments */
    uint8 len;			/* Song length */
    uint8 rst;			/* 0x7f */
    uint8 order[128];		/* Order list */
} PACKED;


int mp_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct mp_header mp;
    uint8 ev[4];
    int smp_size;
    uint32 x;

    LOAD_INIT ();

    fread (&mp, 1, sizeof (mp), f);

    m->xxh->ins = 31;
    m->xxh->smp = m->xxh->ins;
    m->xxh->len = mp.len;

    memcpy (m->xxo, mp.order, m->xxh->len);

    for (m->xxh->pat = i = 0; i < m->xxh->len; i++)
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    m->xxh->pat++;

    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    for (smp_size = i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (mp.ins[i].len);
	B_ENDIAN16 (mp.ins[i].loop_start);
	B_ENDIAN16 (mp.ins[i].loop_length);
	smp_size += 2 * mp.ins[i].len;
    }

    fread (&x, 4, 1, f);
    if (x)
	fseek (f, -4, SEEK_CUR);
    else
	smp_size += 4;

    if (m->size != 378 + m->xxh->pat * 0x400 + smp_size)
	return -1;

    strcpy (m->type, "Module Protector");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Len  LBeg LEnd L Vl Ft\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxih[i].nsm = !!(m->xxs[i].len = 2 * mp.ins[i].len);
	m->xxs[i].lps = 2 * mp.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * mp.ins[i].loop_length;
	m->xxs[i].flg = mp.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].vol = mp.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	if (V (1) && m->xxs[i].len > 2)
	    report ("[%2X] %04x %04x %04x %c %02x %+01x\n",
		i, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', m->xxi[i][0].vol,
		m->xxi[i][0].fin >> 4);
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
	xmp_drv_loadpatch (f, i, m->c4rate, 0, &m->xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
