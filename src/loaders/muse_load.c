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
	return -1;
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

	return -1;
}
