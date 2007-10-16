/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for ProRunner 1.0 modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Modules sent by Bert
 * Jahn.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "mod.h"


int pru1_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, k;
    struct xxm_event *event;
    struct mod_header ph;
    uint8 pe[4];

    LOAD_INIT ();

    fread (&ph, 1, sizeof (struct mod_header), f);

    if (ph.magic[0] != 'S' || ph.magic[1] != 'N' || ph.magic[2] != 'T' ||
	ph.magic[3] != '.')
	return -1;

    strncpy (m->name, ph.name, 20);
    sprintf (m->type, "ProRunner v1");

    MODULE_INFO ();

    m->xxh->len = ph.len;

    for (m->xxh->pat = i = 0; i < m->xxh->len; i++) {
	m->xxo[i] = ph.order[i];
	if (m->xxo[i] > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    }
    m->xxh->pat++;

    m->xxh->trk = m->xxh->pat * m->xxh->chn;

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (ph.ins[i].size);
	B_ENDIAN16 (ph.ins[i].loop_start);
	B_ENDIAN16 (ph.ins[i].loop_size);
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	m->xxs[i].len = 2 * ph.ins[i].size;
	m->xxs[i].lps = 2 * ph.ins[i].loop_start;
	m->xxs[i].lpe = m->xxs[i].lps + 2 * ph.ins[i].loop_size;
	m->xxs[i].flg = ph.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	m->xxi[i][0].fin = (int8) ph.ins[i].finetune << 4;
	m->xxi[i][0].vol = ph.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;
	strncpy (m->xxih[i].name, ph.ins[i].name, 22);

	if (V (1) && (strlen (m->xxih[i].name) || m->xxs[i].len > 2)) {
	    report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
		i, m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps,
		m->xxs[i].lpe, ph.ins[i].loop_size > 1 ? 'L' : ' ',
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
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);
		fread (pe, 4, 1, f);

		/* event format:
		 *
		 * 0000 0000  0000 0000  0000 0000  0000 0000
		 * \       /  \       /       \  /  \       /
		 *   inst       note           fx     fxval
		 */

		if ((event->note = pe[1]) != 0)
		    event->note += 36;
		event->ins = pe[0];
		event->fxt = LSN (pe[2]);
		event->fxp = pe[3];

		disable_continue_fx (event);
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
	xmp_drv_loadpatch (f, m->xxi[i][0].sid, m->c4rate, 0,
	    &m->xxs[m->xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    return 0;
}
