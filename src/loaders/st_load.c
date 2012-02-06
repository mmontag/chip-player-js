/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Ultimate Soundtracker support based on the module format description
 * written by Michael Schwendt <sidplay@geocities.com>
 */

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "load.h"
#include "mod.h"
#include "period.h"

static int st_test (FILE *, char *, const int);
static int st_load (struct context_data *, FILE *, const int);

struct format_loader st_loader = {
    "ST",
    "Soundtracker",
    st_test,
    st_load
};

static int period[] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
    -1
};

static int st_test(FILE *f, char *t, const int start)
{
    int i, j, k;
    int pat, smp_size;
    struct st_header mh;
    uint8 mod_event[4];
    struct stat st;

    fstat(fileno(f), &st);

    if (st.st_size < 600)
	return -1;

    smp_size = 0;

    fseek(f, start, SEEK_SET);
    fread(mh.name, 1, 20, f);
    if (test_name(mh.name, 20) < 0)
	return -1;

    for (i = 0; i < 15; i++) {
	fread(mh.ins[i].name, 1, 22, f);
	mh.ins[i].size = read16b(f);
	mh.ins[i].finetune = read8(f);
	mh.ins[i].volume = read8(f);
	mh.ins[i].loop_start = read16b(f);
	mh.ins[i].loop_size = read16b(f);
    }
    mh.len = read8(f);
    mh.restart = read8(f);
    fread(mh.order, 1, 128, f);
	
    for (pat = i = 0; i < 128; i++) {
	if (mh.order[i] > 0x7f)
	    return -1;
	if (mh.order[i] > pat)
	    pat = mh.order[i];
    }
    pat++;

    if (pat > 0x7f || mh.len == 0 || mh.len > 0x7f)
	return -1;

    for (i = 0; i < 15; i++) {
	if (test_name(mh.ins[i].name, 22) < 0)
	    return -1;

	if (mh.ins[i].volume > 0x40)
	    return -1;

	if (mh.ins[i].finetune > 0x0f)
	    return -1;

	if (mh.ins[i].size > 0x8000)
	    return -1;

	if ((mh.ins[i].loop_start >> 1) > 0x8000)
	    return -1;

	if (mh.ins[i].loop_size > 0x8000)
	    return -1;

	/* This test fails in atmosfer.mod, disable it 
	 *
	 * if (mh.ins[i].loop_size > 1 && mh.ins[i].loop_size > mh.ins[i].size)
	 *    return -1;
	 */

	if ((mh.ins[i].loop_start >> 1) > mh.ins[i].size)
	    return -1;

	if (mh.ins[i].size && (mh.ins[i].loop_start >> 1) == mh.ins[i].size)
	    return -1;

	if (mh.ins[i].size == 0 && mh.ins[i].loop_start > 0)
	    return -1;

	smp_size += 2 * mh.ins[i].size;
    }

    if (smp_size < 8)
	return -1;

    if (st.st_size < (600 + pat * 1024 + smp_size))
	return -1;

    for (i = 0; i < pat; i++) {
	for (j = 0; j < (64 * 4); j++) {
	    int p;
	
	    fread (mod_event, 1, 4, f);

	    if (MSN(mod_event[0]))	/* sample number > 15 */
		return -1;

	    p = 256 * LSN(mod_event[0]) + mod_event[1];

	    if (p == 0)
		continue;

	    if (p == 162)	/* used in Karsten Obarski's blueberry.mod */
		continue;

	    for (k = 0; period[k] >= 0; k++) {
		if (p == period[k])
		    break;
	    }
	    if (period[k] < 0)
		return -1;
	}
    }

    return 0;
}

