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

#include <unistd.h>
#include <limits.h>
#include "load.h"
#include "iff.h"

static int muse_test(FILE *, char *, const int);
static int muse_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info muse_loader = {
	"MUSE",
	"Epic MegaGames MUSE",
	muse_test,
	muse_load
};


static int muse_test(FILE *f, char *t, const int start)
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
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	char buf[64];
	
	fread(buf, 1, 64, f);
	strncpy(m->name, buf, 64);

	read8(f);	/* unknown */
	m->xxh->chn = read8(f);
	m->xxh->tpo = read8(f);
	m->xxh->bpm = read8(f);
	read32l(f);	/* unknown */
	read8(f);	/* unknown */
	/* channelpan */
}

static void get_ordr(struct xmp_context *ctx, int size, FILE *f)
{
}

static int muse_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	struct xmp_options *o = &ctx->o;
	struct xxm_event *event;
	char tmp[PATH_MAX];
	int i, j;
	int fd;

	LOAD_INIT();

	read32b(f);

	m->xxh->smp = m->xxh->ins = 0;

	iff_register("INIT", get_init);
	iff_register("ORDR", get_ordr);

	/* Load IFF chunks */
	while (!feof(f))
		iff_chunk(ctx, f);

	iff_release();

	m->xxh->trk = m->xxh->pat * m->xxh->chn;

	strcpy (m->type, "MUSE (Epic Megagames MUSE)");

	MODULE_INFO();

	return -1;
}
