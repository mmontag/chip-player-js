/* Extended Module Player
 * Copyright (C) 1996-2006 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: fnk_load.c,v 1.3 2007-10-14 19:08:14 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* INCOMPLETE LOADER!!! NO EFFECTS! */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

#define MAGIC_Funk	MAGIC4('F','u','n','k')


static int fnk_test (FILE *, char *);
static int fnk_load (FILE *);

struct xmp_loader_info fnk_loader = {
    "FNK",
    "Funktracker",
    fnk_test,
    fnk_load
};

static int fnk_test(FILE *f, char *t)
{
    uint8 a, b;

    if (read32b(f) != MAGIC_Funk)
	return -1;

    fseek(f, 8, SEEK_CUR);
    a = read8(f);    
    b = read8(f);    

    if (a != 'F' || b != '2')
	return -1;

    read_title(f, t, 0);

    return 0;
}


struct fnk_instrument {
    uint8 name[19];		/* ASCIIZ instrument name */
    uint32 loop_start;		/* Instrument loop start */
    uint32 length;		/* Instrument length */
    uint8 volume;		/* Volume (0-255) */
    uint8 pan;			/* Pan (0-255) */
    uint8 shifter;		/* Portamento and offset shift */
    uint8 wavsallorm;		/* Vibrato and tremolo wavsallorms */
    uint8 retrig;		/* Retrig and arpeggio speed */
};

struct fnk_header {
    uint8 marker[4];		/* 'Funk' */
    uint8 info[4];		/* */
    uint32 filesize;		/* File size */
    uint8 format[4];		/* F2xx, Fkxx or Fvxx */
    uint8 loop;			/* Loop order number */
    uint8 order[256];		/* Order list */
    uint8 pbrk[128];		/* Break list for patterns */
    struct fnk_instrument fih[64];	/* Instruments */
};


#if 0

#define FX_NONE	0xff

static uint8 fx[] = {
    FX_NONE,		FX_NONE,
    FX_NONE,		FX_NONE,
    FX_NONE,		FX_NONE,
    FX_NONE,		FX_NONE,
    FX_NONE,		FX_NONE,
    FX_PORTA_UP,	FX_PORTA_DN,
    FX_TONEPORTA,	FX_EXTENDED,
    FX_VIBRATO,		FX_NONE,
    FX_NONE,		FX_NONE,
};
#endif


static int fnk_load (FILE * f)
{
    int i, j;
    struct xxm_event *event;
    struct fnk_header ffh;
    uint8 ev[3];

    LOAD_INIT ();

    fread(&ffh.marker, 4, 1, f);
    fread(&ffh.info, 4, 1, f);
    ffh.filesize = read32l(f);
    fread(&ffh.format, 4, 1, f);
    ffh.loop = read8(f);
    fread(&ffh.order, 256, 1, f);
    fread(&ffh.pbrk, 128, 1, f);

    for (i = 0; i < 64; i++) {
	fread(&ffh.fih[i].name, 19, 1, f);
	ffh.fih[i].loop_start = read32l(f);
	ffh.fih[i].length = read32l(f);
	ffh.fih[i].volume = read8(f);
	ffh.fih[i].pan = read8(f);
	ffh.fih[i].shifter = read8(f);
	ffh.fih[i].wavsallorm = read8(f);
	ffh.fih[i].retrig = read8(f);
    }

    if (strncmp ((char *) ffh.marker, "Funk", 4) ||
	strncmp ((char *) ffh.format, "F2", 2))
	return -1;

    xxh->chn = (ffh.format[2] < '0') || (ffh.format[2] > '9') ||
	(ffh.format[3] < '0') || (ffh.format[3] > '9') ? 8 :
	(ffh.format[2] - '0') * 10 + ffh.format[3] - '0';

    xxh->ins = 64;

    for (i = 0; i < 256 && ffh.order[i] != 0xff; i++)
	if (ffh.order[i] > xxh->pat)
	    xxh->pat = i;

    xxh->len = i;
    xxh->trk = xxh->chn * xxh->pat;
    memcpy (xxo, ffh.order, xxh->len);
    xxh->tpo = 6;
    xxh->bpm = ffh.info[3] >> 1;
    if (xxh->bpm & 64)
	xxh->bpm = -(xxh->bpm & 63);
    xxh->bpm += 125;
    xxh->smp = xxh->ins;
    strcpy(xmp_ctl->type, "Funktracker");

    MODULE_INFO ();

    INSTRUMENT_INIT ();

    /* Convert instruments */
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	xxih[i].nsm = !!(xxs[i].len = ffh.fih[i].length);
	xxs[i].lps = ffh.fih[i].loop_start;
	if (xxs[i].lps == -1)
	    xxs[i].lps = 0;
	xxs[i].lpe = ffh.fih[i].length;
	xxs[i].flg = ffh.fih[i].loop_start != -1 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = ffh.fih[i].volume;
	xxi[i][0].pan = ffh.fih[i].pan;
	xxi[i][0].sid = i;

	copy_adjust(xxih[i].name, ffh.fih[i].name, 19);

	if ((V (1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 2)))
	    report ("[%2X] %-20.20s %04x %04x %04x %c V%02x P%02x\n", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe,
		xxs[i].flg & WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, xxi[i][0].pan);
    }

    PATTERN_INIT ();

    /* Read and convert patterns */
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);
    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	xxp[i]->rows = 64;
	TRACK_ALLOC (i);
	EVENT (i, 1, ffh.pbrk[i]).f2t = FX_BREAK;
	for (j = 0; j < 64 * xxh->chn; j++) {
	    event = &EVENT (i, j % xxh->chn, j / xxh->chn);
	    fread (&ev, 1, 3, f);
	    switch (ev[0] >> 2) {
	    case 0x3f:
	    case 0x3e:
	    case 0x3d:
		break;
	    default:
		event->note = 25 + (ev[0] >> 2);
		event->ins = 1 + MSN (ev[1]) + ((ev[0] & 0x03) << 4);
		event->vol = ffh.fih[event->ins - 1].volume;
	    }
#if 0
	    if (ev[2] + 1) {
		event->fxt = fx[MSN (ev[2])];
		event->fxp = fx[LSN (ev[2])];
		if (!event->fxp)
		    event->fxp = 0;
		switch (event->fxt) {
		case FX_ARPEGGIO:
		    event->fxp = 0;
		    break;
		case FX_VIBRATO:
		    event->fxp |= 0x80;		/* Wild guess */
		    break;
		case FX_EXTENDED:
		    event->fxp = 0x53;		/* Wild guess */
		    break;
		}
	    }
#endif
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
	xmp_drv_loadpatch (f, xxi[i][0].sid, xmp_ctl->c4rate, 0, &xxs[i], NULL);
	if (V (0))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++)
	xxc[i].pan = (i % 2) * 0xff;
    xmp_ctl->volbase = 0x100;

    return 0;
}
