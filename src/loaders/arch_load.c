/* Archimedes Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: arch_load.c,v 1.5 2007-08-23 13:47:40 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "period.h"

static int year, month, day;
static int pflag, sflag, max_ins;
static uint8 ster[8], rows[64];
static int8 *sbuf[36];


static int8 table[128] = {
	/*   0 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*   8 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  16 */	  0,   0,   0,   0,   0,   0,   0,   0,
	/*  24 */	  1,   1,   1,   1,   1,   1,   1,   1,
	/*  32 */	  1,   1,   1,   1,   2,   2,   2,   2,
	/*  40 */	  2,   2,   2,   2,   3,   3,   3,   3,
	/*  48 */	  3,   3,   4,   4,   4,   4,   5,   5,
	/*  56 */	  5,   5,   6,   6,   6,   6,   7,   7,
	/*  64 */	  7,   8,   8,   9,   9,  10,  10,  11,
	/*  72 */	 11,  12,  12,  13,  13,  14,  14,  15,
	/*  80 */	 15,  16,  17,  18,  19,  20,  21,  22,
	/*  88 */	 23,  24,  25,  26,  27,  28,  29,  30,
	/*  96 */	 31,  33,  34,  36,  38,  40,  42,  44,
	/* 104 */	 46,  48,  50,  52,  54,  56,  58,  60,
	/* 112 */	 62,  65,  68,  72,  77,  80,  84,  91,
	/* 120 */	 95,  98, 103, 109, 114, 120, 126, 127
};

static void fix_effect(struct xxm_event *e)
{
	switch (e->fxt) {
	case 0x00:			/* 00 xy Normal play or Arpeggio */
		e->fxt = FX_ARPEGGIO;
		/* x: first halfnote to add
		   y: second halftone to subtract */
		break;
	case 0x01:			/* 01 xx Slide Up */
		e->fxt = FX_PORTA_UP;
		break;
	case 0x02:			/* 02 xx Slide Down */
		e->fxt = FX_PORTA_DN;
		break;
	case 0x0b:			/* 0B xx Break Pattern */
		e->fxt = FX_BREAK;
		break;
	case 0x0e:			/* 0E xy Set Stereo */
		e->fxt = e->fxp = 0;
		/* y: stereo position (1-7,ignored). 1=left 4=center 7=right */
		break;
	case 0x10:			/* 10 xx Volume Slide Up */
		e->fxt = FX_VOLSLIDE_UP;
		break;
	case 0x11:			/* 11 xx Volume Slide Down */
		e->fxt = FX_VOLSLIDE_DN;
		break;
	case 0x13:			/* 13 xx Position Jump */
		e->fxt = FX_JUMP;
		break;
	case 0x15:			/* 15 xy Line Jump. (not in manual) */
		e->fxt = e->fxp = 0;
		/* Jump to line 10*x+y in same pattern. (10*x+y>63 ignored) */
		break;
	case 0x1c:			/* 1C xy Set Speed */
		e->fxt = FX_TEMPO;
		break;
	case 0x1f:			/* 1F xx Set Volume */
		e->vol = e->fxp;
		e->fxp = e->fxt = 0;
		break;
	default:
		e->fxt = e->fxp = 0;
	}
}

static void get_tinf(int size, FILE *f)
{
	int x;

	x = read8(f);
	year = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);
	x = read8(f);
	year += ((x & 0xf0) >> 4) * 1000 + (x & 0x0f) * 100;

	x = read8(f);
	month = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);

	x = read8(f);
	day = ((x & 0xf0) >> 4) * 10 + (x & 0x0f);
}

static void get_mvox(int size, FILE *f)
{
	xxh->chn = read32l(f);
}

static void get_ster(int size, FILE *f)
{
	fread(ster, 1, 8, f);
}

static void get_mnam(int size, FILE *f)
{
	fread(xmp_ctl->name, 1, 32, f);
}

static void get_anam(int size, FILE *f)
{
	fread(author_name, 1, 32, f);
}

static void get_mlen(int size, FILE *f)
{
	xxh->len = read32l(f);
}

static void get_pnum(int size, FILE *f)
{
	xxh->pat = read32l(f);
}

static void get_plen(int size, FILE *f)
{
	fread(rows, 1, 64, f);
}

static void get_sequ(int size, FILE *f)
{
	fread(xxo, 1, 128, f);

	strcpy(xmp_ctl->type, "MUSX (Archimedes Tracker)");

	MODULE_INFO();
	reportv(0, "Creation date  : %02d/%02d/%04d\n", day, month, year);
}

