/* DigiBoosterPRO module loader for xmp
 * Copyright (C) 1999-2007 Claudio Matsuoka
 *
 * $Id: dbm_load.c,v 1.10 2007-09-29 12:07:58 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/* Based on DigiBooster_E.guide from the DigiBoosterPro 2.20 package.
 * DigiBooster Pro written by Tomasz & Waldemar Piasta
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"
#include "period.h"

#define MAGIC_DBM0	MAGIC4('D','B','M','0')

static int have_song;


static void get_info(int size, FILE *f)
{
	xxh->ins = read16b(f);
	xxh->smp = read16b(f);
	read16b(f);			/* Songs */
	xxh->pat = read16b(f);
	xxh->chn = read16b(f);

	xxh->trk = xxh->pat * xxh->chn;

	INSTRUMENT_INIT();
}

static void get_song(int size, FILE *f)
{
	int i;
	char buffer[50];

	if (have_song)
		return;

	have_song = 1;

	fread(buffer, 44, 1, f);
	if (V(0) && *buffer)
		report("Song name      : %s\n", buffer);

	xxh->len = read16b(f);
	reportv(0, "Song length    : %d patterns\n", xxh->len);
	for (i = 0; i < xxh->len; i++)
		xxo[i] = read16b(f);
}

static void get_inst(int size, FILE *f)
{
	int i;
	int c2spd, flags, snum;
	uint8 buffer[50];

	reportv(0, "Instruments    : %d\n", xxh->ins);

	reportv(1, "     Instrument name                Smp Vol Pan C2Spd\n");

	for (i = 0; i < xxh->ins; i++) {
		xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		xxih[i].nsm = 1;
		fread(buffer, 30, 1, f);
		copy_adjust(xxih[i].name, buffer, 30);
		snum = read16b(f);
		if (snum == 0 || snum > xxh->smp)
			continue;
		xxi[i][0].sid = --snum;
		xxi[i][0].vol = read16b(f);
		c2spd = read32b(f);
		xxs[snum].lps = read32b(f);
		xxs[snum].lpe = xxs[i].lps + read32b(f);
		xxi[i][0].pan = read16b(f);
		flags = read16b(f);
		xxs[snum].flg = flags & 0x03 ? WAVE_LOOPING : 0;
		xxs[snum].flg |= flags & 0x02 ? WAVE_BIDIR_LOOP : 0;

		c2spd_to_note(c2spd, &xxi[i][0].xpo, &xxi[i][0].fin);

		reportv(1, "[%2X] %-30.30s #%02X V%02x P%02x %5d\n",
			i, xxih[i].name, snum,
			xxi[i][0].vol, xxi[i][0].pan, c2spd);
	}
}

static void get_patt(int size, FILE *f)
{
	int i, c, r, n, sz;
	struct xxm_event *event, dummy;
	uint8 x;

	PATTERN_INIT();

	reportv(0, "Stored patterns: %d ", xxh->pat);

	/*
	 * Note: channel and flag bytes are inverted in the format
	 * description document
	 */

	for (i = 0; i < xxh->pat; i++) {
		PATTERN_ALLOC(i);
		xxp[i]->rows = read16b(f);
		TRACK_ALLOC(i);

		sz = read32b(f);
		//printf("rows = %d, size = %d\n", xxp[i]->rows, sz);

		r = 0;
		c = -1;

		while (sz > 0) {
			//printf("  offset=%x,  sz = %d, ", ftell(f), sz);
			c = read8(f);
			if (--sz <= 0) break;
			//printf("c = %02x\n", c);

			if (c == 0) {
				r++;
				c = -1;
				continue;
			}
			c--;

			n = read8(f);
			if (--sz <= 0) break;
			//printf("    n = %d\n", n);

			if (c >= xxh->chn || r >= xxp[i]->rows)
				event = &dummy;
			else
				event = &EVENT(i, c, r);

			if (n & 0x01) {
				x = read8(f);
				event->note = 1 + MSN(x) * 12 + LSN(x);
				if (--sz <= 0) break;
			}
			if (n & 0x02) {
				event->ins = read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x04) {
				event->fxt = read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x08) {
				event->fxp = read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x10) {
				event->f2t = read8(f);
				if (--sz <= 0) break;
			}
			if (n & 0x20) {
				event->f2p = read8(f);
				if (--sz <= 0) break;
			}

			if (event->fxt == 0x1c)
				event->fxt = FX_S3M_BPM;

			if (event->fxt > 0x1c)
				event->fxt = event->f2p = 0;

			if (event->f2t == 0x1c)
				event->f2t = FX_S3M_BPM;

			if (event->f2t > 0x1c)
				event->f2t = event->f2p = 0;
		}
		reportv(0, ".");
	}
	reportv(0, "\n");
}

static void get_smpl(int size, FILE *f)
{
	int i, flags;

	reportv(0, "Stored samples : %d ", xxh->smp);

	reportv(2, "\n     Len   LBeg  LEnd  L");

	for (i = 0; i < xxh->smp; i++) {
		flags = read32b(f);
		xxs[i].len = read32b(f);

		if (flags & 0x02)
			xxs[i].flg |= WAVE_16_BITS;
		if (flags & 0x04 || ~flags & 0x01)
			continue;
		
		xmp_drv_loadpatch(f, i, xmp_ctl->c4rate, 0, &xxs[i], NULL);

		reportv(2, "\n[%2X] %05x%c%05x %05x %c ",
			i, xxs[i].len,
			xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
			xxs[i].lps, xxs[i].lpe,
			xxs[i].flg & WAVE_LOOPING ?
			(xxs[i].flg & WAVE_BIDIR_LOOP ? 'B' : 'L') : ' ');

		reportv(0, ".");
	}
	reportv(0, "\n");
}

static void get_venv(int size, FILE *f)
{
	int i, j, nenv, ins;

	nenv = read16b(f);

	reportv(0, "Vol envelopes  : %d ", nenv);

	for (i = 0; i < xxh->ins; i++) {
		xxae[i] = calloc(4, 32);
	}

	for (i = 0; i < nenv; i++) {
		ins = read16b(f);
		xxih[ins].aei.flg = read8(f) & 0x07;
		xxih[ins].aei.npt = read8(f);
		xxih[ins].aei.sus = read8(f);
		xxih[ins].aei.lps = read8(f);
		xxih[ins].aei.lpe = read8(f);
		read8(f);	/* 2nd sustain */
		//read8(f);	/* reserved */

		for (j = 0; j < 32; j++) {
			xxae[ins][j * 2 + 0] = read16b(f);
			xxae[ins][j * 2 + 1] = read16b(f);
		}
		reportv(0, ".");
	}
	reportv(0, "\n");
}

int dbm_load(FILE *f)
{
	char name[44];
	uint16 version;

	LOAD_INIT();

	/* Check magic */
	if (read32b(f) != MAGIC_DBM0)
		return -1;

	have_song = 0;
	version = read16b(f);

	fseek(f, 10, SEEK_CUR);
	fread(name, 1, 44, f);

	/* IFF chunk IDs */
	iff_register("INFO", get_info);
	iff_register("SONG", get_song);
	iff_register("INST", get_inst);
	iff_register("PATT", get_patt);
	iff_register("SMPL", get_smpl);
	iff_register("VENV", get_venv);

	strncpy(xmp_ctl->name, name, XMP_DEF_NAMESIZE);
	strcpy(xmp_ctl->type, "DBM0 (DigiBooster Pro)");
	sprintf(tracker_name, "DigiBooster Pro %d.%0x",
		version >> 8, version & 0xff);

	MODULE_INFO();

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(f);

	iff_release();

	return 0;
}
