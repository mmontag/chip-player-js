/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: mtm_load.c,v 1.5 2007-10-13 18:25:05 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "mtm.h"

static int mtm_test (FILE *, char *);
static int mtm_load (FILE *);

struct xmp_loader_info mtm_loader = {
    "MTM",
    "Multitracker",
    mtm_test,
    mtm_load
};

static int mtm_test(FILE *f, char *t)
{
    uint8 buf[4];

    fread(buf, 1, 4, f);
    if (memcmp(buf, "MTM", 3))
	return -1;
    if (buf[3] != 0x10)
	return -1;

    read_title(f, t, 20);

    return 0;
}


static int mtm_load(FILE *f)
{
    int i, j;
    struct mtm_file_header mfh;
    struct mtm_instrument_header mih;
    uint8 mt[192];
    uint16 mp[32];

    LOAD_INIT ();

    fread(&mfh.magic, 3, 1, f);		/* "MTM" */
    mfh.version = read8(f);		/* MSN=major, LSN=minor */
    fread(&mfh.name, 20, 1, f);		/* ASCIIZ Module name */
    mfh.tracks = read16l(f);		/* Number of tracks saved */
    mfh.patterns = read8(f);		/* Number of patterns saved */
    mfh.modlen = read8(f);		/* Module length */
    mfh.extralen = read16l(f);		/* Length of the comment field */
    mfh.samples = read8(f);		/* Number of samples */
    mfh.attr = read8(f);		/* Always zero */
    mfh.rows = read8(f);		/* Number rows per track */
    mfh.channels = read8(f);		/* Number of tracks per pattern */
    fread(&mfh.pan, 32, 1, f);		/* Pan positions for each channel */

#if 0
    if (strncmp ((char *)mfh.magic, "MTM", 3))
	return -1;
#endif

    xxh->trk = mfh.tracks + 1;
    xxh->pat = mfh.patterns + 1;
    xxh->len = mfh.modlen + 1;
    xxh->ins = mfh.samples;
    xxh->smp = xxh->ins;
    xxh->chn = mfh.channels;
    xxh->tpo = 6;
    xxh->bpm = 125;

    strncpy(xmp_ctl->name, (char *)mfh.name, 20);
    sprintf(xmp_ctl->type, "MTM (MultiTracker %d.%02d)",
				MSN(mfh.version), LSN(mfh.version));

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    /* Read and convert instruments */
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	fread(&mih.name, 22, 1, f);		/* Instrument name */
	mih.length = read32l(f);		/* Instrument length in bytes */
	mih.loop_start = read32l(f);		/* Sample loop start */
	mih.loopend = read32l(f);		/* Sample loop end */
	mih.finetune = read8(f);		/* Finetune */
	mih.volume = read8(f);			/* Playback volume */
	mih.attr = read8(f);			/* &0x01: 16bit sample */

	xxih[i].nsm = !!(xxs[i].len = mih.length);
	xxs[i].lps = mih.loop_start;
	xxs[i].lpe = mih.loopend;
	xxs[i].flg = xxs[i].lpe ? WAVE_LOOPING : 0;	/* 1 == Forward loop */
	xxs[i].flg |= mfh.attr & 1 ? WAVE_16_BITS : 0;
	xxi[i][0].vol = mih.volume;
	xxi[i][0].fin = 0x80 + (int8)(mih.finetune << 4);
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, mih.name, 22);

	if ((V (1)) && (strlen ((char *) xxih[i].name) || xxs[i].len))
	    report ("[%2X] %-22.22s %04x%c%04x %04x %c V%02x F%+03d\n", i,
		xxih[i].name, xxs[i].len, xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
		xxs[i].lps, xxs[i].lpe, xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
		xxi[i][0].vol, xxi[i][0].fin - 0x80);
    }

    fread (xxo, 1, 128, f);

    PATTERN_INIT ();

    if (V (0))
	report ("Stored tracks  : %d ", xxh->trk - 1);
    for (i = 0; i < xxh->trk; i++) {
	xxt[i] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * mfh.rows, 1);
	xxt[i]->rows = mfh.rows;
	if (!i)
	    continue;
	fread (&mt, 3, 64, f);
	for (j = 0; j < 64; j++) {
	    if ((xxt[i]->event[j].note = mt[j * 3] >> 2))
		xxt[i]->event[j].note += 25;
	    xxt[i]->event[j].ins = ((mt[j * 3] & 0x3) << 4) + MSN (mt[j * 3 + 1]);
	    xxt[i]->event[j].fxt = LSN (mt[j * 3 + 1]);
	    xxt[i]->event[j].fxp = mt[j * 3 + 2];
	    if (xxt[i]->event[j].fxt > FX_TEMPO)
		xxt[i]->event[j].fxt = xxt[i]->event[j].fxp = 0;
	    /* Set pan effect translation */
	    if ((xxt[i]->event[j].fxt == FX_EXTENDED) &&
		(MSN (xxt[i]->event[j].fxp) == 0x8)) {
		xxt[i]->event[j].fxt = FX_SETPAN;
		xxt[i]->event[j].fxp <<= 4;
	    }
	}
	if (V (0) && !(i % xxh->chn))
	    report (".");
    }
    if (V (0))
	report ("\n");

    /* Read patterns */
    reportv(0, "Stored patterns: %d ", xxh->pat - 1);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	for (j = 0; j < 32; j++)
	    mp[j] = read16l(f);
	for (j = 0; j < xxh->chn; j++)
	    xxp[i]->info[j].index = mp[j];
	reportv(0, ".");
    }

    /* Comments */
    for (i = 0; i < mfh.extralen; i++)
	fread (&j, 1, 1, f);

    /* Read samples */
    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->ins; i++) {
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate,
	    XMP_SMP_UNS, &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = mfh.pan[i] << 4;

    return 0;
}
