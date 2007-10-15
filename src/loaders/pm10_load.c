/* Extended Module Player - pm10_load.c
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr.
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for Promizer 1.0c/1.8a mods based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Module sent by Bert Jahn.
 * Format created by MC68000/Masque/TRSI.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include "load.h"

struct pm10_instrument {
    uint16 size;
    uint8 finetune;
    uint8 volume;
    uint16 loop_start;
    uint16 loop_size;
} PACKED;

struct pm10_header {
    uint32 smp_ptr;		/* Bytes to add to get the sample data addr */
    uint32 patt_size;		/* Size of the pattern data */
    struct pm10_instrument ins[31];
    uint16 len;			/* Size of pattern pointers */
    uint32 pptr[128];		/* Pattern pointers */
} PACKED;

static int prom_10_18_load (struct xmp_mod_context *, FILE *);

static char *ver;
static int extra_size;


int pm10_load(struct xmp_mod_context *m, FILE *f)
{
    ver = "1.0c";
    extra_size = 4452;

    return prom_10_18_load (f);
}


int pm18_load(struct xmp_mod_context *m, FILE *f)
{
    ver = "1.8a";
    extra_size = 4456;

    return prom_10_18_load (f);
}


static int prom_10_18_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, k, l;
    struct xxm_event *event;
    struct pm10_header ph;
    int table_size, smp_size, patt_ptr, pptr[128];
    uint16 x16;
    uint32 *event_table;

    LOAD_INIT ();

    fseek (f, extra_size, SEEK_SET);
    fread (&ph, 1, sizeof (struct pm10_header), f);
    B_ENDIAN32 (ph.smp_ptr);
    B_ENDIAN32 (ph.patt_size);
    B_ENDIAN16 (ph.len);

    m->xxh->len = ph.len >> 2;

    if (m->xxh->len > 128)
	return -1;

    for (i = 0; i < m->xxh->len; i++)
	B_ENDIAN32 (ph.pptr[i]);

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

    if (extra_size + 4 + ph.smp_ptr + smp_size != xmp_ctl->size)
	return -1;

    sprintf (xmp_ctl->type, "Promizer %s", ver);

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
    m->xxh->pat++;

    /*
     * Build a pattern pointer table...
     */
    for (l = -1, i = 0; i < m->xxh->pat; i++) {
	for (k = 0x7fffffff, j = 0; j < m->xxh->len; j++) {
	    if (ph.pptr[j] < k && (int)ph.pptr[j] > l) {
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
	if (m->xxi[i][0].fin > 48)
	    m->xxi[i][0].xpo = -1;
	if (m->xxi[i][0].fin < -48)
	    m->xxi[i][0].xpo = 1;

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
     * Find the number of entries in the event table
     */

    patt_ptr = ftell (f);
    ph.patt_size >>= 1;
    for (table_size = i = 0; i < ph.patt_size; i++) {
	fread (&x16, 2, 1, f);
	B_ENDIAN16 (x16);
	if (x16 > table_size)
	    table_size = x16;
    }

    if (V (0))
	report ("Stored events  : %d\n", table_size);

    /*
     * Read the event table
     */

    event_table = calloc (4, table_size);
    fread (event_table, 4, table_size, f);

    /*
     * Read and decode the patterns
     */

    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    fseek (f, patt_ptr, SEEK_SET);
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
		if (x16 < table_size)
		    cvt_pt_event (event, (uint8 *)&event_table[x16]);

		if (event->fxt == FX_JUMP || event->fxt == FX_BREAK) {
		    i++;
		    goto next_pattern;
		}
	    }
	}

	if (V (0))
	    report (".");
    }

    free (event_table);

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    fseek (f, extra_size + 4 + ph.smp_ptr, SEEK_SET);

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
