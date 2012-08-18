/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
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
#include "unxz/xz.h"

const char *xmp_version = XMP_VERSION;
const unsigned int xmp_vercode = XMP_VERCODE;

xmp_context xmp_create_context()
{
	struct context_data *ctx;

	ctx = calloc(1, sizeof(struct context_data));
	if (ctx == NULL) {
		return NULL;
	}

	xz_crc32_init();

	return (xmp_context)ctx;
}

void xmp_free_context(xmp_context opaque)
{
	free(opaque);
}

static void set_position(struct context_data *ctx, int pos, int dir)
{
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct flow_control *f = &p->flow;
	int seq, start;

	/* If dir is 0, we can jump to a different sequence */
	if (dir == 0) {
		seq = get_sequence(ctx, pos);
	} else {
		seq = p->sequence;
	}

	start = m->seq_data[seq].entry_point;

	if (seq >= 0) {
		p->sequence = seq;

		if (pos >= 0) {
			if (mod->xxo[pos] == 0xff) {
				return;
			}

			while (mod->xxo[pos] == 0xfe && pos > start) {
				if (dir < 0) {
					pos--;
				} else {
					pos++;
				}
			}

			if (pos > p->scan[seq].ord) {
				f->end_point = 0;
			} else {
				f->num_rows = mod->xxp[mod->xxo[p->ord]]->rows;
				f->end_point = p->scan[seq].num;
			}
		}

		if (pos < m->mod.len) {
			if (pos == 0) {
				p->pos = -1;
			} else {
				p->pos = pos;
			}
		}
	}
}

int xmp_control(xmp_context opaque, int cmd, ...)
{
	va_list ap;
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_data *s = &ctx->s;
	int ret = 0;

	va_start(ap, cmd);

	switch (cmd) {
	case XMP_CTL_POS_PREV:
		if (p->pos == m->seq_data[p->sequence].entry_point) {
			set_position(ctx, -1, -1);
		} else if (p->pos > m->seq_data[p->sequence].entry_point) {
			set_position(ctx, p->pos - 1, -1);
		}
		ret = p->pos;
		break;
	case XMP_CTL_POS_NEXT:
		if (p->pos < m->mod.len)
			set_position(ctx, p->pos + 1, 1);
		ret = p->pos;
		break;
	case XMP_CTL_POS_SET: {
		int arg = va_arg(ap, int);
		set_position(ctx, arg, 0);
		ret = p->pos;
		break; }
	case XMP_CTL_MOD_STOP:
		p->pos = -2;
		break;
	case XMP_CTL_MOD_RESTART:
		p->pos = -1;
		break;
	case XMP_CTL_SEEK_TIME: {
		int arg = va_arg(ap, int);
		int i, t;

		for (i = m->mod.len - 1; i >= 0; i--) {
			int pat = m->mod.xxo[i];
			if (pat >= m->mod.pat) {
				continue;
			}
			if (get_sequence(ctx, i) != p->sequence) {
				continue;
			}
			t = m->xxo_info[i].time;
			if (arg >= t) {
				set_position(ctx, i, 1);
				break;
			}
		}
		if (i < 0) {
			xmp_set_position(opaque, 0);
		}
		break; }
	case XMP_CTL_CH_MUTE: {
		int arg1 = va_arg(ap, int);
		int arg2 = va_arg(ap, int);
		ret = virt_mute(ctx, arg1, arg2);
		break; }
	case XMP_CTL_MIXER_AMP: {
		int arg = va_arg(ap, int);
		s->amplify = arg;
		break; }
	case XMP_CTL_MIXER_MIX: {
		int arg = va_arg(ap, int);
		s->mix = arg;
		break; }
	default:
		ret = -XMP_ERROR_INTERNAL;
	}

	va_end(ap);

	return ret;
}

char **xmp_get_format_list()
{
	return format_list();
}

void xmp_inject_event(xmp_context opaque, int channel, struct xmp_event *e)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;

	memcpy(&p->inject_event[channel], e, sizeof(struct xmp_event));
	p->inject_event[channel]._flag = 1;
}
 
