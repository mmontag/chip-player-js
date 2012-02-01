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

#include <unistd.h>
#include <limits.h>
#include "load.h"
#include "iff.h"
#include "period.h"

/* Galaxy Music System 5.0 module file loader
 *
 * Based on the format description by Dr.Eggman
 * (http://www.jazz2online.com/J2Ov2/articles/view.php?articleID=288)
 * and Jazz Jackrabbit modules by Alexander Brandon from Lori Central
 * (http://www.loricentral.com/jj2music.html)
 */

static int gal5_test(FILE *, char *, const int);
static int gal5_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info gal5_loader = {
	"GAL5",
	"Galaxy Music System 5.0",
	gal5_test,
	gal5_load
};

static uint8 chn_pan[64];

static int gal5_test(FILE *f, char *t, const int start)
{
        if (read32b(f) != MAGIC4('R', 'I', 'F', 'F'))
		return -1;

	read32b(f);

	if (read32b(f) != MAGIC4('A', 'M', ' ', ' '))
		return -1;

	if (read32b(f) != MAGIC4('I', 'N', 'I', 'T'))
		return -1;

	read_title(f, t, 0);

	return 0;
}

static void get_init(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	char buf[64];
	int flags;
	
	fread(buf, 1, 64, f);
	strncpy(m->mod.name, buf, 64);
	strcpy(m->mod.type, "Galaxy Music System 5.0");
	flags = read8(f);	/* bit 0: Amiga period */
	if (~flags & 0x01)
		m->mod.xxh->flg = XXM_FLG_LINEAR;
	m->mod.xxh->chn = read8(f);
	m->mod.xxh->tpo = read8(f);
	m->mod.xxh->bpm = read8(f);
	read16l(f);		/* unknown - 0x01c5 */
	read16l(f);		/* unknown - 0xff00 */
	read8(f);		/* unknown - 0x80 */
	fread(chn_pan, 1, 64, f);
}

static void get_ordr(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i;

	m->mod.xxh->len = read8(f) + 1;
	/* Don't follow Dr.Eggman's specs here */

	for (i = 0; i < m->mod.xxh->len; i++)
		m->mod.xxo[i] = read8(f);
}

static void get_patt_cnt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i;

	i = read8(f) + 1;	/* pattern number */

	if (i > m->mod.xxh->pat)
		m->mod.xxh->pat = i;
}

static void get_inst_cnt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i;

	read32b(f);		/* 42 01 00 00 */
	read8(f);		/* 00 */
	i = read8(f) + 1;	/* instrument number */

	if (i > m->mod.xxh->ins)
		m->mod.xxh->ins = i;
}

static void get_patt(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	struct xmp_event *event, dummy;
	int i, len, chan;
	int rows, r;
	uint8 flag;
	
	i = read8(f);	/* pattern number */
	len = read32l(f);
	
	rows = read8(f) + 1;

	PATTERN_ALLOC(i);
	m->mod.xxp[i]->rows = rows;
	TRACK_ALLOC(i);

	for (r = 0; r < rows; ) {
		if ((flag = read8(f)) == 0) {
			r++;
			continue;
		}

		chan = flag & 0x1f;

		event = chan < m->mod.xxh->chn ? &EVENT(i, chan, r) : &dummy;

		if (flag & 0x80) {
			uint8 fxp = read8(f);
			uint8 fxt = read8(f);

			switch (fxt) {
			case 0x14:		/* speed */
				fxt = FX_S3M_TEMPO;
				break;
			default:
				if (fxt > 0x0f) {
					printf("unknown effect %02x %02x\n", fxt, fxp);
					fxt = fxp = 0;
				}
			}

			event->fxt = fxt;
			event->fxp = fxp;
		}

		if (flag & 0x40) {
			event->ins = read8(f);
			event->note = read8(f);

			if (event->note == 128) {
				event->note = XMP_KEY_OFF;
			} else if (event->note > 12) {
				event->note -= 12;
			} else {
				event->note = 0;
			}
		}

		if (flag & 0x20) {
			event->vol = 1 + read8(f) / 2;
		}
	}
}

