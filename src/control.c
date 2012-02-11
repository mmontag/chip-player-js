/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "virtual.h"
#include "mixer.h"

xmp_context xmp_create_context()
{
	struct context_data *ctx;
	static int first = 1;

	if (first) {
		xmp_init_formats();
		first = 0;
	}

	ctx = calloc(1, sizeof(struct context_data));

	if (ctx == NULL)
		return NULL;

	return (xmp_context) ctx;
}

void xmp_free_context(xmp_context ctx)
{
	free(ctx);
}

void xmp_channel_mute(xmp_context opaque, int num, int mute)
{
	struct context_data *ctx = (struct context_data *)opaque;

	virtch_mute(ctx, num, mute);
}

int xmp_player_ctl(xmp_context opaque, int cmd, int arg)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;

	switch (cmd) {
	case XMP_CTL_ORD_PREV:
		if (p->pos > 0)
			p->pos--;
		return p->pos;
	case XMP_CTL_ORD_NEXT:
		if (p->pos < m->mod.len)
			p->pos++;
		return p->pos;
	case XMP_CTL_ORD_SET:
		if (arg < m->mod.len && arg >= 0) {
			if (p->pos == arg && arg == 0)	/* special case */
				p->pos = -1;
			else
				p->pos = arg;
		}
		return p->pos;
	case XMP_CTL_MOD_STOP:
		p->pos = -2;
		break;
	case XMP_CTL_MOD_RESTART:
		p->pos = -1;
		break;
	case XMP_CTL_GVOL_DEC:
		if (p->volume > 0)
			p->volume--;
		return p->volume;
	case XMP_CTL_GVOL_INC:
		if (p->volume < 64)
			p->volume++;
		return p->volume;
	case XMP_CTL_SEEK_TIME: {
		int i, t;
		arg *= 1000;
		for (i = 0; i < m->mod.len; i++) {
			t = m->xxo_info[i].time;
			if (t > arg) {
				if (i > 0)
					i--;
				xmp_ord_set(opaque, i);
				return 0;
			}
		}
		return -1; }
	}

	return 0;
}