static int st_load(struct context_data *ctx, FILE *f, const int start)
{
    struct module_data *m = &ctx->m;
    struct xmp_module *mod = &m->mod;
    int i, j;
    int smp_size;
    struct xmp_event ev, *event;
    struct st_header mh;
    uint8 mod_event[4];
    int ust = 1, nt = 0, serr = 0;
    /* int lps_mult = m->fetch & XMP_CTL_FIXLOOP ? 1 : 2; */
    char *modtype;
    int fxused;
    int pos;

    LOAD_INIT();

    mod->ins = 15;
    mod->smp = mod->ins;
    smp_size = 0;

    fread(mh.name, 1, 20, f);
    for (i = 0; i < 15; i++) {
	fread(mh.ins[i].name, 1, 22, f);
	mh.ins[i].size = read16b(f);
	mh.ins[i].finetune = read8(f);
	mh.ins[i].volume = read8(f);
	mh.ins[i].loop_start = read16b(f);
	mh.ins[i].loop_size = read16b(f);
    }
    mh.len = read8(f);
    mh.restart = read8(f);
    fread(mh.order, 1, 128, f);
	
    mod->len = mh.len;
    mod->rst = mh.restart;

    /* UST: The byte at module offset 471 is BPM, not the song restart
     *      The default for UST modules is 0x78 = 120 BPM = 48 Hz.
     */
    if (mod->rst < 0x40)	/* should be 0x20 */
	ust = 0;

    memcpy (mod->xxo, mh.order, 128);

    for (i = 0; i < 128; i++)
	if (mod->xxo[i] > mod->pat)
	    mod->pat = mod->xxo[i];
    mod->pat++;

    for (i = 0; i < mod->ins; i++) {
	/* UST: Volume word does not contain a "Finetuning" value in its
	 * high-byte.
	 */
	if (mh.ins[i].finetune)
	    ust = 0;

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

    INSTRUMENT_INIT();

    for (i = 0; i < mod->ins; i++) {
	mod->xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
	mod->xxs[i].len = 2 * mh.ins[i].size;
	mod->xxs[i].lps = mh.ins[i].loop_start;
	mod->xxs[i].lpe = mod->xxs[i].lps + 2 * mh.ins[i].loop_size;
	mod->xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
	mod->xxi[i].sub[0].fin = (int8)(mh.ins[i].finetune << 4);
	mod->xxi[i].sub[0].vol = mh.ins[i].volume;
	mod->xxi[i].sub[0].pan = 0x80;
	mod->xxi[i].sub[0].sid = i;
	mod->xxi[i].nsm = !!(mod->xxs[i].len);
	strncpy((char *)mod->xxi[i].name, (char *)mh.ins[i].name, 22);
	str_adj((char *)mod->xxi[i].name);
    }

    mod->trk = mod->chn * mod->pat;

    strncpy (mod->name, (char *) mh.name, 20);

    /* Scan patterns for tracker detection */
    fxused = 0;
    pos = ftell(f);

    for (i = 0; i < mod->pat; i++) {
	for (j = 0; j < (64 * mod->chn); j++) {
	    fread (mod_event, 1, 4, f);

	    cvt_pt_event (&ev, mod_event);

	    if (ev.fxt)
		fxused |= 1 << ev.fxt;
	    else if (ev.fxp)
		fxused |= 1;
	    
	    /* UST: Only effects 1 (arpeggio) and 2 (pitchbend) are
	     * available.
	     */
	    if (ev.fxt && ev.fxt != 1 && ev.fxt != 2)
		ust = 0;

	    /* Karsten Obarski's sleepwalk mod uses arpeggio 30 and 40 */
	    if (ev.fxt == 1) {		/* unlikely arpeggio */
		if (ev.fxp == 0x00)
		    ust = 0;
		/*if ((ev.fxp & 0x0f) == 0 || (ev.fxp & 0xf0) == 0)
		    ust = 0;*/
	    }

	    if (ev.fxt == 2) {		/* bend up and down at same time? */
		if ((ev.fxp & 0x0f) != 0 && (ev.fxp & 0xf0) != 0)
		    ust = 0;
	    }
	}
    }

    if (fxused & ~0x0006)
	ust = 0;

    if (ust)
	modtype = "Ultimate Soundtracker";
    else if ((fxused & ~0xd007) == 0)
	modtype = "Soundtracker IX";	/* or MasterSoundtracker? */
    else if ((fxused & ~0xf807) == 0)
	modtype = "D.O.C Soundtracker 2.0";
    else if ((fxused & ~0xfc07) == 0)
	modtype = "Soundtracker 2.3/2.4";
    else if ((fxused & ~0xfc3f) == 0)
	modtype = "Noisetracker 1.0/1.2";
    else if ((fxused & ~0xfcbf) == 0)
	modtype = "Noisetracker 2.0";
    else
	modtype = "unknown tracker";

    if (ust)
	snprintf(mod->type, XMP_NAMESIZE, "%s", modtype);
    else
	snprintf(mod->type, XMP_NAMESIZE, "%s", modtype);

    MODULE_INFO();

    if (serr) {
	_D(_D_CRIT "File size error: %d", serr);
    }

    fseek(f, start + pos, SEEK_SET);

    PATTERN_INIT();

    /* Load and convert patterns */

    _D(_D_INFO "Stored patterns: %d", mod->pat);

    for (i = 0; i < mod->pat; i++) {
	PATTERN_ALLOC (i);
	mod->xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < (64 * mod->chn); j++) {
	    event = &EVENT (i, j % mod->chn, j / mod->chn);
	    fread (mod_event, 1, 4, f);

	    cvt_pt_event(event, mod_event);
	}
    }

    for (i = 0; i < mod->ins; i++) {
	_D(_D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d",
		i, mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps,
		mod->xxs[i].lpe, mh.ins[i].loop_size > 1 ? 'L' : ' ',
		mod->xxi[i].sub[0].vol, mod->xxi[i].sub[0].fin >> 4);
    }

    m->quirk |= QUIRK_MODRNG;

    /* Perform the necessary conversions for Ultimate Soundtracker */
    if (ust) {
	/* Fix restart & bpm */
	mod->bpm = mod->rst;
	mod->rst = 0;

	/* Fix sample loops */
	for (i = 0; i < mod->ins; i++) {
	    /* FIXME */	
	}

	/* Fix effects (arpeggio and pitchbending) */
	for (i = 0; i < mod->pat; i++) {
	    for (j = 0; j < (64 * mod->chn); j++) {
		event = &EVENT(i, j % mod->chn, j / mod->chn);
		if (event->fxt == 1)
		    event->fxt = 0;
		else if (event->fxt == 2 && (event->fxp & 0xf0) == 0)
		    event->fxt = 1;
		else if (event->fxt == 2 && (event->fxp & 0x0f) == 0)
		    event->fxp >>= 4;
	    }
	}
    } else {
	if (mod->rst >= mod->len)
	    mod->rst = 0;
    }

    /* Load samples */

    _D(_D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->smp; i++) {
	if (!mod->xxs[i].len)
	    continue;
	load_sample(ctx, f, mod->xxi[i].sub[0].sid, SAMPLE_FLAG_FULLREP,
	    &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
    }

    return 0;
}
