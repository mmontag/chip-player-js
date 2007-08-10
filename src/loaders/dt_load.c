/* Digital Tracker DTM loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: dt_load.c,v 1.5 2007-08-10 11:49:32 cmatsuoka Exp $
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


static void get_d_t_(int size, FILE *f)
{
	read16b(f);	/* type */
	read16b(f);	/* 0xff then mono */
	read16b(f);	/* reserved */
	xxh->tpo = read16b(f);
	xxh->bpm = read16b(f);
	read32b(f);	/* undocumented */

	fread(xmp_ctl->name, 32, 1, f);
	strcpy(xmp_ctl->type, "Digital Tracker (DTM)");

	MODULE_INFO();
}

static void get_s_q_(int size, FILE *f)
{
	int i, maxpat;

	xxh->len = read16b(f);
	xxh->rst = read16b(f);
	read32b(f);	/* reserved */

	for (maxpat = i = 0; i < 128; i++) {
		xxo[i] = read8(f);
		if (xxo[i] > maxpat)
			maxpat = xxo[i];
	}
	xxh->pat = maxpat + 1;
}

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

	reportv(1, "\n     Instrument name        Len   LBeg  LSize LS Res Vol Fine C2Spd");
	for (i = 0; i < xxh->ins; i++) {
		int fine, replen, flag;

		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		read32b(f);		/* reserved */
		xxs[i].len = read32b(f);
		xxih[i].nsm = !!xxs[i].len;
		fine = read8s(f);	/* finetune */
		xxi[i][0].vol = read8(f);
		xxs[i].lps = read32b(f);
		replen = read32b(f);
		xxs[i].lpe = xxs[i].lps + replen - 1;
		xxs[i].flg = replen > 2 ?  WAVE_LOOPING : 0;

		fread(name, 22, 1, f);
		copy_adjust(xxih[i].name, name, 22);

		flag = read16b(f);	/* bit 0-7:resol 8:stereo */
		xxs[i].flg |= (flag & 0xff) > 8 ? WAVE_16_BITS : 0;

		read32b(f);		/* midi note (0x00300000) */
		c2spd = read32b(f);	/* frequency */
		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);
		xxi[i][0].sid = i;

		if (strlen((char *)xxih[i].name) || xxs[i].len > 0) {
			if (V(1))
				report("\n[%2X] %-22.22s %05x%c%05x %05x %c%c %2db V%02x F%+03d %5d",
					i, xxih[i].name,
					xxs[i].len,
					xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
					xxs[i].lps,
					replen,
					xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
					flag & 0x100 ? 'S' : ' ',
					flag & 0xff,
					xxi[i][0].vol,
					fine,
					c2spd);
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

			event = &EVENT(pat, k, j);
			a = read8(f);
			b = read8(f);
			c = read8(f);
			d = read8(f);
			if (a)
				event->note = 12 * (a >> 4) + (a & 0x0f);
			event->vol = (b & 0xfc) >> 2;
			event->ins = ((b & 0x03) << 4) + (c >> 4);
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
	char magic[4];

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp(magic, "D.T.", 4))
		return -1;

	fseek(f, 0, SEEK_SET);

	pflag = sflag = 0;
	
	/* IFF chunk IDs */
	iff_register("D.T.", get_d_t_);
	iff_register("S.Q.", get_s_q_);
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

