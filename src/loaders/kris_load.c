/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for ChipTracker modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by Kris/Anarchy.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct kris_instrument {
    uint8 name[22];		/* Sample name or 0x01 for no sample */
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct kris_header {
    uint8 title[22];
    struct kris_instrument ins[31];
    uint8 magic[4];		/* 'KRIS' */
    uint8 len;
    uint8 pad;			/* Set to 0x7f for OLD NoiseTracker players */
    uint16 order[512];		/* In track address format */
    uint8 unknown[2];		/* 00-00 ? */
} PACKED;


int kris_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j;
    struct xxm_event *event;
    struct kris_header kh;
    uint8 kris_event[3];

    LOAD_INIT ();

    fread (&kh, 1, sizeof (struct kris_header), f);

    if (strncmp ((char *)kh.magic, "KRIS", 4))
	return -1;

    m->xxh->pat = m->xxh->len = kh.len;

    strncpy (m->name, (char *) kh.title, 20);
    sprintf (m->type, "ChipTracker");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (kh.ins[i].size);
	B_ENDIAN16 (kh.ins[i].loop_start);
	B_ENDIAN16 (kh.ins[i].loop_size);
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	/*
	 * Note: the sample size and loop size are given in 16-bit words,
	 * while the loop start is given in samples.
	 */
	m->xxs[i].len = 2 * kh.ins[i].size;
	m->xxs[i].lps = kh.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * kh.ins[i].loop_size;
	m->xxs[i].flg = kh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8) kh.ins[i].finetune << 4;
	m->xxi[i][0].vol = kh.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;
	strncpy (m->xxih[i].name, kh.ins[i].name, 20);
	str_adj (m->xxih[i].name);

	if (V (1) &&
		(strlen ((char *) m->xxih[i].name) || (m->xxs[i].len > 2))) {
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x %+d\n",
		i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
		m->xxs[i].lpe, kh.ins[i].loop_size > 1 ? 'L' : ' ',
		m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    /* Load and convert patterns */

    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    xxp = calloc (sizeof (struct xxm_pattern *), m->xxh->pat + 1);
    for (m->xxh->trk = i = 0; i < m->xxh->len; i++) {
	m->xxo[i] = i;
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	for (j = 0; j < 4; j++) {
	    B_ENDIAN16 (kh.order[i * 4 + j]);
	    m->xxp[i]->info[j].index = kh.order[i * 4 + j] >> 8;
	    if (m->xxp[i]->info[j].index > m->xxh->trk)
		m->xxh->trk = m->xxp[i]->info[j].index;
	}
	if (V (0))
	    report (".");
    }
    m->xxh->trk++;

    /* Load and convert tracks */

    if (V (0))
	report ("\nStored tracks  : %d ", m->xxh->trk);

    xxt = calloc (sizeof (struct xxm_track *), m->xxh->trk);
    for (i = 0; i < m->xxh->trk; i++) {
	m->xxt[i] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * 64, 1);
	m->xxt[i]->rows = 64;

	for (j = 0; j < 64; j++) {
	    event = &m->xxt[i]->event[j];
	    fread (kris_event, 1, 4, f);

	    /* Event format:
	     *
	     * 0000 0000  0000 0000  0000 0000  0000 0000
	     * \       /  \       /  \  / \  /  \       /
	     *   note        ins    unused fx    parameter
	     *
	     * 0xa8 is a blank note.
	     */
	    event->note = kris_event[0];
	    if (event->note != 0xa8)
		event->note = (event->note >> 1) + 1;
	    else
		event->note = 0;
	    event->ins = kris_event[1];
	    event->fxt = LSN (kris_event[2]);
	    event->fxp = kris_event[3];

	    disable_continue_fx (event);
	}
	if (V (0) && !(i % 4))
	    report (".");
    }

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    if (V (0))
	report ("\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->smp; i++) {
	if (!m->xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, m->xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &m->xxs[m->xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
