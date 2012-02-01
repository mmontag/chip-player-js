/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
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

#define MAGIC_D_T_	MAGIC4('D','.','T','.')


static int dt_test(FILE *, char *, const int);
static int dt_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info dt_loader = {
	"DTM",
	"Digital Tracker",
	dt_test,
	dt_load
};

static int dt_test(FILE *f, char *t, const int start)
{
	if (read32b(f) != MAGIC_D_T_)
		return -1;

	read_title(f, t, 0);

	return 0;
}


static int pflag, sflag;
static int realpat;


static void get_d_t_(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int b;

	read16b(f);			/* type */
	read16b(f);			/* 0xff then mono */
	read16b(f);			/* reserved */
	m->mod.xxh->tpo = read16b(f);
	if ((b = read16b(f)) > 0)	/* RAMBO.DTM has bpm 0 */
		m->mod.xxh->bpm = b;
	read32b(f);			/* undocumented */

	fread(m->mod.name, 32, 1, f);
	strcpy(m->mod.type, "DTM (Digital Tracker)");

	MODULE_INFO();
}

static void get_s_q_(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, maxpat;

	m->mod.xxh->len = read16b(f);
	m->mod.xxh->rst = read16b(f);
	read32b(f);	/* reserved */

	for (maxpat = i = 0; i < 128; i++) {
		m->mod.xxo[i] = read8(f);
		if (m->mod.xxo[i] > maxpat)
			maxpat = m->mod.xxo[i];
	}
	m->mod.xxh->pat = maxpat + 1;
}

static void get_patt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;

	m->mod.xxh->chn = read16b(f);
	realpat = read16b(f);
	m->mod.xxh->trk = m->mod.xxh->chn * m->mod.xxh->pat;
}

static void get_inst(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, c2spd;
	uint8 name[30];

	m->mod.xxh->ins = m->mod.xxh->smp = read16b(f);

	_D(_D_INFO "Instruments    : %d ", m->mod.xxh->ins);

	INSTRUMENT_INIT();

	for (i = 0; i < m->mod.xxh->ins; i++) {
		int fine, replen, flag;

		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);

		read32b(f);		/* reserved */
		m->mod.xxs[i].len = read32b(f);
		m->mod.xxi[i].nsm = !!m->mod.xxs[i].len;
		fine = read8s(f);	/* finetune */
		m->mod.xxi[i].sub[0].vol = read8(f);
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxs[i].lps = read32b(f);
		replen = read32b(f);
		m->mod.xxs[i].lpe = m->mod.xxs[i].lps + replen - 1;
		m->mod.xxs[i].flg = replen > 2 ?  XMP_SAMPLE_LOOP : 0;

		fread(name, 22, 1, f);
		copy_adjust(m->mod.xxi[i].name, name, 22);

		flag = read16b(f);	/* bit 0-7:resol 8:stereo */
		if ((flag & 0xff) > 8) {
			m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
			m->mod.xxs[i].len >>= 1;
			m->mod.xxs[i].lps >>= 1;
			m->mod.xxs[i].lpe >>= 1;
		}

		read32b(f);		/* midi note (0x00300000) */
		c2spd = read32b(f);	/* frequency */
		c2spd_to_note(c2spd, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);

		/* It's strange that we have both c2spd and finetune */
		m->mod.xxi[i].sub[0].fin += fine;

		m->mod.xxi[i].sub[0].sid = i;

		_D(_D_INFO "[%2X] %-22.22s %05x%c%05x %05x %c%c %2db V%02x F%+03d %5d",
			i, m->mod.xxi[i].name,
			m->mod.xxs[i].len,
			m->mod.xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
			m->mod.xxs[i].lps,
			replen,
			m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
			flag & 0x100 ? 'S' : ' ',
			flag & 0xff,
			m->mod.xxi[i].sub[0].vol,
			fine,
			c2spd);
	}
}

static void get_dapt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int pat, i, j, k;
	struct xmp_event *event;
	static int last_pat;
	int rows;

	if (!pflag) {
		_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);
		pflag = 1;
		last_pat = 0;
		PATTERN_INIT();
	}

	read32b(f);	/* 0xffffffff */
	i = pat = read16b(f);
	rows = read16b(f);

	for (i = last_pat; i <= pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = rows;
		TRACK_ALLOC(i);
	}
	last_pat = pat + 1;

	for (j = 0; j < rows; j++) {
		for (k = 0; k < m->mod.xxh->chn; k++) {
			uint8 a, b, c, d;

			event = &EVENT(pat, k, j);
			a = read8(f);
			b = read8(f);
			c = read8(f);
			d = read8(f);
			if (a) {
				a--;
				event->note = 12 * (a >> 4) + (a & 0x0f);
			}
			event->vol = (b & 0xfc) >> 2;
			event->ins = ((b & 0x03) << 4) + (c >> 4);
			event->fxt = c & 0xf;
			event->fxp = d;
		}
	}
}

static void get_dait(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	static int i = 0;

	if (!sflag) {
		_D(_D_INFO "Stored samples : %d ", m->mod.xxh->smp);
		sflag = 1;
		i = 0;
	}

	if (size > 2) {
		xmp_drv_loadpatch(ctx, f, m->mod.xxi[i].sub[0].sid,
			XMP_SMP_BIGEND, &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	}

	i++;
}

static int dt_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;

	LOAD_INIT();

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
		iff_chunk(ctx, f);

	iff_release();

	return 0;
}

