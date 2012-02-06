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


void *xmp_create_context()
{
	struct context_data *ctx;
	struct xmp_options *o;
	uint16 w;

	ctx = calloc(1, sizeof(struct context_data));

	if (ctx == NULL)
		return NULL;

	*ctx->m.mod.name = *ctx->m.mod.type = 0;

	o = &ctx->o;

	w = 0x00ff;
	o->big_endian = (*(char *)&w == 0x00);

	/* Set defaults */
	o->amplify = DEFAULT_AMPLIFY;
	o->freq = 44100;
	o->mix = 70;
	o->resol = 16;
	o->flags = XMP_CTL_FILTER | XMP_CTL_ITPT;

	return ctx;
}

void xmp_free_context(xmp_context ctx)
{
	free(ctx);
}

void xmp_init()
{
	xmp_init_formats();
}

void xmp_deinit()
{
}

void xmp_channel_mute(xmp_context opaque, int from, int num, int on)
{
	struct context_data *ctx = (struct context_data *)opaque;

	from += num - 1;

	if (num > 0) {
		while (num--)
			virtch_mute(ctx, from - num, on);
	}
}

int xmp_player_ctl(xmp_context opaque, int cmd, int arg)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;

	switch (cmd) {
	case XMP_ORD_PREV:
		if (p->pos > 0)
			p->pos--;
		return p->pos;
	case XMP_ORD_NEXT:
		if (p->pos < m->mod.len)
			p->pos++;
		return p->pos;
	case XMP_ORD_SET:
		if (arg < m->mod.len && arg >= 0) {
			if (p->pos == arg && arg == 0)	/* special case */
				p->pos = -1;
			else
				p->pos = arg;
		}
		return p->pos;
	case XMP_MOD_STOP:
		p->pos = -2;
		break;
	case XMP_MOD_RESTART:
		p->pos = -1;
		break;
	case XMP_GVOL_DEC:
		if (p->volume > 0)
			p->volume--;
		return p->volume;
	case XMP_GVOL_INC:
		if (p->volume < 64)
			p->volume++;
		return p->volume;
	}

	return 0;
}

int xmp_seek_time(xmp_context opaque, int time)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct module_data *m = &ctx->m;
	int i, t;
	/* _D("seek to %d, total %d", time, xmp_cfg.time); */

	time *= 1000;
	for (i = 0; i < m->mod.len; i++) {
		t = m->xxo_info[i].time;

		_D("%2d: %d %d", i, time, t);

		if (t > time) {
			if (i > 0)
				i--;
			xmp_ord_set(opaque, i);
			return 0;
		}
	}
	return -1;
}
