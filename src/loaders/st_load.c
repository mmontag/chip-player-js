/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Ultimate Soundtracker support based on the module format description
 * written by Michael Schwendt <sidplay@geocities.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <sys/types.h>

#include "load.h"
#include "mod.h"
#include "period.h"


int st_load (FILE *f)
{
    int i, j;
    int smp_size, hdr_size, pat_size;
    struct xxm_event *event;
    struct st_header mh;
    uint8 mod_event[4];
    int ust = 1, nt = 0, serr = 0;
    /* int lps_mult = xmp_ctl->fetch & XMP_CTL_FIXLOOP ? 1 : 2; */
    char *modtype;
    int fxused;

    LOAD_INIT ();

    xxh->ins = 15;
    xxh->smp = xxh->ins;
    smp_size = 0;
    pat_size = 0;
    //lps_mult = 1;

    hdr_size = sizeof (struct st_header);
    fread (&mh, 1, sizeof (struct st_header), f);

    xxh->len = mh.len;
    xxh->rst = mh.restart;

    /* UST: The byte at module offset 471 is BPM, not the song restart */

    if (xxh->rst < 0x20)
	ust = 0;

#if 0
    if (xxh->rst == 0x78)	/* Is it correct? */
	nt = 1;
#endif

    if (xxh->rst >= xxh->len)
	xxh->rst = 0;
    memcpy (xxo, mh.order, 128);

    for (i = 0; i < 128; i++)
	if (xxo[i] > xxh->pat)
	    xxh->pat = xxo[i];
    xxh->pat++;

    if (xxh->pat > 0x7f || xxh->len == 0 || xxh->len > 0x7f)
	return -1;

    pat_size = 256 * xxh->chn * xxh->pat;

    for (i = 0; i < xxh->ins; i++) {
	B_ENDIAN16 (mh.ins[i].size);
	B_ENDIAN16 (mh.ins[i].loop_start);
	B_ENDIAN16 (mh.ins[i].loop_size);

	if (mh.ins[i].volume > 0x40)
	    return -1;

	if (mh.ins[i].finetune > 0x0f)
	    return -1;

	/* UST: Volume word does not contain a "Finetuning" value in its
	 * high-byte.
	 */
	if (mh.ins[i].finetune)
	    ust = 0;

	if (mh.ins[i].size > 0x8000 || (mh.ins[i].loop_start >> 1) > 0x8000
		|| mh.ins[i].loop_size > 0x8000)
	    return -1;

	if (mh.ins[i].loop_size > 1 && mh.ins[i].loop_size > mh.ins[i].size)
	    return -1;

	if ((mh.ins[i].loop_start >> 1) > mh.ins[i].size)
	    return -1;

	if (mh.ins[i].size && (mh.ins[i].loop_start >> 1) == mh.ins[i].size)
	    return -1;

	if (mh.ins[i].size == 0 && mh.ins[i].loop_start > 0)
	    return -1;

	if (mh.ins[i].size == 0 && mh.ins[i].loop_size == 1)
	    nt = 1;

	/* UST: Maximum sample length is 9999 bytes decimal, but 1387 words
	 * hexadecimal. Longest samples on original sample disk ST-01 were
	 * 9900 bytes.
	 */
	if (mh.ins[i].size > 0x1387 || mh.ins[i].loop_start > 9999
		|| mh.ins[i].loop_size > 0x1387)
	    ust = 0;

	smp_size += 2 * mh.ins[i].size;
    }

    if (smp_size < 8)
	return -1;

    /* Relaxed limit -- was 4 */
    if (abs (serr = xmp_ctl->size - (600 + xxh->pat*1024 + smp_size)) > 8192)
	return -1;

    INSTRUMENT_INIT ();

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxs[i].len = 2 * mh.ins[i].size;
	xxs[i].lps = mh.ins[i].loop_start;
	xxs[i].lpe = xxs[i].lps + 2 * mh.ins[i].loop_size;
	xxs[i].flg = mh.ins[i].loop_size > 1 ? WAVE_LOOPING : 0;
	xxi[i][0].fin = (int8)(mh.ins[i].finetune << 4);
	xxi[i][0].vol = mh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxih[i].nsm = !!(xxs[i].len);
	strncpy (xxih[i].name, mh.ins[i].name, 22);
	str_adj (xxih[i].name);
    }