static void get_patt(int size, FILE *f)
{
	static int i = 0;
	int j, k;
	struct xxm_event *event;

	if (!pflag) {
		reportv(0, "Stored patterns: %d ", xxh->pat);
	pflag = 1;
		i = 0;
		xxh->trk = xxh->pat * xxh->chn;
		PATTERN_INIT();
	}

	PATTERN_ALLOC(i);
	xxp[i]->rows = rows[i];
	TRACK_ALLOC(i);

	for (j = 0; j < rows[i]; j++) {
		for (k = 0; k < xxh->chn; k++) {
			event = &EVENT(i, k, j);

			event->fxp = read8(f);
			event->fxt = read8(f);
			event->ins = read8(f);
			event->note = read8(f);

			if (event->note)
				event->note += 36;

			fix_effect(event);
		}
	}

	i++;
	reportv(0, ".");
}

/*
 * From the Audio File Formats (version 2.5)
 * Submitted-by: Guido van Rossum <guido@cwi.nl>
 * Last-modified: 27-Aug-1992
 *
 * The Acorn Archimedes uses a variation on U-LAW with the bit order
 * reversed and the sign bit in bit 0.  Being a 'minority' architecture,
 * Arc owners are quite adept at converting sound/image formats from
 * other machines, and it is unlikely that you'll ever encounter sound in
 * one of the Arc's own formats (there are several).
 */

static void convert(int8 *buf, int len)
{
	int i;
	uint8 x;

	for (i = 0; i < len; i++) {
		x = buf[i];
		buf[i] = table[x >> 1];
		if (x & 0x01)
			buf[i] *= -1;
	}
}

static void get_samp(int size, FILE *f)
{
	static int i = 0;

	if (!sflag) {
		xxh->smp = xxh->ins = 36;
		INSTRUMENT_INIT();
		reportv(0, "\nInstruments    : %d ", xxh->ins);
	        reportv(1, "\n     Instrument name      Len   LBeg  LEnd  L Vol");
		sflag = 1;
		max_ins = 0;
		i = 0;
	}

	xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
	read32l(f);	/* SNAM */
	read32l(f);
	fread(xxih[i].name, 1, 20, f);
	read32l(f);	/* SVOL */
	read32l(f);
	xxi[i][0].vol = read32l(f) >> 2;
	read32l(f);	/* SLEN */
	read32l(f);
	xxs[i].len = read32l(f);
	read32l(f);	/* ROFS */
	read32l(f);
	xxs[i].lps = read32l(f);
	read32l(f);	/* RLEN */
	read32l(f);
	xxs[i].lpe = read32l(f);
	read32l(f);	/* SDAT */
	read32l(f);
	read32l(f);	/* 0x00000000 */

	xxih[i].nsm = 1;
	xxi[i][0].sid = i;
	xxi[i][0].pan = 0x80;

	if (xxs[i].lpe > 2) {
		xxs[i].flg = WAVE_LOOPING;
		xxs[i].lpe = xxs[i].lps + xxs[i].lpe;
	}

	sbuf[i] = malloc(xxs[i].len);
	fread(sbuf[i], 1, xxs[i].len, f);
	convert(sbuf[i], xxs[i].len);

	xmp_drv_loadpatch(NULL, xxi[i][0].sid, xmp_ctl->c4rate,XMP_SMP_NOLOAD,
					&xxs[xxi[i][0].sid], (char *)sbuf[i]);

	if (strlen((char *)xxih[i].name) || xxs[i].len > 0) {
		if (V(1))
			report("\n[%2X] %-20.20s %05x %05x %05x %c V%02x",
				i, xxih[i].name,
				xxs[i].len,
				xxs[i].lps,
				xxs[i].lpe,
				xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				xxi[i][0].vol);
		else
			report(".");
	}

	i++;
	max_ins++;
}

int arch_load(FILE *f)
{
	char magic[4];
	int i;

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "MUSX", 4))
		return -1;

	fseek(f, 8, SEEK_SET);

	pflag = sflag = 0;

	/* IFF chunk IDs */
	iff_register("TINF", get_tinf);
	iff_register("MVOX", get_mvox);
	iff_register("STER", get_ster);
	iff_register("MNAM", get_mnam);
	iff_register("ANAM", get_anam);
	iff_register("MLEN", get_mlen);
	iff_register("PNUM", get_pnum);
	iff_register("PLEN", get_plen);
	iff_register("SEQU", get_sequ);
	iff_register("PATT", get_patt);
	iff_register("SAMP", get_samp);

	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	reportv(0, "\n");

	iff_release();

	for (i = 0; i < max_ins; i++)
		free(sbuf[i]);

	return 0;
}

