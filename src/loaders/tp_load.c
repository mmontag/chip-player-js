/* Extended Module Player
 * Copyright (C) 1996-2001 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: tp_load.c,v 1.1 2001-06-02 20:26:54 cmatsuoka Exp $
 */

/* Loader for The Player 6.0 modules based on the format description
 * written by Sylvain Chipaux (Asle/ReDoX). Format created by Jarno
 * Paananen (Guru/Sahara Surfers)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include "load.h"
#include "period.h"


struct tp_instrument {
    uint16 size;		/* Sample size in words */
    uint8 finetune;		/* Finetune (0->F) */
    uint8 volume;		/* Volume (0->40h) */
    uint16 loop_start;		/* Loop start in words */
} PACKED;


struct tp_header {
    uint16 smp_addr;		/* Address of the sample data */
    uint8 pat;			/* Number of patterns saved */
    uint8 ins;			/* (& 0x3f) Number of instruments */
} PACKED;



static void read_event (int t, int j, uint8 *track)
{
    struct xxm_event *event;

    event = &xxt[t]->event[j];

    if ((event->note = (track[0] & 0x7f) >> 1) > 0)
	event->note += 36;
    event->ins = ((track[0] & 0x01) << 5) | MSN (track[1]);
    event->fxt = LSN (track[1]);
    event->fxp = track[2];

    switch (event->fxt) {
    case 0x0a:
    case 0x06:
    case 0x05:
	if ((int8)event->fxp < 0)
	    event->fxp = (-(int8)event->fxp) & 0x0f;
	else
	    event->fxp <<= 4;
	break;
    case 0x08:
	event->fxt = 0x00;
    }

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
	}
    }
}


static void read_track (int t, int s, int i, uint8 *track)
{
    int j, k;

    for (j = s; j < s + i; j++) {
	/* Event format:
	 *
	 * case 1: (flag bit set to 0)
	 *
	 * flag bit set to 0
	 * |
	 * 0000-0000  0000-0000  0000-0000
	 *  \     /\     / \  /  \       /
	 *   note    ins    fx     value
	 *
	 * case 2: (flag bit set to 1)
	 *
	 * flag bit set to 1
	 * |
	 * 0000-0000  0000-0000  0000-0000  0000-0000
	 *  \     /\     / \  /  \       /  \       /
	 *   note    ins    fx     value       skip
	 *
	 * case 3:
	 *
	 * 0000-0000  0000-0000  0000-0000  0000-0000
	 * \       /  \       /  \                  /
	 *   0x80       lines       number of bytes
	 */

	if (*track & 0x80) {
	    if (*track == 0x80) {
		int b = ((int)track[2] << 8) | track[3];
		read_track (t, j, track[1], track + 4 - b);
	        goto read_end;
	    }
	    if (track[3] < 0x80) {
		read_event (t, j, track);
		j += track[3];
		goto read_end;
	    }
	    for (k = j + 0x100 - track[3]; j < k; j++)
		read_event (t, k, track);
	    goto read_end;
	}

	read_event (t, j, track);
	track += 3;
	continue;

read_end:
	track += 4;
    }

}


