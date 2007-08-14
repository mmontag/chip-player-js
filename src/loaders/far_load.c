/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: far_load.c,v 1.4 2007-08-14 03:33:53 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on the Farandole Composer format specifications by Daniel Potter.
 *
 * "(...) this format is for EDITING purposes (storing EVERYTHING you're
 * working on) so it may include information not completely neccessary."
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "far.h"


#define NONE			0xff
#define FX_FAR_SETVIBRATO	0xfe
#define FX_FAR_F_VSLIDE_UP	0xfd

static uint8 fx[] =
{
    NONE,		FX_PORTA_UP,
    FX_PORTA_DN,	FX_TONEPORTA,
    NONE,		FX_FAR_SETVIBRATO,
    FX_VIBRATO,		FX_FAR_F_VSLIDE_UP,
    FX_VOLSLIDE,	NONE,
    NONE,		NONE,
    FX_EXTENDED,	NONE,
    NONE,		FX_TEMPO
};


int far_load (FILE * f)
{
    int i, j, vib = 0;
    struct xxm_event *event;
    struct far_header ffh;
    struct far_header2 ffh2;
    struct far_instrument fih;
    struct far_event fe;
    uint8 sample_map[8];

    LOAD_INIT ();

    fread(&ffh.magic, 4, 1, f);		/* File magic: 'FAR\xfe' */
    fread(&ffh.name, 40, 1, f);		/* Song name */
    fread(&ffh.crlf, 3, 1, f);		/* 0x0d 0x0a 0x1A */
    ffh.headersize = read16l(f);	/* Remaining header size in bytes */
    ffh.version = read8(f);		/* Version MSN=major, LSN=minor */
    fread(&ffh.ch_on, 16, 1, f);	/* Channel on/off switches */
    fread(&ffh.rsvd1, 9, 1, f);		/* Current editing values */
    ffh.tempo = read8(f);		/* Default tempo */
    fread(&ffh.pan, 16, 1, f);		/* Channel pan definitions */
    read32l(f);				/* Grid, mode (for editor) */
    ffh.textlen = read16l(f);		/* Length of embedded text */

    if (strncmp ((char *) ffh.magic, "FAR", 3) || (ffh.magic[3] != 0xfe))
	return -1;

    fseek(f, ffh.textlen, SEEK_CUR);	/* Skip song text */

    fread(&ffh2.order, 256, 1, f);	/* Orders */
    ffh2.patterns = read8(f);		/* Number of stored patterns (?) */
    ffh2.songlen = read8(f);		/* Song length in patterns */
    ffh2.restart = read8(f);		/* Restart pos */
    for (i = 0; i < 256; i++)
	ffh2.patsize[i] = read16l(f);	/* Size of each pattern in bytes */

    xxh->chn = 16;
    /*xxh->pat=ffh2.patterns; (Error in specs? --claudio) */
    xxh->len = ffh2.songlen;
    xxh->tpo = 6;
    xxh->bpm = 8 * 60 / ffh.tempo;
    memcpy (xxo, ffh2.order, xxh->len);

    for (xxh->pat = i = 0; i < 256; i++) {
	if (ffh2.patsize[i])
	    xxh->pat = i + 1;
    }

    xxh->trk = xxh->chn * xxh->pat;

    strncpy(xmp_ctl->name, (char *)ffh.name, 40);
    sprintf(xmp_ctl->type, "Farandole Composer %d.%d",
	MSN (ffh.version), LSN (ffh.version));

    MODULE_INFO ();

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0)) {
	report("Comment bytes  : %d\n", ffh.textlen);
	report("Stored patterns: %d ", xxh->pat);
    }

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	if (!ffh2.patsize[i])
	    continue;
	xxp[i]->rows = (ffh2.patsize[i] - 2) / 64;
	TRACK_ALLOC (i);
	fread (&j, 1, 2, f);
	for (j = 0; j < xxp[i]->rows * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (&fe, 1, 4, f);
	    memset (event, 0, sizeof (struct xxm_event));
	    if (fe.note)
		event->note = fe.note + 36;
	    if (event->note || fe.instrument)
		event->ins = fe.instrument + 1;
	    fe.volume = LSN (fe.volume) * 16 + MSN (fe.volume);
	    if (fe.volume)
		event->vol = fe.volume - 0x10;
	    event->fxt = fx[MSN (fe.effect)];
	    event->fxp = LSN (fe.effect);
	    switch (event->fxt) {
	    case NONE:
	        event->fxt = event->fxp = 0;
		break;
	    case FX_FAR_SETVIBRATO:
		vib = event->fxp;
		event->fxt = event->fxp = 0;
		break;
	    case FX_VIBRATO:
		event->fxp = (event->fxp << 4) + vib;
		break;
	    case FX_FAR_F_VSLIDE_UP:	/* Fine volume slide up */
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_VSLIDE_UP << 4);
		break;
	    case FX_VOLSLIDE:		/* Fine volume slide down */
		event->fxt = FX_EXTENDED;
		event->fxp |= (EX_F_VSLIDE_DN << 4);
		break;
	    case FX_TEMPO:
		event->fxp = 8 * 60 / event->fxp;
		break;
	    }
	}
	reportv(0, ".");
    }

    fread (sample_map, 1, 8, f);
    for (i = 1; i < 0x100; i <<= 1) {
	for (j = 0; j < 8; j++)
	    if (sample_map[j] & i)
		xxh->ins++;
    }
    xxh->smp = xxh->ins;

    INSTRUMENT_INIT ();

    /* Read and convert instruments and samples */
    reportv(0, "\nInstruments    : %d ", xxh->ins);

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	fread(&fih.name, 32, 1, f);	/* Instrument name */
	fih.length = read32l(f);	/* Length of sample (up to 64Kb) */
	fih.finetune = read8(f);	/* Finetune (unsuported) */
	fih.volume = read8(f);		/* Volume (unsuported?) */
	fih.loop_start = read32l(f);	/* Loop start */
	fih.loopend = read32l(f);	/* Loop end */
	fih.sampletype = read8(f);	/* 1=16 bit sample */
	fih.loopmode = read8(f);

	fih.length &= 0xffff;
	fih.loop_start &= 0xffff;
	fih.loopend &= 0xffff;
	xxih[i].nsm = !!(xxs[i].len = fih.length);
	xxs[i].lps = fih.loop_start;
	xxs[i].lpe = fih.loopend;
	xxs[i].flg = fih.sampletype ? WAVE_16_BITS : 0;
	xxs[i].flg |= fih.loopmode ? WAVE_LOOPING : 0;
	xxi[i][0].vol = 0xff; /* fih.volume; */
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, fih.name, 24);

	if ((V(1)) && (strlen((char *)xxih[i].name) || xxs[i].len))
	    report ("\n[%2X] %-32.32s %04x %04x %04x %c V%02x ",
		i, fih.name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		fih.loopmode ? 'L' : ' ', xxi[i][0].vol);
	if (sample_map[i / 8] & (1 << (i % 8)))
	    xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	reportv(0, ".");
    }
    reportv(0, "\n");

    xmp_ctl->volbase = 0xff;

    return 0;
}

