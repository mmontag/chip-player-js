/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: 669_load.c,v 1.4 2007-08-14 03:35:02 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


struct ssn_file_header {
    uint8 marker[2];		/* 'if'=standard, 'JN'=extended */
    uint8 message[108];		/* Song message */
    uint8 nos;			/* Number of samples (0-64) */
    uint8 nop;			/* Number of patterns (0-128) */
    uint8 loop;			/* Loop order number */
    uint8 order[128];		/* Order list */
    uint8 tempo[128];		/* Tempo list for patterns */
    uint8 pbrk[128];		/* Break list for patterns */
};

struct ssn_instrument_header {
    uint8 name[13];		/* ASCIIZ instrument name */
    uint32 length;		/* Instrument length */
    uint32 loop_start;		/* Instrument loop start */
    uint32 loopend;		/* Instrument loop end */
};


#define NONE 0xff

/* Effects bug fixed by Miod Vallat <miodrag@multimania.com> */

static uint8 fx[] = {
    FX_PORTA_UP,	FX_PORTA_DN,
    FX_TONEPORTA,	FX_EXTENDED,
    FX_VIBRATO,		FX_TEMPO,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE,
    NONE,		NONE
};


int ssn_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct ssn_file_header sfh;
    struct ssn_instrument_header sih;
    uint8 ev[3];

    LOAD_INIT ();

    fread(&sfh.marker, 2, 1, f);	/* 'if'=standard, 'JN'=extended */
    fread(&sfh.message, 108, 1, f);	/* Song message */
    sfh.nos = read8(f);			/* Number of samples (0-64) */
    sfh.nop = read8(f);			/* Number of patterns (0-128) */
    sfh.loop = read8(f);		/* Loop order number */
    fread(&sfh.order, 128, 1, f);	/* Order list */
    fread(&sfh.tempo, 128, 1, f);	/* Tempo list for patterns */
    fread(&sfh.pbrk, 128, 1, f);	/* Break list for patterns */

    if (strncmp ((char *) sfh.marker, "if", 2) && strncmp ((char *) sfh.marker, "JN", 2))
	return -1;
    if (sfh.order[127] != 0xff)
	return -1;

    xxh->chn = 8;
    xxh->ins = sfh.nos;
    xxh->pat = sfh.nop;
    xxh->trk = xxh->chn * xxh->pat;
    for (i = 0; i < 128; i++)
	if (sfh.order[i] > sfh.nop)
	    break;
    xxh->len = i;
    memcpy (xxo, sfh.order, xxh->len);
    xxh->tpo = 6;
    xxh->bpm = 0x50;
    xxh->smp = xxh->ins;
    xxh->flg |= XXM_FLG_LINEAR;

    strcpy (xmp_ctl->type, strncmp ((char *) sfh.marker, "if", 2) ?
	"Extended 669 (UNIS)" : "669");

    MODULE_INFO ();

    if (V (0)) {
	report ("| %-36.36s\n", sfh.message);
	report ("| %-36.36s\n", sfh.message + 36);
	report ("| %-36.36s\n", sfh.message + 72);
    }

    /* Read and convert instruments and samples */

    INSTRUMENT_INIT ();

    if (V (0))
	report ("Instruments    : %d\n", xxh->pat);

    if (V (1))
	report ("     Instrument     Len  LBeg LEnd L\n");

    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	fread (&sih.name, 13, 1, f);		/* ASCIIZ instrument name */
	sih.length = read32l(f);		/* Instrument size */
	sih.loop_start = read32l(f);		/* Instrument loop start */
	sih.loopend = read32l(f);		/* Instrument loop end */

	xxih[i].nsm = !!(xxs[i].len = sih.length);
	xxs[i].lps = sih.loop_start;
	xxs[i].lpe = sih.loopend >= 0xfffff ? 0 : sih.loopend;
	xxs[i].flg = xxs[i].lpe ? WAVE_LOOPING : 0;	/* 1 == Forward loop */
	xxi[i][0].vol = 0x40;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, sih.name, 13);

	if ((V (1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
	    report ("[%2X] %-14.14s %04x %04x %04x %c\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ');
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);
    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	EVENT (i, 0, 0).f2t = FX_TEMPO;
	EVENT (i, 0, 0).f2p = sfh.tempo[i];
	EVENT (i, 1, sfh.pbrk[i]).f2t = FX_BREAK;
	for (j = 0; j < 64 * 8; j++) {
	    event = &EVENT (i, j % 8, j / 8);
	    fread (&ev, 1, 3, f);
	    if ((ev[0] & 0xfe) != 0xfe) {
		event->note = 1 + 24 + (ev[0] >> 2);
		event->ins = 1 + MSN (ev[1]) + ((ev[0] & 0x03) << 4);
	    }
	    if (ev[0] != 0xff)
		event->vol = (LSN (ev[1]) << 2) + 1;
	    if (ev[2] + 1) {
		event->fxt = fx[MSN (ev[2])];
		event->fxp = LSN (ev[2]);

		switch (event->fxt) {
		case NONE:
		    event->fxp = 0;
		    break;
		case FX_PORTA_UP:
		case FX_PORTA_DN:
		case FX_TONEPORTA:
		    event->fxp <<= 1;
		    break;
		case FX_VIBRATO:
		    event->fxp |= 0x80;		/* Wild guess */
		    break;
		case FX_EXTENDED:
		    event->fxp = (EX_FINETUNE << 4) | 3;	/* Wild guess */
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
	if (xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate,
	    XMP_SMP_UNS, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = (i % 2) * 0xff;

    return 0;
}
