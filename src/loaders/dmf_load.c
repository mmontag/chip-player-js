/* X-Tracker DMF loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dmf_load.c,v 1.1 2007-08-07 22:11:19 cmatsuoka Exp $
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

static void get_sequ(int size, FILE *f)
{
}

static void get_patt(int size, FILE *f)
{
}

static void get_inst(int size, FILE *f)
{
}

static void get_smpi(int size, FILE *f)
{
}

static void get_smpd(int size, FILE *f)
{
}

int dmf_load(FILE *f)
{
	char magic[4];
	int i, j;

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "DDMF", 4))
		return -1;

	xxh->smp = xxh->ins = 0;

	/* IFF chunk IDs */
	iff_register("SEQU", get_sequ);
	iff_register("PATT", get_patt);
	iff_register("INST", get_inst);
	iff_register("SMPI", get_smpi);
	iff_register("SMPD", get_smpd);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	iff_release();

	xxh->trk = xxh->pat * xxh->chn;

	strcpy (xmp_ctl->type, "Delusion Digital Music File");

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	if (V(0)) {
	    report("Stored patterns: %d\n", xxh->pat);
	    report("Stored samples : %d", xxh->smp);
	}



	if (V(0))
		report("\n");

	return 0;
}

