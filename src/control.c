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
#include <stdarg.h>

#include "format.h"
#include "virtual.h"
#include "mixer.h"

/**
 * @brief Create player context
 *
 * Create and initialize a handle used to identify this player context.
 *
 * @return Player context handle.
 */
xmp_context xmp_create_context()
{
	struct context_data *ctx;

	ctx = calloc(1, sizeof(struct context_data));
	if (ctx == NULL) {
		return NULL;
	}

	return (xmp_context)ctx;
}

/**
 * @brief Destroy player context
 *
 * Release all context data referenced by the given handle.
 *
 * @param handle Player context handle.
 */
void xmp_free_context(xmp_context handle)
{
	free(handle);
}

int _xmp_ctl(xmp_context handle, int cmd, ...)
{
	va_list ap;
	struct context_data *ctx = (struct context_data *)handle;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	int ret = 0;

	va_start(ap, cmd);

	switch (cmd) {
	case XMP_CTL_ORD_PREV:
		if (p->pos > 0)
			p->pos--;
		ret = p->pos;
		break;
	case XMP_CTL_ORD_NEXT:
		if (p->pos < m->mod.len)
			p->pos++;
		ret = p->pos;
		break;
	case XMP_CTL_ORD_SET: {
		int arg = va_arg(ap, int);

		if (arg < m->mod.len && arg >= 0) {
			if (p->pos == arg && arg == 0)	/* special case */
				p->pos = -1;
			else
				p->pos = arg;
		}
		ret = p->pos;
		break; }
	case XMP_CTL_MOD_STOP:
		p->pos = -2;
		break;
	case XMP_CTL_MOD_RESTART:
		p->pos = -1;
		break;
	case XMP_CTL_GVOL_DEC:
		if (p->volume > 0)
			p->volume--;
		ret = p->volume;
		break;
	case XMP_CTL_GVOL_INC:
		if (p->volume < 64)
			p->volume++;
		ret = p->volume;
		break;
	case XMP_CTL_SEEK_TIME: {
		int arg = va_arg(ap, int);
		int i, t;

		arg *= 1000;
		for (i = 0; i < m->mod.len; i++) {
			t = m->xxo_info[i].time;
			if (t > arg) {
				if (i > 0)
					i--;
				xmp_ord_set(handle, i);
				break;
			}
		}
		break; }
	case XMP_CTL_CH_MUTE: {
		int arg1 = va_arg(ap, int);
		int arg2 = va_arg(ap, int);

		virtch_mute(ctx, arg1, arg2);
		break; }
	default:
		ret = -1;
	}

	va_end(ap);

	return 0;
}

int xmp_get_format_list(char ***list)
{
	return format_list(list);
}
