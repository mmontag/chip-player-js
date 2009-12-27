/* Extended Module Player
 * Copyright (C) 1996-2009 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

static int mfp_test(FILE *, char *, const int);
static int mfp_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info mfp_loader = {
	"MFP",
	"Magnetic Fields Packer",
	mfp_test,
	mfp_load
};

static int mfp_test(FILE *f, char *t, const int start)
{
	char buf[378];
	int i;

	if (fread(buf, 1, 378, f) < 378)
		return -1;

	/* check restart byte */
	if (buf[249] != 0x7f)
		return -1;

	for (i = 0; i < 31; i++) {
		/* check finetune */
		if (buf[i * 8 + 2] & 0xf0)
			return -1;

		/* check volume */
		if (buf[i * 8 + 3] > 0x40)
			return -1;

		/* check len vs loop start */
	}

	return 0;
}

static int mfp_load(struct xmp_context *ctx, FILE * f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	int i, j;
	struct xxm_event *event;
	struct stat stat;
	char *basename;
	char modulename[PATH_MAX];
	FILE *s;

	LOAD_INIT();

#if 0
	strncpy(modulename, m->filename, PATH_MAX);
	basename = strtok(modulename, ".");

	afh.speed = read8(f);
	afh.length = read8(f);
	afh.restart = read8(f);
	fread(&afh.order, 128, 1, f);
#endif

	sprintf(m->type, "Magnetic Fields Packer");
	MODULE_INFO();

	m->xxh->chn = 4;

	m->xxh->ins = m->xxh->smp = 31;
	INSTRUMENT_INIT();

	for (i = 0; i < 31; i++) {
		int loop_size;

		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);
		
		m->xxs[i].len = 2 * read16b(f);
		m->xxi[i][0].fin = (int8)(read8(f) << 4);
		m->xxi[i][0].vol = read8(f);
		m->xxs[i].lps = 2 * read16b(f);
		loop_size = read16b(f);

		m->xxs[i].lpe = m->xxs[i].lps + 2 * loop_size;
		m->xxs[i].flg = loop_size > 1 ? WAVE_LOOPING : 0;
		m->xxi[i][0].pan = 0x80;
		m->xxi[i][0].sid = i;
		m->xxih[i].nsm = !!(m->xxs[i].len);
		m->xxih[i].rls = 0xfff;

report("\n[%2X] %04x %04x %04x %c V%02x %+d",
			i, m->xxs[i].len, m->xxs[i].lps,
			m->xxs[i].lpe, loop_size > 1 ? 'L' : ' ',
			m->xxi[i][0].vol, m->xxi[i][0].fin >> 4);
	}

	m->xxh->len = read8(f);
	read8(f);		/* restart */

	for (i = 0; i < 128; i++) {
		m->xxo[i] = read8(f);
		if (m->xxo[i] > m->xxh->pat)
			m->xxh->pat = m->xxo[i];
	}
	m->xxh->pat++;
	m->xxh->trk = m->xxh->pat * m->xxh->chn;

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(ctx, 0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = 64;
		TRACK_ALLOC(i);
		for (j = 0; j < 64 * m->xxh->chn; j++) {
			event = &EVENT(i, j % m->xxh->chn, j / m->xxh->chn);
		}
		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	/* Read samples */
	reportv(ctx, 0, "Loading samples: %d ", m->xxh->ins);

	for (i = 0; i < m->xxh->ins; i++) {

#if 0
		xmp_drv_loadpatch(ctx, s, m->xxi[i][0].sid, m->c4rate,
				  XMP_SMP_UNS, &m->xxs[m->xxi[i][0].sid], NULL);
#endif

		reportv(ctx, 0, ".");
	}
	reportv(ctx, 0, "\n");

	m->xxh->flg |= XXM_FLG_MODRNG;

	return 0;
}
