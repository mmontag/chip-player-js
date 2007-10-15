/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Pha Packer modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by
 * Azatoth/Phenomena. Tested with modules sent by Bert Jahn.
 *
 * This format is called ProPacker V1.0 by NoiseConverter and Exotic
 * Ripper, also called HanniPacker and StrangePlayer.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct pha_instrument {
    uint16 size;
    int8 unknown;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
    uint32 ptr;
    int8 finetune;
    int8 unknown2;
} PACKED;

struct pha_header {
    struct pha_instrument ins[31];
    uint32 len;
    uint8 unknown[10];
    uint32 pptr[128];
} PACKED;


int pha_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, k, l;
    int reuse[4];
    struct xxm_event *event, old[4];
    struct pha_header ph;
    uint8 x8, y8;
    int smp_size, test_ptr, pptr[128];

    LOAD_INIT ();

    fread (&ph, 1, sizeof (struct pha_header), f);
    B_ENDIAN32 (ph.len);

    m->xxh->len = ph.len >> 2;

    if (m->xxh->len > 128)
	return -1;

    for (i = 0; i < m->xxh->len; i++)
	B_ENDIAN32 (ph.pptr[i]);

    for (test_ptr = smp_size = i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (ph.ins[i].size);
	B_ENDIAN16 (ph.ins[i].loop_start);
	B_ENDIAN16 (ph.ins[i].loop_size);
	B_ENDIAN32 (ph.ins[i].ptr);

	if (ph.ins[i].size > 0x8000 ||
	    ph.ins[i].loop_start > ph.ins[i].size ||
	    (ph.ins[i].loop_start + ph.ins[i].loop_size) > (ph.ins[i].size + 1))
	    return -1;

	smp_size += ph.ins[i].size * 2;

	if (!test_ptr && smp_size == (ph.ins[i].size * 2)) {
	    if (ph.ins[i].ptr != 0x03c0)
		return -1;
	    test_ptr++;
	}
    }

    sprintf (m->type, "Pha Packer");

    MODULE_INFO ();

    /*
     * Find number of different pattern pointers == number of stored patterns
     */
    pptr[0] = ph.pptr[0];
    for (m->xxh->pat = i = 0; i < m->xxh->len; i++) {
	for (k = j = 0; j <= m->xxh->pat; j++) {
	    if (ph.pptr[i] == pptr[j]) {
		k = 1;
		break;
	    }
	}
	if (!k) {
	    pptr[++m->xxh->pat] = ph.pptr[i];
	}
    }

    /*
     * Build a pattern pointer table...
     */
    for (l = i = 0; i < m->xxh->pat; i++) {
	for (k = 0x7fffffff, j = 0; j < m->xxh->len; j++) {
	    if (ph.pptr[j] < k && ph.pptr[j] > l) {
		k = ph.pptr[j];
	    }
	}
	l = pptr[i] = k;
    }

    /*
     * ...and the order table
     */
    for (i = 0; i < m->xxh->len; i++) {
	for (j = 0; j < m->xxh->pat; j++) {
	    if (ph.pptr[i] == pptr[j])
		m->xxo[i] = j;
	}
    }

    m->xxh->trk = m->xxh->pat * m->xxh->chn;

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
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

	if (V (1) && m->xxs[i].len > 2) {
	    report ("[%2X] %04x %04x %04x %c %06x V%02x %+d\n",
		i, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		ph.ins[i].loop_size > 1 ? 'L' : ' ', ph.ins[i].ptr,
		m->xxi[i][0].vol, (int8) m->xxi[i][0].fin >> 4);
	}
    }

    /* Load samples */

    if (V (0))
	report ("Stored samples : %d ", m->xxh->smp);
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

    PATTERN_INIT ();

    /* Load and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    reuse[0] = reuse[1] = reuse[2] = reuse[3] = 0;

    for (i = 0; i < m->xxh->pat; i++) {
	fseek (f, pptr[i], SEEK_SET);
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);

	fread (&x8, 1, 1, f);
	fread (&y8, 1, 1, f);

	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);

		if (reuse[k]) {
		    event->note = old[k].note;
		    event->ins = old[k].ins;
		    event->fxt = old[k].fxt;
		    event->fxp = old[k].fxp;
		    reuse[k]--;
		    continue;
		}

		/* event format:
		 *
		 * 0000 0000  0000 0000  0000 0000  0000 0000
		 *   \     /  \       /       \  /  \       /
		 *     ins       note          fx     fxval
		 */

		if ((event->note = y8) != 0)
		    event->note = (event->note >> 1) + 36;
		old[k].note = event->note;
		old[k].ins = event->ins = x8 & 0x3f;

		fread (&x8, 1, 1, f);
		fread (&y8, 1, 1, f);

		event->fxt = LSN (x8);
		event->fxp = y8;

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

		old[k].fxt = event->fxt;
		old[k].fxp = event->fxp;

		fread (&x8, 1, 1, f);
		fread (&y8, 1, 1, f);

	        if (x8 == 0xff) {
		    reuse[k] = 0xff - y8;
		    fread (&x8, 1, 1, f);
		    fread (&y8, 1, 1, f);
		}
	    }
	}

	if (V (0))
	    report (".");
    }

    if (V (0))
	report ("\n");

    m->xxh->flg |= XXM_FLG_MODRNG;

    return 0;
}
