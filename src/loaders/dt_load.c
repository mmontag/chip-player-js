/* Digital Tracker DTM loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dt_load.c,v 1.4 2007-08-10 03:52:51 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include "load.h"
#include "iff.h"
#include "period.h"

static int pflag, sflag;
static int realpat;

static void get_patt(int size, FILE *f)
{
	xxh->chn = read16b(f);
	realpat = read16b(f);
	xxh->trk = xxh->chn * xxh->pat;
}

static void get_inst(int size, FILE *f)
{
	int i, c2spd;
	uint8 name[30];

	xxh->ins = xxh->smp = read16b(f);
	reportv(0, "Instruments    : %d ", xxh->ins);

	INSTRUMENT_INIT();

	for (i = 0; i < xxh->ins; i++) {
		int c, g;

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		read32b(f);
		xxih[i].nsm = !!(xxs[i].len = read32b(f));
		c = read8(f);
		xxi[i][0].vol = read8(f);
		xxs[i].lps = read32b(f);
		xxs[i].lpe = xxs[i].lps + read32b(f);

		fread(name, 22, 1, f);
		copy_adjust(xxih[i].name, name, 22);

		g = read16b(f);
		read16b(f);		/* 0x0030 */
		read32b(f);		/* 0x00000000 */
		c2spd = read16b(f);
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
		xxi[i][0].sid = i;

		if (strlen((char *)xxih[i].name) || xxs[i].len > 0) {
			if (V(1))
				report("\n[%2X] %-22.22s %05x %05x %05x V%02x %02x %04x %5d",
					i, xxih[i].name,
					xxs[i].len,
					xxs[i].lps,
					xxs[i].lpe,
					xxi[i][0].vol,
					c,
					g, c2spd);
			else
				report(".");
		}
	}
	reportv(0, "\n");
}

static void get_dapt(int size, FILE *f)
{
	int pat, i, j, k;
	struct xxm_event *event;
	static int last_pat = 0;
	int rows;

	if (!pflag) {
		reportv(0, "Stored patterns: %d ", realpat);
		pflag = 1;
		PATTERN_INIT();
	}

	read32b(f);	/* 0xffffffff */
	i = pat = read16b(f);
	rows = read16b(f);

	for (i = last_pat; i <= pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = rows;
		TRACK_ALLOC(i);
	}
	last_pat = pat + 1;

	for (j = 0; j < 64; j++) {
		for (k = 0; k < xxh->chn; k++) {
			uint8 a, b, c, d;
			int x;
			event = &EVENT(pat, k, j);
			a = read8(f);
			b = read8(f);
			c = read8(f);
			d = read8(f);
			if (a)
				event->note = 12 * (a >> 4) + (a & 0x0f);
			x = ((int)b << 4) + (c >> 4);
			if (x)
				event->ins = x;
			event->fxt = c & 0xf;
			event->fxp = d;
		}
	}

	reportv(0, ".");
}

static void get_dait(int size, FILE *f)
{
	static int i = 0;

	if (!sflag) {
		reportv(0, "\nStored samples : %d ", xxh->smp);
		sflag = 1;
		i = 0;
	}

	if (size > 2) {
		xmp_drv_loadpatch(f, xxi[i][0].sid, xmp_ctl->c4rate, 0,
						&xxs[xxi[i][0].sid], NULL);
		reportv(0, ".");
	}

	i++;
}

int dt_load(FILE *f)
{
	int i, maxpat;
	char magic[4];

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "D.T.", 4))
		return -1;

	read32b(f);
	read32b(f);
	read32b(f);
	read16b(f);
	read32b(f);

	fread(xmp_ctl->name, 18, 1, f);
	read16b(f);
	fread(magic, 1, 4, f);
	if (strncmp(magic, "S.Q.", 4))
		return -1;

	read16b(f);
	read16b(f);
	xxh->len = read16b(f);
	read16b(f);
	read16b(f);
	read16b(f);

	for (maxpat = i = 0; i < 128; i++) {
		xxo[i] = read8(f);
		if (xxo[i] > maxpat)
			maxpat = xxo[i];
	}
	xxh->pat = maxpat + 1;

	strcpy(xmp_ctl->type, "Digital Tracker module");
	MODULE_INFO();

	pflag = sflag = 0;
	
	/* IFF chunk IDs */
	iff_register("PATT", get_patt);
	iff_register("INST", get_inst);
	iff_register("DAPT", get_dapt);
	iff_register("DAIT", get_dait);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	reportv(0, "\n");

	iff_release();

	return 0;
}

