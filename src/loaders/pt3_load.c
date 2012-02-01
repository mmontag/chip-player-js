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

#include <stdio.h>
#include "load.h"
#include "mod.h"
#include "iff.h"

#define MAGIC_FORM	MAGIC4('F','O','R','M')
#define MAGIC_MODL	MAGIC4('M','O','D','L')

static int pt3_test(FILE *, char *, const int);
static int pt3_load(struct xmp_context *, FILE *, const int);
static int ptdt_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info pt3_loader = {
	"PT3",
	"Protracker 3",
	pt3_test,
	pt3_load
};

static int pt3_test(FILE *f, char *t, const int start)
{
	uint32 form, id;

	form = read32b(f);
	read32b(f);
	id = read32b(f);

	if (form != MAGIC_FORM || id != MAGIC_MODL)
		return -1;

	read_title(f, t, 0);

	return 0;
}

#define PT3_FLAG_CIA	0x0001	/* VBlank if not set */
#define PT3_FLAG_FILTER	0x0002	/* Filter status */
#define PT3_FLAG_SONG	0x0004	/* Modules have this bit unset */
#define PT3_FLAG_IRQ	0x0008	/* Soft IRQ */
#define PT3_FLAG_VARPAT	0x0010	/* Variable pattern length */
#define PT3_FLAG_8VOICE	0x0020	/* 4 voices if not set */
#define PT3_FLAG_16BIT	0x0040	/* 8 bit samples if not set */
#define PT3_FLAG_RAWPAT	0x0080	/* Packed patterns if not set */


static void get_info(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int flags;
	int day, month, year, hour, min, sec;
	int dhour, dmin, dsec;

	fread(m->mod.name, 1, 32, f);
	m->mod.ins = read16b(f);
	m->mod.len = read16b(f);
	m->mod.pat = read16b(f);
	m->mod.gvl = read16b(f);
	m->mod.bpm = read16b(f);
	flags = read16b(f);
	day   = read16b(f);
	month = read16b(f);
	year  = read16b(f);
	hour  = read16b(f);
	min   = read16b(f);
	sec   = read16b(f);
	dhour = read16b(f);
	dmin  = read16b(f);
	dsec  = read16b(f);

	MODULE_INFO();

	_D(_D_INFO "Creation date: %02d/%02d/%02d %02d:%02d:%02d",
		       day, month, year, hour, min, sec);
	_D(_D_INFO "Playing time: %02d:%02d:%02d", dhour, dmin, dsec);
}

static void get_cmnt(struct xmp_context *ctx, int size, FILE *f)
{
	_D(_D_INFO "Comment size: %d", size);
}

static void get_ptdt(struct xmp_context *ctx, int size, FILE *f)
{
	ptdt_load(ctx, f, 0);
}

static int pt3_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	char buf[20];

	LOAD_INIT();

	read32b(f);		/* FORM */
	read32b(f);		/* size */
	read32b(f);		/* MODL */

	read32b(f);		/* VERS */
	read32b(f);		/* VERS size */

	fread(buf, 1, 10, f);
	set_type(m, "%-6.6s (Protracker IFFMODL)", buf + 4);

	/* IFF chunk IDs */
	iff_register("INFO", get_info);
	iff_register("CMNT", get_cmnt);
	iff_register("PTDT", get_ptdt);

	iff_setflag(IFF_FULL_CHUNK_SIZE);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(ctx, f);

	iff_release();

	return 0;
}

static int ptdt_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, j;
	struct xmp_event *event;
	struct mod_header mh;
	uint8 mod_event[4];

	fread(&mh.name, 20, 1, f);
	for (i = 0; i < 31; i++) {
		fread(&mh.ins[i].name, 22, 1, f);
		mh.ins[i].size = read16b(f);
		mh.ins[i].finetune = read8(f);
		mh.ins[i].volume = read8(f);
		mh.ins[i].loop_start = read16b(f);
		mh.ins[i].loop_size = read16b(f);
	}
	mh.len = read8(f);
	mh.restart = read8(f);
	fread(&mh.order, 128, 1, f);
	fread(&mh.magic, 4, 1, f);

	m->mod.ins = 31;
	m->mod.smp = m->mod.ins;
	m->mod.chn = 4;
	m->mod.len = mh.len;
	m->mod.rst = mh.restart;
	memcpy(m->mod.xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (m->mod.xxo[i] > m->mod.pat)
			m->mod.pat = m->mod.xxo[i];
	}

	m->mod.pat++;
	m->mod.trk = m->mod.chn * m->mod.pat;

	INSTRUMENT_INIT();

	for (i = 0; i < m->mod.ins; i++) {
		m->mod.xxi[i].sub = calloc(sizeof (struct xmp_subinstrument), 1);
		m->mod.xxs[i].len = 2 * mh.ins[i].size;
		m->mod.xxs[i].lps = 2 * mh.ins[i].loop_start;
		m->mod.xxs[i].lpe = m->mod.xxs[i].lps + 2 * mh.ins[i].loop_size;
		m->mod.xxs[i].flg = mh.ins[i].loop_size > 1 ? XMP_SAMPLE_LOOP : 0;
		if (m->mod.xxs[i].flg & XMP_SAMPLE_LOOP) {
			if (m->mod.xxs[i].len == 0 && m->mod.xxs[i].len > m->mod.xxs[i].lpe)
				m->mod.xxs[i].flg |= XMP_SAMPLE_LOOP_FULL;
		}
		m->mod.xxi[i].sub[0].fin = (int8)(mh.ins[i].finetune << 4);
		m->mod.xxi[i].sub[0].vol = mh.ins[i].volume;
		m->mod.xxi[i].sub[0].pan = 0x80;
		m->mod.xxi[i].sub[0].sid = i;
		m->mod.xxi[i].nsm = !!(m->mod.xxs[i].len);
		m->mod.xxi[i].rls = 0xfff;

		copy_adjust(m->mod.xxi[i].name, mh.ins[i].name, 22);

		_D(_D_INFO "[%2X] %-22.22s %04x %04x %04x %c V%02x %+d %c",
				i, m->mod.xxi[i].name,
				m->mod.xxs[i].len, m->mod.xxs[i].lps,
				m->mod.xxs[i].lpe,
				mh.ins[i].loop_size > 1 ? 'L' : ' ',
				m->mod.xxi[i].sub[0].vol,
				m->mod.xxi[i].sub[0].fin >> 4,
				m->mod.xxs[i].flg & XMP_SAMPLE_LOOP_FULL ? '!' : ' ');
	}

	PATTERN_INIT();

	/* Load and convert patterns */
	_D(_D_INFO "Stored patterns: %d", m->mod.pat);

	for (i = 0; i < m->mod.pat; i++) {
		PATTERN_ALLOC(i);
		m->mod.xxp[i]->rows = 64;
		TRACK_ALLOC(i);
		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			fread(mod_event, 1, 4, f);
			cvt_pt_event(event, mod_event);
		}
	}

	m->mod.flg |= XXM_FLG_MODRNG;

	/* Load samples */
	_D(_D_INFO "Stored samples: %d", m->mod.smp);

	for (i = 0; i < m->mod.smp; i++) {
		if (!m->mod.xxs[i].len)
			continue;
		load_patch(ctx, f, m->mod.xxi[i].sub[0].sid, 0,
				  &m->mod.xxs[m->mod.xxi[i].sub[0].sid], NULL);
	}

	return 0;
}
