/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "stm.h"
#include "period.h"

#define FX_NONE		0xff

/*
 * Skaven's note from http://www.futurecrew.com/skaven/oldies_music.html
 *
 * FYI for the tech-heads: In the old Scream Tracker 2 the Arpeggio command
 * (Jxx), if used in a single row with a 0x value, caused the note to skip
 * the specified amount of halftones upwards halfway through the row. I used
 * this in some songs to give the lead some character. However, when played
 * in ModPlug Tracker, this effect doesn't work the way it did back then.
 */

static uint8 fx[] =
{
    FX_NONE,		FX_TEMPO,
    FX_JUMP,		FX_BREAK,
    FX_VOLSLIDE,	FX_PORTA_DN,
    FX_PORTA_UP,	FX_TONEPORTA,
    FX_VIBRATO,		FX_TREMOR,
    FX_ARPEGGIO
};


int stm_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct stm_file_header sfh;
    uint8 b;
    int bmod2stm = 0;

    LOAD_INIT ();

    fread (&sfh, 1, sizeof (sfh), f);

    if (!strncmp ((char *) sfh.magic, "BMOD2STM", 8))
	bmod2stm = 1;

    if (strncmp ((char *) sfh.magic, "!Scream!", 8) && !bmod2stm)
	return -1;
    if (sfh.type != STM_TYPE_MODULE)
	return -1;
    if (sfh.vermaj < 1)		/* We don't want STX files */
	return -1;

    xxh->pat = sfh.patterns;
    xxh->trk = xxh->pat * xxh->chn;
    xxh->tpo = MSN (sfh.tempo);
    xxh->smp = xxh->ins;
    xmp_ctl->c4rate = C4_NTSC_RATE;

    strncpy (xmp_ctl->name, sfh.name, 20);
    sprintf (xmp_ctl->type, "!Scream! (STM)");
    if (bmod2stm)
	sprintf (tracker_name, "BMOD2STM (%d.%02d)", sfh.vermaj, sfh.vermin);
    else
	sprintf (tracker_name, "Scream Tracker %d.%02d", sfh.vermaj, sfh.vermin);

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    if (V (1))
	report ("     Sample name    Len  LBeg LEnd L Vol C2Spd\n");

    /* Read and convert instruments and samples */
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	L_ENDIAN16 (sfh.ins[i].length);
	L_ENDIAN16 (sfh.ins[i].loopbeg);
	L_ENDIAN16 (sfh.ins[i].loopend);
	L_ENDIAN16 (sfh.ins[i].c2spd);
	xxih[i].nsm = !!(xxs[i].len = sfh.ins[i].length);
	xxs[i].lps = sfh.ins[i].loopbeg;
	xxs[i].lpe = sfh.ins[i].loopend;
	if (xxs[i].lpe == 0xffff)
	    xxs[i].lpe = 0;
	xxs[i].flg = xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = sfh.ins[i].volume;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	strncpy ((char *) xxih[i].name, sfh.ins[i].name, 12);
	str_adj ((char *) xxih[i].name);
	if ((V (1)) &&
	    (strlen ((char *) xxih[i].name) || (xxs[i].len > 1))) {
	    report ("[%2X] %-14.14s %04x %04x %04x %c V%02x %5d\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, sfh.ins[i].c2spd);
	}

	sfh.ins[i].c2spd = 8363 * sfh.ins[i].c2spd / 8448;
	c2spd_to_note (sfh.ins[i].c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
    }

    fread (xxo, 1, 128, f);

    for (i = 0; i < 128; i++)
	if (xxo[i] >= xxh->pat)
	    break;

    xxh->len = i;

    if (V (0))
	report ("Module length  : %d patterns\n", xxh->len);

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);
    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (&b, 1, 1, f);
	    memset (event, 0, sizeof (struct xxm_event));
	    switch (b) {
	    case 251:
	    case 252:
	    case 253:
		break;
	    case 255:
		b = 0;
	    default:
		event->note = b ? 1 + LSN (b) + 12 * (2 + MSN (b)) : 0;
		fread (&b, 1, 1, f);
		event->vol = b & 0x07;
		event->ins = (b & 0xf8) >> 3;
		fread (&b, 1, 1, f);
		event->vol += (b & 0xf0) >> 1;
		if (event->vol > 0x40)
		    event->vol = 0;
		else
		    event->vol++;
		event->fxt = fx[LSN (b)];
		fread (&b, 1, 1, f);
		event->fxp = b;
		switch (event->fxt) {
		case FX_TEMPO:
		    event->fxp = MSN (event->fxp);
		    break;
		case FX_NONE:
		    event->fxp = event->fxt = 0;
		    break;
		}
	    }
	}
	if (V (0))
	    report (".");
    }

    /* Read samples */
    if (V (0))
	report ("\nStored samples : %d ", xxh->smp);
    for (i = 0; i < xxh->ins; i++) {
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
	    &xxs[xxi[i][0].sid], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    xmp_ctl->fetch |= XMP_CTL_VSALL | XMP_MODE_ST3;

    return 0;
}
