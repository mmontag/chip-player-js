/* Extended Module Player - pm20_load.c
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Promizer 2.0/4.0 mods based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Module sent by Bert Jahn.
 * Format created by MC68000/Masque/TRSI.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include "load.h"

struct pm20_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct pm20_header {
    uint16 len;			/* Song length */
    uint16 ord_size;		/* Size of pattern pointers */
    uint16 pptr[128];		/* Pattern pointers */
    struct pm20_instrument ins[31];
    uint32 smp_ptr;		/* Bytes to add to get the sample data addr */
    uint32 table_ptr;		/* Size of the pattern data */
} PACKED;

static int prom_20_40_load (struct xmp_mod_context *, FILE *);

static char *ver;
static int extra_size;


int pm20_load(struct xmp_mod_context *m, FILE *f)
{
    ver = "2.0";
    extra_size = 5198;

    return prom_20_40_load (f);
}


int pm40_load(struct xmp_mod_context *m, FILE *f)
{
    uint8 id[5];

    fread (id, 1, 5, f);
    if (id[0] != 'P' || id[1] != 'M' || id[2] !='4' || id[3] !='0')
	return -1;

    ver = "4.0";
    extra_size = 0;

    return prom_20_40_load (f);
}


static int prom_20_40_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, k, l;
    struct xxm_event *event;
    struct pm20_header ph;
    int table_size, smp_size, pptr[128];
    uint8 *pe;
    uint16 x16;
    uint32 *event_table;

    LOAD_INIT ();

    fseek (f, extra_size, SEEK_SET);
    fread (&ph, 1, sizeof (struct pm20_header), f);
    B_ENDIAN16 (ph.len);
    B_ENDIAN16 (ph.ord_size);
    B_ENDIAN32 (ph.smp_ptr);
    B_ENDIAN32 (ph.table_ptr);

    m->xxh->len = ph.ord_size >> 2;

    if (m->xxh->len > 128)
	return -1;

    for (i = 0; i < m->xxh->len; i++)
	B_ENDIAN16 (ph.pptr[i]);

    for (smp_size = i = 0; i < m->xxh->ins; i++) {
	B_ENDIAN16 (ph.ins[i].size);
	B_ENDIAN16 (ph.ins[i].loop_start);
	B_ENDIAN16 (ph.ins[i].loop_size);

	if (ph.ins[i].size > 0x8000 ||
	    ph.ins[i].loop_start > ph.ins[i].size ||
	    (ph.ins[i].loop_start + ph.ins[i].loop_size) > (ph.ins[i].size + 1))
	    return -1;

	smp_size += ph.ins[i].size * 2;
    }

    if ((extra_size + 4 + ph.smp_ptr + smp_size != m->size)
        && (extra_size + ph.smp_ptr + smp_size != m->size))
	return -1;

    sprintf (m->type, "Promizer %s", ver);

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
	m->xxi[i][0].fin = (int8) ph.ins[i].finetune << (3 + (extra_size == 0));
	m->xxi[i][0].vol = ph.ins[i].volume;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;

	if (V (1) && (strlen (m->xxih[i].name) || m->xxs[i].len > 2)) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		ph.ins[i].loop_size > 1 ? 'L' : ' ',
		m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    PATTERN_INIT ();

    /* Load and convert patterns */

    /*
     * Read the event table
     */

    fseek (f, extra_size + ph.table_ptr, SEEK_SET);
    table_size = ph.smp_ptr - ph.table_ptr;

    if (V (0))
	report ("Stored events  : %d\n", table_size);

    event_table = calloc (4, table_size);
    fread (event_table, 4, table_size, f);

    /*
     * Read and decode the patterns
     */

    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    fseek (f, extra_size + 516, SEEK_SET);
    for (i = 0; i < m->xxh->pat; i++) {
next_pattern:
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64; j++) {
	    for (k = 0; k < 4; k++) {
		event = &EVENT (i, k, j);
		fread (&x16, 2, 1, f);
		B_ENDIAN16 (x16);

		pe = (uint8 *) &event_table[x16];

		/* Promizer event format:
		 *
		 * 0000 0000  0000 0000  0000 0000  0000 0000
		 *  \    /    \      /        \  /  \       /
		 *   inst       note           fx     fxval
		 */

		if ((event->note = pe[1] >> 1) != 0)
		    event->note += 36;
		event->ins = pe[0] >> 2;
		event->fxt = LSN (pe[2]);
		event->fxp = pe[3];

		if (event->fxt == FX_JUMP || event->fxt == FX_BREAK) {
		    i++;
		    goto next_pattern;
		}

		disable_continue_fx (event);
	    }
	}

	if (V (0))
	    report (".");
    }

    free (event_table);

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    fseek (f, (extra_size ? extra_size + 4 : 0) + ph.smp_ptr , SEEK_SET);

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
