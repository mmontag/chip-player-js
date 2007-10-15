/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: 669_load.c,v 1.9 2007-10-15 23:37:23 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"


static int ssn_test (FILE *, char *);
static int ssn_load (struct xmp_mod_context *, FILE *);

struct xmp_loader_info ssn_loader = {
    "669",
    "Composer 669",
    ssn_test,
    ssn_load
};

static int ssn_test(FILE *f, char *t)
{
    uint16 id;

    id = read16b(f);
    if (id != 0x6966 && id != 0x4a4e)
	return -1;

    fseek(f, 238, SEEK_CUR);
    if (read8(f) != 0xff)
	return -1;

    read_title(f, t, 0);

    return 0;
}



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


static int ssn_load(struct xmp_mod_context *m, FILE *f)
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

    m->xxh->chn = 8;
    m->xxh->ins = sfh.nos;
    m->xxh->pat = sfh.nop;
    m->xxh->trk = m->xxh->chn * m->xxh->pat;
    for (i = 0; i < 128; i++)
	if (sfh.order[i] > sfh.nop)
	    break;
    m->xxh->len = i;
    memcpy (m->xxo, sfh.order, m->xxh->len);
    m->xxh->tpo = 6;
    m->xxh->bpm = 0x50;
    m->xxh->smp = m->xxh->ins;
    m->xxh->flg |= XXM_FLG_LINEAR;

    strcpy(m->type, strncmp((char *)sfh.marker, "if", 2) ?
				"669 (UNIS 669)" : "669 (Composer 669)");

    MODULE_INFO ();

    if (V (0)) {
	report ("| %-36.36s\n", sfh.message);
	report ("| %-36.36s\n", sfh.message + 36);
	report ("| %-36.36s\n", sfh.message + 72);
    }

    /* Read and convert instruments and samples */

    INSTRUMENT_INIT ();

    if (V (0))
	report ("Instruments    : %d\n", m->xxh->pat);

    if (V (1))
	report ("     Instrument     Len  LBeg LEnd L\n");

    for (i = 0; i < m->xxh->ins; i++) {
	m->xxi[i] = calloc (sizeof (struct xxm_instrument), 1);

	fread (&sih.name, 13, 1, f);		/* ASCIIZ instrument name */
	sih.length = read32l(f);		/* Instrument size */
	sih.loop_start = read32l(f);		/* Instrument loop start */
	sih.loopend = read32l(f);		/* Instrument loop end */

	m->xxih[i].nsm = !!(m->xxs[i].len = sih.length);
	m->xxs[i].lps = sih.loop_start;
	m->xxs[i].lpe = sih.loopend >= 0xfffff ? 0 : sih.loopend;
	m->xxs[i].flg = m->xxs[i].lpe ? WAVE_LOOPING : 0;	/* 1 == Forward loop */
	m->xxi[i][0].vol = 0x40;
	m->xxi[i][0].pan = 0x80;
	m->xxi[i][0].sid = i;

	copy_adjust(m->xxih[i].name, sih.name, 13);

	if ((V (1)) && (strlen ((char *) m->xxih[i].name) || (m->xxs[i].len > 2)))
	    report ("[%2X] %-14.14s %04x %04x %04x %c\n", i,
		m->xxih[i].name, m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
		m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ');
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", m->xxh->pat);
    for (i = 0; i < m->xxh->pat; i++) {
	PATTERN_ALLOC (i);
	m->xxp[i]->rows = 64;
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
	report ("\nStored samples : %d ", m->xxh->smp);
    for (i = 0; i < m->xxh->ins; i++) {
	if (m->xxs[i].len <= 2)
	    continue;
	xmp_drv_loadpatch (f, m->xxi[i][0].sid, xmp_ctl->c4rate,
	    XMP_SMP_UNS, &m->xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < m->xxh->chn; i++)
	m->xxc[i].pan = (i % 2) * 0xff;

    return 0;
}
