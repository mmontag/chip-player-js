/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Loader for NoisePacker 1/2/3 modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by Twins/Phenomena.
 *
 * NP2 tested with music from the Multica demo -- get it from the Multica .adf
 * using "dd if=Multica.adf of=NP2.multica bs=1 skip=85920 count=111180"
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"


struct np_instrument {
    uint32 unknown;		/* Asle: looks like it's always 0 */
    uint16 size;		/* Sample size in words */
    uint8 finetune;
    uint8 volume;
    uint32 unknown2;		/* Asle: looks like it's always 0 */
    uint16 loop_size;		/* Loop size in words */
    uint16 loop_start;		/* Loop start in bytes */
} PACKED;


struct np3_instrument {
    uint8 finetune;		/* Asle: not sure */
    uint8 volume;
    uint32 unknown;		/* Asle: looks like it's always 0 */
    uint16 size;		/* Sample size in words */
    uint32 unknown2;		/* Asle: looks like it's always 0 */
    uint16 loop_size;		/* Loop size in words */
    uint16 loop_start;		/* Loop start in words */
} PACKED;


struct np_header {
    uint16 hdrsize;		/* Header + instruments + 4 unknown bytes */
    uint16 ordnum;		/* Size of pattern list in bytes */
    uint16 unknown;
    uint16 trksize;		/* Track data size */
} PACKED;