int tp_load (FILE *f)
{
    int i, j, k, l;
    struct stat st;
    struct tp_header th;
    struct tp_instrument ti[31];
    uint8 *t, *track;
    uint16 *tptr, *sptr;
    int smp_size;

    return -1;

    fstat (fileno (f), &st);
    LOAD_INIT ();

    xxh->tpo = 6;
    xxh->bpm = 125;
    xxh->chn = 4;

    fread (&th, 1, sizeof (struct tp_header), f);

    B_ENDIAN16 (th.smp_addr);

    xxh->pat = th.pat;
    xxh->ins = xxh->smp = th.ins;

    if (xxh->ins > 31)
	return -1;

    for (smp_size = i = 0; i < xxh->ins; i++) {
	fread (&ti[i], 1, sizeof (struct tp_instrument), f);
	B_ENDIAN16 (ti[i].size);
	B_ENDIAN16 (ti[i].loop_start);
	smp_size += 2 * ti[i].size;
    }

    tptr = calloc (2, 4 * xxh->pat);
    sptr = calloc (2, 4 * xxh->pat + 1);
    fread (tptr, 2, 4 * xxh->pat, f);

    for (i = 0; i < xxh->pat; i++)
	B_ENDIAN16 (tptr[i]);

    /*
     * Find the number of different track pointers == number of stored tracks
     */
    sptr[0] = tptr[0];
    for (xxh->trk = i = 0; i < (xxh->pat * 4); i++) {
	for (k = j = 0; j <= xxh->trk; j++) {
	    if (tptr[i] == sptr[j]) {
		k = 1;
		break;
	    }
	}
	if (!k) {
	    sptr[++xxh->trk] = tptr[i];
	}
    }
    xxh->trk++;

    /*
     * Build the track index
     */
    for (l = -1, i = 0; i < xxh->trk; i++) {
        for (k = 0x7fffffff, j = 0; j < (xxh->pat * 4); j++) {
            if (tptr[j] < k && (int)sptr[j] > l) {
                k = tptr[j];
            }
        }
        l = sptr[i] = k;
    }

    sptr[xxh->trk] = th.smp_addr;

    /*
     * Load orders
     */
    for (xxh->len = i = 0; ; i++) {
	fread (&xxo[i], 1, 1, f);
	if (xxo[i] == 0xff)
	    break;
    }
    xxh->len = i;
    

#if 0 
    if (st.st_size != th.hdrsize + th.ordnum + 8 * xxh->pat
	+ th.trksize + smp_size)
	return -1;
#endif

    sprintf (xmp_ctl->type, "The Player 6.0A");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * ti[i].size;
	xxs[i].lps = ti[i].loop_start == 0xffff ? 0 : ti[i].loop_start;
	xxs[i].lpe = ti[i].loop_start == 0xffff ? 0 : 2 * ti[i].size;
	xxs[i].flg = ti[i].loop_start == 0xffff ? 0 : WAVE_LOOPING;
	xxi[i][0].fin = (int8) ti[i].finetune << 4;
	xxi[i][0].vol = ti[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	xxih[i].rls = 0xfff;

	if (V (1) && xxs[i].len > 0) {
	    report ("[%2X] %04x %04x %04x %c V%02x %+d\n",
		i, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
	}
    }

    /* Load and convert tracks */

    if (V (0))
	report ("Stored tracks  : %d ", xxh->trk);

    xxt = calloc (sizeof (struct xxm_track *), xxh->trk);
    t = calloc (1, 256);

    for (i = 0; i < xxh->trk; i++) {
	track = t;
	xxt[i] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * 64, 1);
	xxt[i]->rows = 64;

	/* Slurp the track data from the input stream */
	fread (track, 1, sptr[i + 1] - sptr[i], f);

	/* Read 64 lines from the track */
	read_track (i, 0, 64, track);

	if (V (0) && !(i % 4))
	    report (".");
    }

#if 0
    /* Load and convert patterns */

    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);

    xxp = calloc (sizeof (struct xxm_pattern *), xxh->pat + 1);
    for (xxh->trk = i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;

	/* Asle's note: Beware because this list is reversed! Meaning that
	 * one pattern first contains the address of its track 4, then of
	 * track 3...
	 */

	for (j = 3; j >= 0; j--) {
	    fread (&w, 2, 1, f);
	    B_ENDIAN16 (w);
	    xxp[i]->info[j].index = w / 192;
	    if (xxp[i]->info[j].index > xxh->trk)
		xxh->trk = xxp[i]->info[j].index;
	}
	if (V (0))
	    report (".");
    }
    xxh->trk++;

    if (xmp_ctl->fetch & XMP_CTL_MODOCT)	/* #?# **** FIXME **** */
	xxh->flg |= XXM_FLG_MODRNG;
    if (xmp_ctl->fetch & XMP_CTL_NTSC) {
	xmp_ctl->rrate = NTSC_RATE;
	xmp_ctl->c4rate = C4_NTSC_RATE;
    }

    /* Load samples */

    /* Asle's note: [In NP3] the sample data start address is ALWAYS even!
     * It means that you have to bypass one byte if the current address is
     * not even.
     */

    if (ver == 3 && ftell (f) & 1)
	fseek (f, 1, SEEK_CUR);

    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->smp; i++) {
	if (!xxs[i].len)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = (((i + 1) / 2) % 2) * 0xff;
#endif

    return 0;
}
