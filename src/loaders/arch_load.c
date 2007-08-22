/* Archimedes Tracker module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: arch_load.c,v 1.1 2007-08-22 13:26:26 cmatsuoka Exp $
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
static int rows, pflag, sflag;
static uint8 ster[8];


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
	rows = read32l(f);
}

static void get_sequ(int size, FILE *f)
{
	fread(xxo, 1, 128, f);

	strcpy(xmp_ctl->type, "Archimedes Tracker");

	MODULE_INFO();
	reportv(0, "Creation date  : %02d/%02d/%04d\n", day, month, year);
}

static void get_patt(int size, FILE *f)
{
	if (!pflag) {
		reportv(0, "Stored patterns: %d ", xxh->pat);
		pflag = 1;
		xxh->trk = xxh->pat * xxh->chn;
		PATTERN_INIT();
	}
}

static void get_samp(int size, FILE *f)
{
	if (!sflag) {
		reportv(0, "\nStored samples : %d ", xxh->smp);
		sflag = 1;
	}
}

int arch_load(FILE *f)
{
	char magic[4];

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

	return 0;
}

