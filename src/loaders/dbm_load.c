/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
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


static int dbm_test(FILE *, char *, const int);
static int dbm_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info dbm_loader = {
	"DBM",
	"DigiBooster Pro",
	dbm_test,
	dbm_load
};

static int dbm_test(FILE * f, char *t, const int start)
{
	if (read32b(f) != MAGIC_DBM0)
		return -1;

	fseek(f, 12, SEEK_CUR);
	read_title(f, t, 44);

	return 0;
}



static int have_song;


static void get_info(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;

	m->mod.ins = read16b(f);
	m->mod.smp = read16b(f);
	read16b(f);			/* Songs */
	m->mod.pat = read16b(f);
	m->mod.chn = read16b(f);

	m->mod.trk = m->mod.pat * m->mod.chn;

	INSTRUMENT_INIT();
}

static void get_song(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i;
	char buffer[50];

	if (have_song)
		return;

	have_song = 1;

	fread(buffer, 44, 1, f);
	_D(_D_INFO "Song name: %s", buffer);

	m->mod.len = read16b(f);
	_D(_D_INFO "Song length: %d patterns", m->mod.len);

	for (i = 0; i < m->mod.len; i++)
		m->mod.xxo[i] = read16b(f);
}

static void get_inst(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i;
	int c2spd, flags, snum;
	uint8 buffer[50];

	_D(_D_INFO "Instruments: %d", m->mod.ins);

	for (i = 0; i < m->mod.ins; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		m->mod.xxi[i].nsm = 1;
		fread(buffer, 30, 1, f);
		copy_adjust(m->mod.xxi[i].name, buffer, 30);
		snum = read16b(f);
		if (snum == 0 || snum > m->mod.smp)
			continue;
		m->mod.xxi[i].sub[0].sid = --snum;
		m->mod.xxi[i].sub[0].vol = read16b(f);
		c2spd = read32b(f);
		m->mod.xxs[snum].lps = read32b(f);
		m->mod.xxs[snum].lpe = m->mod.xxs[i].lps + read32b(f);
		m->mod.xxi[i].sub[0].pan = 0x80 + (int16)read16b(f);
		if (m->mod.xxi[i].sub[0].pan > 0xff)
			m->mod.xxi[i].sub[0].pan = 0xff;
		flags = read16b(f);
		m->mod.xxs[snum].flg = flags & 0x03 ? XMP_SAMPLE_LOOP : 0;
		m->mod.xxs[snum].flg |= flags & 0x02 ? XMP_SAMPLE_LOOP_BIDIR : 0;

		c2spd_to_note(c2spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);

		_D(_D_INFO "[%2X] %-30.30s #%02X V%02x P%02x %5d",
			i, m->mod.xxi[i].name, snum,
			m->mod.xxi[i].sub[0].vol, m->mod.xxi[i].sub[0].pan, c2spd);
	}
}

static void get_patt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, c, r, n, sz;
	struct xmp_event *event, dummy;
	uint8 x;

	PATTERN_INIT();

	_D(_D_INFO "Stored patterns: %d ", m->mod.pat);

	/*
	 * Note: channel and flag bytes are inverted in the format
	 * description document
	 */

	for (i = 0; i < m->mod.pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = read16b(f);
		TRACK_ALLOC(i);

		sz = read32b(f);
		//printf("rows = %d, size = %d\n", m->mod.xxp[i]->rows, sz);

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

			if (c >= m->mod.chn || r >= m->mod.xxp[i]->rows)
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
	}
}

static void get_smpl(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, flags;

	_D(_D_INFO "Stored samples: %d", m->mod.smp);

	for (i = 0; i < m->mod.smp; i++) {
		flags = read32b(f);
		m->mod.xxs[i].len = read32b(f);

		if (flags & 0x02) {
			m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
		}

		if (flags & 0x04) {	/* Skip 32-bit samples */
			m->mod.xxs[i].len <<= 2;
			fseek(f, m->mod.xxs[i].len, SEEK_CUR);
			continue;
		}
		
		load_patch(ctx, f, i, XMP_SMP_BIGEND,
							&m->mod.xxs[i], NULL);

		if (m->mod.xxs[i].len == 0)
			continue;

		_D(_D_INFO "[%2X] %08x %05x%c%05x %05x %c",
			i, flags, m->mod.xxs[i].len,
			m->mod.xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			m->mod.xxs[i].lps, m->mod.xxs[i].lpe,
			m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ?
			(m->mod.xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : 'L') : ' ');

	}
}

static void get_venv(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, j, nenv, ins;

	nenv = read16b(f);

	_D(_D_INFO "Vol envelopes  : %d ", nenv);

	for (i = 0; i < nenv; i++) {
		ins = read16b(f) - 1;
		m->mod.xxi[ins].aei.flg = read8(f) & 0x07;
		m->mod.xxi[ins].aei.npt = read8(f);
		m->mod.xxi[ins].aei.sus = read8(f);
		m->mod.xxi[ins].aei.lps = read8(f);
		m->mod.xxi[ins].aei.lpe = read8(f);
		read8(f);	/* 2nd sustain */
		//read8(f);	/* reserved */

		for (j = 0; j < 32; j++) {
			m->mod.xxi[ins].aei.data[j * 2 + 0] = read16b(f);
			m->mod.xxi[ins].aei.data[j * 2 + 1] = read16b(f);
		}
	}
}

static int dbm_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	char name[44];
	uint16 version;
	int i;

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

	strncpy(m->mod.name, name, XMP_NAMESIZE);
	snprintf(m->mod.type, XMP_NAMESIZE, "DBM0 (DigiBooster Pro "
				"%d.%02x)", version >> 8, version & 0xff);
	MODULE_INFO();

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(ctx, f);

	iff_release();

	for (i = 0; i < m->mod.chn; i++)
		m->mod.xxc[i].pan = 0x80;

	return 0;
}
