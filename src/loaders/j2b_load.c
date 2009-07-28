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

static int j2b_test(FILE *, char *, const int);
static int j2b_load(struct xmp_context *, FILE *, const int);

struct xmp_loader_info j2b_loader = {
	"J2B",
	"Jazz 2",
	j2b_test,
	j2b_load
};


static int j2b_test(FILE *f, char *t, const int start)
{
	return -1;
}

static int j2b_load(struct xmp_context *ctx, FILE *f, const int start)
{
	struct xmp_player_context *p = &ctx->p;
	struct xmp_mod_context *m = &p->m;
	struct xmp_options *o = &ctx->o;
	struct xxm_event *event;
	char tmp[PATH_MAX];
	int i, j;
	int fd;

	/* Inflate */

	get_temp_dir(tmp, PATH_MAX);
	strncat(tmp, "xmp_XXXXXX", PATH_MAX);

	if ((fd = mkstemp(tmp)) < 0)
		return -1;

#if 0
	if (j2b_wizardry(fileno(f), fd, &fmt) < 0) {
		close(fd);
		unlink(tmp);
		return -1;
	}
#endif

	if ((f = fdopen(fd, "w+b")) == NULL) {
		close(fd);
		unlink(tmp);
		return -1;
	}


	/* Module loading */

	LOAD_INIT();


end:
	fclose(f);
	unlink(tmp);
	return 0;

err:
	fclose(f);
	unlink(tmp);
	return -1;
}
