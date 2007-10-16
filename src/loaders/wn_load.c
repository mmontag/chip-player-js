/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: wn_load.c,v 1.5 2007-10-16 01:14:36 cmatsuoka Exp $
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

    m->xxh->len = wn.len;
    m->xxh->pat = wn.pat;
    m->xxh->trk = m->xxh->chn * m->xxh->pat;

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (wn.ins[i].len);
	B_ENDIAN16 (wn.ins[i].loop_start);
	B_ENDIAN16 (wn.ins[i].loop_length);
    }

    memcpy (m->xxo, wn.order, m->xxh->len);

    strncpy (m->name, wn.name, 20);
    strcpy (m->type, "Wanton Packer");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxih[i].nsm = !!(m->xxs[i].len = 2 * wn.ins[i].len);
	m->xxs[i].lps = 2 * wn.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * wn.ins[i].loop_length;
	m->xxs[i].flg = wn.ins[i].loop_length > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].vol = wn.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	strncpy ((char *) m->xxih[i].name, wn.ins[i].name, 22);
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

    m->xxh->flg |= XXM_FLG_MODRNG;

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