int np_load(struct xmp_mod_context *m, FILE *f)
{
    int i, j, ver;
    struct xxm_event *event;
    struct np_header nh;
    struct np_instrument ni[31];
    struct np3_instrument n3[31];
    uint8 np_event[3];
    int smp_size, smp_size3;
    uint16 w;

    LOAD_INIT ();

    ver = m->fetch & XMP_CTL_FIXLOOP ? 1 : 2;
    fread (&nh, 1, sizeof (struct np_header), f);
    B_ENDIAN16 (nh.hdrsize);
    B_ENDIAN16 (nh.ordnum);
    B_ENDIAN16 (nh.trksize);

    m->xxh->len = nh.ordnum / 2;
    m->xxh->ins = m->xxh->smp = nh.hdrsize >> 4;

    if (m->xxh->len > 127 || m->xxh->ins > 31)
	return -1;

    for (smp_size = smp_size3 = i = 0; i < m->xxh->ins; i++) {
	fread (&ni[i], 1, sizeof (struct np_instrument), f);
	memcpy (&n3[i], &ni[i], 16);
	B_ENDIAN16 (ni[i].size);
	B_ENDIAN16 (ni[i].loop_start);
	B_ENDIAN16 (ni[i].loop_size);
	B_ENDIAN16 (n3[i].size);
	B_ENDIAN16 (n3[i].loop_start);
	B_ENDIAN16 (n3[i].loop_size);
	smp_size += 2 * ni[i].size;
	smp_size3 += 2 * n3[i].size;
    }

    fseek (f, 4, SEEK_CUR);	/* Skip pattern list size and unknown bytes */

    for (m->xxh->pat = i = 0; i < m->xxh->len; i++) {
	fread (&w, 2, 1, f);
	B_ENDIAN16 (w);
	if ((m->xxo[i] = w >> 3) > m->xxh->pat)
	    m->xxh->pat = m->xxo[i];
    }
    m->xxh->pat++;

    if (m->size == nh.hdrsize + nh.ordnum + 8 * m->xxh->pat
	+ nh.trksize + smp_size3)
	ver = 3;
    else if (m->size != nh.hdrsize + nh.ordnum + 8 * m->xxh->pat
	+ nh.trksize + smp_size)
	return -1;

    sprintf (m->type, "NoisePacker v%d", ver);

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	if (ver < 3 ) {
	    m->xxs[i].len = 2 * ni[i].size;
	    m->xxs[i].lps = (m->fetch & XMP_CTL_FIXLOOP ? 1 : 2) *
		ni[i].loop_start;
	    m->xxs[i].lpe = m->xxs[i].lps + 2 * ni[i].loop_size;
	    m->xxs[i].flg = ni[i].loop_size > 1 ? WAVE_LOOPING : 0;
	    m->xxi[i][0].fin = (int8) ni[i].finetune << 4;
	    m->xxi[i][0].vol = ni[i].volume;
	} else {
	    m->xxs[i].len = 2 * n3[i].size;
	    m->xxs[i].lps = (m->fetch & XMP_CTL_FIXLOOP ? 1 : 2) *
		n3[i].loop_start;
	    m->xxs[i].lpe = m->xxs[i].lps + 2 * n3[i].loop_size;
	    m->xxs[i].flg = n3[i].loop_size > 1 ? WAVE_LOOPING : 0;
	    m->xxi[i][0].fin = (int8) n3[i].finetune << 4;
	    m->xxi[i][0].vol = n3[i].volume;
	}
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;
	m->xxih[i].nsm = !!(m->xxs[i].len);
	m->xxih[i].rls = 0xfff;

	if (V (1) && m->xxs[i].len > 0) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		m->xxi[i][0].vol, (char) m->xxi[i][0].fin >> 4);
	}
    }

    /* Load and convert patterns */

    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);

    xxp = calloc (sizeof (struct xxm_pattern *), m->xxh->pat + 1);
    for (m->xxh->trk = i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;

	/* Asle's note: Beware because this list is reversed! Meaning that
	 * one pattern first contains the address of its track 4, then of
	 * track 3...
	 */

	for (j = 3; j >= 0; j--) {
	    fread (&w, 2, 1, f);
	    B_ENDIAN16 (w);
	    m->xxp[i]->info[j].index = w / 192;
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
	    fread (np_event, 1, 3, f);

	    /* Event format:
	     *
	     * 0000 0000  0000 0000  0000 0000
	     *  \     /\     / \  /  \       /
	     *   note    ins    fx   parameter
	     *
	     * In NoisePacker v3:
	     *
	     * description bit set to 1
	     * |
	     * 0000 0000
	     * \       /
	     * -(number of empty rows)
	     */

	    if (np_event[0] & 0x80) {
		j -= (int8)np_event[0];
		continue;
	    }

	    if (((event->note = np_event[0] >> 1) & 0x7f) > 0)
		event->note += 36;
	    event->ins = ((np_event[0] & 1) << 4) | MSN (np_event[1]);
	    event->fxt = LSN (np_event[1]);
	    event->fxp = np_event[2];

	    /* Asle's note: The arpeggio effect is remapped to the effect #8.
	     * Volume slide is remapped to the effect #7 BUT with signed value!!
	     * (7FE -> A01). The vibrato + volume slide (6) and portamento +
	     * volume slide (5) effects are signed. Pattern jump _seems_ to
	     * be *2 and starts at FC! So jump to pattern 0 is BFC, to pattern 1
	     * is BFE, to pattern 2 B00 etc.
	     */

	    switch (event->fxt) {
	    case 0x07:
		event->fxt = 0x0a;
	    case 0x06:
	    case 0x05:
		if ((int8)event->fxp < 0)
		    event->fxp = (-(int8)event->fxp) & 0x0f;
		else
		    event->fxp <<= 4;
		break;
	    case 0x0b:
		event->fxp += 4;;
		event->fxp >>= 1;
		break;
	    case 0x08:
		event->fxt = 0x00;
	    }

	    disable_continue_fx (event);
	}

	if (V (0) && !(i % 4))
	    report (".");
    }

    m->xxh->flg |= XXM_FLG_MODRNG;

    /* Load samples */

    /* Asle's note: [In NP3] the sample data start address is ALWAYS even!
     * It means that you have to bypass one byte if the current address is
     * not even.
     */

    if (ver == 3 && ftell (f) & 1)
	fseek (f, 1, SEEK_CUR);

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
