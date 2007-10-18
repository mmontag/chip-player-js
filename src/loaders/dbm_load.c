/* DigiBoosterPRO module loader for xmp
 * Copyright (C) 1999-2007 Claudio Matsuoka
 *
 * $Id: dbm_load.c,v 1.20 2007-10-18 18:25:10 cmatsuoka Exp $
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


static int dbm_test(FILE *, char *);
static int dbm_load (struct xmp_mod_context *, FILE *, const int);

struct xmp_loader_info dbm_loader = {
	"DBM",
	"DigiBooster Pro",
	dbm_test,
	dbm_load
};

static int dbm_test(FILE * f, char *t)
{
	if (read32b(f) != MAGIC_DBM0)
		return -1;

	fseek(f, 12, SEEK_CUR);
	read_title(f, t, 44);

	return 0;
}



static int have_song;


static void get_info(struct xmp_mod_context *m, int size, FILE *f)
{
	m->xxh->ins = read16b(f);
	m->xxh->smp = read16b(f);
	read16b(f);			/* Songs */
	m->xxh->pat = read16b(f);
	m->xxh->chn = read16b(f);

	m->xxh->trk = m->xxh->pat * m->xxh->chn;

	INSTRUMENT_INIT();
}

static void get_song(struct xmp_mod_context *m, int size, FILE *f)
{
	int i;
	char buffer[50];

	if (have_song)
		return;

	have_song = 1;

	fread(buffer, 44, 1, f);
	if (V(0) && *buffer)
		report("Song name      : %s\n", buffer);

	m->xxh->len = read16b(f);
	reportv(0, "Song length    : %d patterns\n", m->xxh->len);
	for (i = 0; i < m->xxh->len; i++)
		m->xxo[i] = read16b(f);
}

static void get_inst(struct xmp_mod_context *m, int size, FILE *f)
{
	int i;
	int c2spd, flags, snum;
	uint8 buffer[50];

	reportv(0, "Instruments    : %d ", m->xxh->ins);

	reportv(1, "\n     Instrument name                Smp Vol Pan C2Spd");

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		m->xxih[i].nsm = 1;
		fread(buffer, 30, 1, f);
		copy_adjust(m->xxih[i].name, buffer, 30);
		snum = read16b(f);
		if (snum == 0 || snum > m->xxh->smp)
			continue;
		m->xxi[i][0].sid = --snum;
		m->xxi[i][0].vol = read16b(f);
		c2spd = read32b(f);
		m->xxs[snum].lps = read32b(f);
		m->xxs[snum].lpe = m->xxs[i].lps + read32b(f);
		m->xxi[i][0].pan = read16b(f);
		flags = read16b(f);
		m->xxs[snum].flg = flags & 0x03 ? WAVE_LOOPING : 0;
		m->xxs[snum].flg |= flags & 0x02 ? WAVE_BIDIR_LOOP : 0;

		c2spd_to_note(c2spd, &m->xxi[i][0].xpo, &m->xxi[i][0].fin);

		reportv(1, "\n[%2X] %-30.30s #%02X V%02x P%02x %5d ",
			i, m->xxih[i].name, snum,
			m->xxi[i][0].vol, m->xxi[i][0].pan, c2spd);

		reportv(0, ".");
	}
	reportv(0, "\n");
}

static void get_patt(struct xmp_mod_context *m, int size, FILE *f)
{
	int i, c, r, n, sz;
	struct xxm_event *event, dummy;
	uint8 x;

	PATTERN_INIT();

	reportv(0, "Stored patterns: %d ", m->xxh->pat);

	/*
	 * Note: channel and flag bytes are inverted in the format
	 * description document
	 */

	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = read16b(f);
		TRACK_ALLOC(i);

		sz = read32b(f);
		//printf("rows = %d, size = %d\n", m->xxp[i]->rows, sz);

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

			if (c >= m->xxh->chn || r >= m->xxp[i]->rows)
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

static void get_smpl(struct xmp_mod_context *m, int size, FILE *f)
{
	int i, flags;

	reportv(0, "Stored samples : %d ", m->xxh->smp);

	reportv(2, "\n     Len   LBeg  LEnd  L");

	for (i = 0; i < m->xxh->smp; i++) {
		flags = read32b(f);
		m->xxs[i].len = read32b(f);

		if (flags & 0x02)
			m->xxs[i].flg |= WAVE_16_BITS;
		if (flags & 0x04 || ~flags & 0x01)
			continue;
		
		xmp_drv_loadpatch(f, i, m->c4rate, 0, &m->xxs[i], NULL);

		reportv(2, "\n[%2X] %05x%c%05x %05x %c ",
			i, m->xxs[i].len,
			m->xxs[i].flg & WAVE_16_BITS ? '+' : ' ',
			m->xxs[i].lps, m->xxs[i].lpe,
			m->xxs[i].flg & WAVE_LOOPING ?
			(m->xxs[i].flg & WAVE_BIDIR_LOOP ? 'B' : 'L') : ' ');

		reportv(0, ".");
	}
	reportv(0, "\n");
}

static void get_venv(struct xmp_mod_context *m, int size, FILE *f)
{
	int i, j, nenv, ins;

	nenv = read16b(f);

	reportv(0, "Vol envelopes  : %d ", nenv);

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxae[i] = calloc(4, 32);
	}

	for (i = 0; i < nenv; i++) {
		ins = read16b(f);
		m->xxih[ins].aei.flg = read8(f) & 0x07;
		m->xxih[ins].aei.npt = read8(f);
		m->xxih[ins].aei.sus = read8(f);
		m->xxih[ins].aei.lps = read8(f);
		m->xxih[ins].aei.lpe = read8(f);
		read8(f);	/* 2nd sustain */
		//read8(f);	/* reserved */

		for (j = 0; j < 32; j++) {
			m->xxae[ins][j * 2 + 0] = read16b(f);
			m->xxae[ins][j * 2 + 1] = read16b(f);
		}
		reportv(0, ".");
	}
	reportv(0, "\n");
}

static int dbm_load(struct xmp_mod_context *m, FILE *f, const int start)
{
	char name[44];
	uint16 version;

	LOAD_INIT();

	read32b(f);		/* DBM0 */

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

	strncpy(m->name, name, XMP_NAMESIZE);
	snprintf(m->type, XMP_NAMESIZE, "DBM0 (DigiBooster Pro "
				"%d.%02x)", version >> 8, version & 0xff);
	MODULE_INFO();

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(m, f);

	iff_release();

	return 0;
}