#if 0
    /* Another filter for Soundtracker modules */
    if (xxh->ins == 15 && sizeof (struct st_header) + pat_size
	+ smp_size > xmp_ctl->size)
	return -1;
#endif

    xxh->trk = xxh->chn * xxh->pat;

    strncpy (xmp_ctl->name, (char *) mh.name, 20);

    if (nt)
	strcpy (tracker_name, "Sound/Noisetracker");
    else 
	strcpy (tracker_name, "old Soundtracker");

    strcpy (xmp_ctl->type, "15 instrument module");

    MODULE_INFO ();

    if (serr && V(0))
	report ("File size error: %d\n", serr);

    PATTERN_INIT ();

    /* Load and convert patterns */

    fxused = 0;

    if (V(0))
	report ("Stored patterns: %d ", xxh->pat);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * xxh->chn); j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (mod_event, 1, 4, f);

	    cvt_pt_event (event, mod_event);

	    if (event->fxt)
		fxused |= 1 << event->fxt;
	    else if (event->fxp)
		fxused |= 1;
	    
	    /* UST: Only effects 1 (arpeggio) and 2 (pitchbend) are
	     * available.
	     */
	    if (event->fxt != 1 && event->fxp != 2)
		ust = 0;

	    /* Special translation for e8 (set panning) effect.
	     * This is not an official Protracker effect but DMP uses
	     * it for panning, and a couple of modules follow this
	     * "standard".
	     */
	    if ((event->fxt == 0x0e) && ((event->fxp & 0xf0) == 0x80)) {
		event->fxt = FX_SETPAN;
		event->fxp <<= 4;
	    }
	}
	if (V (0))
	    report (".");
    }

    if (fxused == 0)
	modtype = "MasterSoundtracker 1.1";
    else if ((fxused & ~0x0006) == 0)
	modtype = "Ultimate Soundtracker";
    else if ((fxused & ~0xd007) == 0)
	modtype = "Soundtracker IX";
    else if ((fxused & ~0xf807) == 0)
	modtype = "D.O.C. Soundtracker";
    else if ((fxused & ~0xfc07) == 0)
	modtype = "Soundtracker 2.3/2.4";
    else if ((fxused & ~0xfc3f) == 0)
	modtype = "Noisetracker 1.0/1.2";
    else if ((fxused & ~0xfcbf) == 0)
	modtype = "Noisetracker 2.0";
    else if ((fxused & ~0xfeff) == 0)
	modtype = "Protracker";
    else
	modtype = NULL;

    if (V (0)) {
	if (modtype)
	    report ("\nCompatibility  : %s\n", modtype);
    }

    if (V (1))
	report ("     Instrument name        Len  LBeg LEnd L Vol Fin\n");

    for (i = 0; (V (1)) && (i < xxh->ins); i++) {
	if ((strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
	    report ("[%2X] %-22.22s %04x %04x %04x %c V%02x %+d\n",
		i, xxih[i].name, xxs[i].len, xxs[i].lps,
		xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
		xxi[i][0].vol, (char) xxi[i][0].fin >> 4);
    }

    xxh->flg |= XXM_FLG_MODRNG;

    /* Perform the necessary conversions for Ultimate Soundtracker */
    if (ust) {
	/* Fix restart & bpm */
	xxh->bpm = xxh->rst;
	xxh->rst = 0;

	/* Fix sample loops */
	for (i = 0; i < xxh->ins; i++) {
	    /* FIXME */	
	}

	/* Fix effects (arpeggio and pitchbending) */
	for (i = 0; i < xxh->pat; i++) {
	    for (j = 0; j < (64 * xxh->chn); j++) {
		event = &EVENT(i, j % xxh->chn, j / xxh->chn);
		if (event->fxt == 1)
		    event->fxt = 0;
		else if (event->fxt == 2 && (event->fxp & 0xf0) == 0)
		    event->fxt = 1;
		else if (event->fxt == 2 && (event->fxp & 0x0f) == 0)
		    event->fxp >>= 4;
	    }
	}
    }

    /* Load samples */

    if (V (0))
	report ("Stored samples : %d ", xxh->smp);
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

    return 0;
}