static void get_inst(struct xmp_context *ctx, int size, FILE *f)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, srate, finetune, flags;
	int has_unsigned_sample;

	read32b(f);		/* 42 01 00 00 */
	read8(f);		/* 00 */
	i = read8(f);		/* instrument number */
	
	fread(&m->mod.xxi[i].name, 1, 28, f);
	str_adj((char *)m->mod.xxi[i].name);

	fseek(f, 290, SEEK_CUR);	/* Sample/note map, envelopes */
	m->mod.xxi[i].nsm = read16l(f);

	_D(_D_INFO "[%2X] %-28.28s  %2d ", i, m->mod.xxi[i].name, m->mod.xxi[i].nsm);

	if (m->mod.xxi[i].nsm == 0)
		return;

	m->mod.xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), m->mod.xxi[i].nsm);

	/* FIXME: Currently reading only the first sample */

	read32b(f);	/* RIFF */
	read32b(f);	/* size */
	read32b(f);	/* AS   */
	read32b(f);	/* SAMP */
	read32b(f);	/* size */
	read32b(f);	/* unknown - usually 0x40000000 */

	fread(&m->mod.xxs[i].name, 1, 28, f);
	str_adj((char *)m->mod.xxs[i].name);

	read32b(f);	/* unknown - 0x0000 */
	read8(f);	/* unknown - 0x00 */

	m->mod.xxi[i].sub[0].sid = i;
	m->mod.xxi[i].vol = read8(f);
	m->mod.xxi[i].sub[0].pan = 0x80;
	m->mod.xxi[i].sub[0].vol = (read16l(f) + 1) / 512;
	flags = read16l(f);
	read16l(f);			/* unknown - 0x0080 */
	m->mod.xxs[i].len = read32l(f);
	m->mod.xxs[i].lps = read32l(f);
	m->mod.xxs[i].lpe = read32l(f);

	m->mod.xxs[i].flg = 0;
	has_unsigned_sample = 1;
	if (flags & 0x04)
		m->mod.xxs[i].flg |= XMP_SAMPLE_16BIT;
	if (flags & 0x08)
		m->mod.xxs[i].flg |= XMP_SAMPLE_LOOP;
	if (flags & 0x10)
		m->mod.xxs[i].flg |= XMP_SAMPLE_LOOP | XMP_SAMPLE_LOOP_BIDIR;
	if (~flags & 0x80)
		has_unsigned_sample = 1;

	srate = read32l(f);
	finetune = 0;
	c2spd_to_note(srate, &m->mod.xxi[i].sub[0].xpo, &m->mod.xxi[i].sub[0].fin);
	m->mod.xxi[i].sub[0].fin += finetune;

	read32l(f);			/* 0x00000000 */
	read32l(f);			/* unknown */

	_D(_D_INFO "  %x: %05x%c%05x %05x %c V%02x %04x %5d",
		0, m->mod.xxs[i].len,
		m->mod.xxs[i].flg & XMP_SAMPLE_16BIT ? '+' : ' ',
		m->mod.xxs[i].lps,
		m->mod.xxs[i].lpe,
		m->mod.xxs[i].flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' : 
			m->mod.xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
		m->mod.xxi[i].sub[0].vol, flags, srate);

	if (m->mod.xxs[i].len > 1) {
		load_patch(ctx, f, i, has_unsigned_sample ?
			XMP_SMP_UNS : 0, &m->mod.xxs[i], NULL);
	}
}

static int gal5_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_mod_context *m = &ctx->m;
	int i, offset;

	LOAD_INIT();

	read32b(f);	/* Skip RIFF */
	read32b(f);	/* Skip size */
	read32b(f);	/* Skip AM   */

	offset = ftell(f);

	m->mod.xxh->smp = m->mod.xxh->ins = 0;

	iff_register("INIT", get_init);		/* Galaxy 5.0 */
	iff_register("ORDR", get_ordr);
	iff_register("PATT", get_patt_cnt);
	iff_register("INST", get_inst_cnt);
	iff_setflag(IFF_LITTLE_ENDIAN);
	iff_setflag(IFF_SKIP_EMBEDDED);
	iff_setflag(IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(ctx, f);

	iff_release();

	m->mod.xxh->trk = m->mod.xxh->pat * m->mod.xxh->chn;
	m->mod.xxh->smp = m->mod.xxh->ins;

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	_D(_D_INFO "Stored patterns: %d", m->mod.xxh->pat);
	_D(_D_INFO "Stored samples: %d ", m->mod.xxh->smp);

	fseek(f, start + offset, SEEK_SET);

	iff_register("PATT", get_patt);
	iff_register("INST", get_inst);
	iff_setflag(IFF_LITTLE_ENDIAN);
	iff_setflag(IFF_SKIP_EMBEDDED);
	iff_setflag(IFF_CHUNK_ALIGN2);

	/* Load IFF chunks */
	while (!feof (f))
		iff_chunk(ctx, f);

	iff_release();

	for (i = 0; i < m->mod.xxh->chn; i++)
		m->mod.xxc[i].pan = chn_pan[i] * 2;

	return 0;
}
