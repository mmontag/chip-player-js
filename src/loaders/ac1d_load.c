/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for AC1D Packer modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by
 * Slammer/Anarchy.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct ac1d_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct ac1d_header {
    uint8 len;
    uint8 ignore;		/* Noistracker byte set to 0x7f */
    uint8 magic[2];		/* 0xAC1D */
    uint32 smp_addr;		/* Address of sample data */
    struct ac1d_instrument ins[31];
    uint32 addr[128];		/* Pattern address table */
    uint8 order[128];		/* Pattern order table */
} PACKED;


int ac1d_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, k;
    struct xxm_event *event;
    struct ac1d_header ah;
    uint8 x8, y8;

    LOAD_INIT ();

    fread (&ah, 1, sizeof (struct ac1d_header), f);

    if (ah.magic[0] != 0xac || ah.magic[1] != 0x1d)
	return -1;

    B_ENDIAN32 (ah.smp_addr);
    for (i = 0; i < 128; i++) {
	B_ENDIAN32 (ah.addr[i]);
	if (!ah.addr[i])
	    break;
    }

    m->xxh->pat = i;
    m->xxh->trk = m->xxh->pat * m->xxh->chn;
    m->xxh->len = ah.len;

    for (i = 0; i < m->xxh->len; i++)
	m->xxo[i] = ah.order[i];

    sprintf (m->type, "AC1D Packer");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (ah.ins[i].size);
	B_ENDIAN16 (ah.ins[i].loop_start);
	B_ENDIAN16 (ah.ins[i].loop_size);
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * ah.ins[i].size;
	m->xxs[i].lps = 2 * ah.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * ah.ins[i].loop_size;
	m->xxs[i].flg = ah.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8) ah.ins[i].finetune << 4;
	m->xxi[i][0].vol = ah.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;

	if (V (1) && m->xxs[i].len > 2) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, m->xxs[i].len, m->xxs[i].lps,
		m->xxs[i].lpe, ah.ins[i].loop_size > 1 ? 'L' : ' ',
		m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	if (ftell (f) & 1)		/* Patterns are 16-bit aligned */
	    fread (&x8, 1, 1, f);
	fseek (f, 12, SEEK_CUR);	/* Skip track pointers */
	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 64; k++) {
		event = &EVENT (i, j, k);
		fread (&x8, 1, 1, f);
		if (x8 & 0x80) {
		    k += (x8 & 0x7f) - 1;
		    continue;
		}

		fread (&y8, 1, 1, f);
		event->note = x8 & 0x3f;

		if (event->note == 12) {		/* ??? */
		    event->note = 37;
		} else if (event->note == 0x3f) {
		    event->note = 0;
		} else {
		    event->note += 25;
		}

		event->ins = ((x8 & 0x40) >> 2) | MSN (y8);
		if (LSN (y8) == 0x7)
		    continue;

		fread (&x8, 1, 1, f);
		event->fxt = LSN (y8);
		event->fxp = x8;

		if (!event->fxp) {
		    switch (event->fxt) {
		    case 0x05:
			event->fxt = 0x03;
			break;
		    case 0x06:
			event->fxt = 0x04;
			break;
		    case 0x01:
		    case 0x02:
		    case 0x0a:
			event->fxt = 0x00;
		    case 0x0f:
			break;
		    }
		}
	    }
	}

	if (V (0))
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
