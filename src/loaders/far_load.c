/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
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

    fread (&ffh, 1, sizeof (ffh), f);
    if (strncmp ((char *) ffh.magic, "FAR", 3) || (ffh.magic[3] != 0xfe))
	return -1;

    L_ENDIAN16 (ffh.textlen);

    fseek (f, ffh.textlen, SEEK_CUR);		/* Skip song text */
    fread (&ffh2, 1, sizeof (ffh2), f);

    xxh->chn = 16;
    /*xxh->pat=ffh2.patterns; (Error in specs? --claudio) */
    xxh->len = ffh2.songlen;
    xxh->tpo = 6;
    xxh->bpm = 8 * 60 / ffh.tempo;
    memcpy (xxo, ffh2.order, xxh->len);

    for (xxh->pat = i = 0; i < 256; i++) {
	L_ENDIAN16 (ffh2.patsize[i]);
	if (ffh2.patsize[i])
	    xxh->pat = i + 1;
    }

    xxh->trk = xxh->chn * xxh->pat;

    strncpy (xmp_ctl->name, ffh.name, 40);
    sprintf (xmp_ctl->type, "Farandole Composer %d.%d",
	MSN (ffh.version), LSN (ffh.version));

    MODULE_INFO ();

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0)) {
	report ("Comment bytes  : %d\n", ffh.textlen);
	report ("Stored patterns: %d ", xxh->pat);
    }

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	L_ENDIAN16 (ffh2.patsize[i]);
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
	if (V (0))
	    report (".");
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
    if (V (0))
	report ("\nInstruments    : %d ", xxh->ins);
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	fread (&fih, 1, sizeof (fih), f);
	L_ENDIAN32 (fih.length);
	L_ENDIAN32 (fih.loop_start);
	L_ENDIAN32 (fih.loopend);
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
	strncpy ((char *) xxih[i].name, fih.name, 24);
	fih.length = 0;
	str_adj ((char *) fih.name);
	if ((V (1)) && (strlen ((char *) fih.name) || xxs[i].len))
	    report ("\n[%2X] %-32.32s %04x %04x %04x %c V%02x ",
		i, fih.name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		fih.loopmode ? 'L' : ' ', xxi[i][0].vol);
	if (sample_map[i / 8] & (1 << (i % 8)))
	    xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    xmp_ctl->volbase = 0xff;

    return 0;
}

