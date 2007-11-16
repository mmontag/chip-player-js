/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * $Id: coco_load.c,v 1.1 2007-11-16 00:57:29 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

static int coco_test (FILE *, char *, const int);
static int coco_load (struct xmp_context *, FILE *, const int);

struct xmp_loader_info coco_loader = {
	"COCO",
	"Coconizer",
	coco_test,
	coco_load
};

static int coco_test(FILE *f, char *t, const int start)
{
	uint8 x;
	uint32 y;

	x = read8(f);

	if (~x & 0x80)
		return -1;

	x &= 0x3f;
	if (x != 4 && x != 8)
		return -1;

	read_title(f, t, 19);
	
	read8(f);
	read8(f);
	read8(f);
	read8(f);

	y = read32l(f);
	if (y < 64 || y > 0x00100000)
		return -1;

	y = read32l(f);
	if (y < 64 || y > 0x00100000)
		return -1;

	return 0;
}

static int coco_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	struct xxm_event *event;
	int i, j;
	int seq_ptr, pat_ptr, smp_ptr[64];

	LOAD_INIT();

	m->xxh->chn = read8(f) & 0x3f;
	read_title(f, m->name, 19);
	read8(f);

	strcpy(m->type, "Coconizer");

	m->xxh->ins = m->xxh->smp = read8(f);
	m->xxh->len = read8(f);
	m->xxh->pat = read8(f);
	m->xxh->trk = m->xxh->pat * m->xxh->chn;

	seq_ptr = read32l(f);
	pat_ptr = read32l(f);

	MODULE_INFO();
	INSTRUMENT_INIT();

	reportv(ctx, 1, "     Name        Len  LBeg LEnd L Vol\n");

	for (i = 0; i < m->xxh->ins; i++) {
		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		smp_ptr[i] = read32l(f);
		m->xxs[i].len = read32l(f);
		m->xxi[i][0].vol = (0xff - read32l(f)) >> 4;
		m->xxi[i][0].pan = 0x80;
		m->xxs[i].lps = read32b(f);
		m->xxs[i].lpe = m->xxs[i].lps + read32b(f);
		m->xxs[i].flg = m->xxs[i].lps > 0 ? WAVE_LOOPING : 0;
		fread(m->xxih[i].name, 1, 10, f);
		read8(f);
		read8(f);

		m->xxih[i].nsm = !!m->xxs[i].len;
		m->xxi[i][0].sid = i;

		if (V(1) && (strlen((char*)m->xxih[i].name) || (m->xxs[i].len > 1))) {
			report("[%2X] %-10.10s %04x %04x %04x %c V%02x\n",
				i, m->xxih[i].name,
				m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
				m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				m->xxi[i][0].vol);
		}
	}

	/* Sequence */

	fseek(f, start + seq_ptr, SEEK_SET);
	for (i = 0; i < m->xxh->len; i++)
		m->xxo[i] = read8(f);


	/* Patterns */

	PATTERN_INIT();

	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC (i);
		m->xxp[i]->rows = 64;
		TRACK_ALLOC (i);

		for (j = 0; j < (64 * m->xxh->chn); j++) {
			event = &EVENT (i, j % m->xxh->chn, j / m->xxh->chn);
			event->note = read8(f);
			event->ins = read8(f);
			event->fxt = 0; //read8(f);
			event->fxp = 0; //read8(f);
		}

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	/* Read samples */

	reportv(ctx, 0, "Stored samples : %d ", m->xxh->smp);

	for (i = 0; i < m->xxh->ins; i++) {
		if (m->xxih[i].nsm == 0)
			continue;

		fseek(f, start + smp_ptr[i], SEEK_SET);
		xmp_drv_loadpatch(ctx, f, m->xxi[i][0].sid, m->c4rate, 0,
					&m->xxs[m->xxi[i][0].sid], NULL);
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	return 0;
}
