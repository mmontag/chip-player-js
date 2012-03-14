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

const unsigned int xmp_version = VERSION;

xmp_context xmp_create_context()
{
	struct context_data *ctx;

	ctx = calloc(1, sizeof(struct context_data));
	if (ctx == NULL) {
		return NULL;
	}

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

	if (dir == 0) {
		seq = get_sequence(ctx, pos);
	} else {
		seq = p->sequence;
	}

	start = m->sequence[seq].entry_point;

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
			p->pos = pos;
		}
	}
}

int _xmp_ctl(xmp_context opaque, int cmd, ...)
{
	va_list ap;
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct module_data *m = &ctx->m;
	struct mixer_data *s = &ctx->s;
	int ret = 0;

	va_start(ap, cmd);

	switch (cmd) {
	case XMP_CTL_ORD_PREV:
		if (p->pos == m->sequence[p->sequence].entry_point) {
			set_position(ctx, -1, -1);
		} else if (p->pos > m->sequence[p->sequence].entry_point) {
			set_position(ctx, p->pos - 1, -1);
		}
		ret = p->pos;
		break;
	case XMP_CTL_ORD_NEXT:
		if (p->pos < m->mod.len)
			set_position(ctx, p->pos + 1, 1);
		ret = p->pos;
		break;
	case XMP_CTL_ORD_SET: {
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
	case XMP_CTL_GVOL_DEC:
		if (p->gvol.volume > 0)
			p->gvol.volume--;
		ret = p->gvol.volume;
		break;
	case XMP_CTL_GVOL_INC:
		if (p->gvol.volume < 64)
			p->gvol.volume++;
		ret = p->gvol.volume;
		break;
	case XMP_CTL_SEEK_TIME: {
		int arg = va_arg(ap, int);
		int i, t;

		for (i = m->mod.len - 1; i >= 0; i--) {
			if (m->mod.xxo[i] >= m->mod.pat)
				continue;
			t = m->xxo_info[i].time;
			if (arg > t) {
				xmp_ord_set(opaque, i);
				break;
			}
		}
		if (i < 0) {
			xmp_ord_set(opaque, 0);
		}
		break; }
	case XMP_CTL_CH_MUTE: {
		int arg1 = va_arg(ap, int);
		int arg2 = va_arg(ap, int);
		virt_mute(ctx, arg1, arg2);
		break; }
	case XMP_CTL_MIXER_AMP: {
		int arg = va_arg(ap, int);
		s->amplify = arg;
		break; }
	case XMP_CTL_MIXER_MIX: {
		int arg = va_arg(ap, int);
		s->mix = arg;
		break; }
	case XMP_CTL_QUIRK_FX9:
		if (va_arg(ap, int)) {
			m->quirk |= QUIRK_FX9BUG;
		} else {
			m->quirk &= ~QUIRK_FX9BUG;
		}
		break;
	case XMP_CTL_QUIRK_FXEF:
		if (va_arg(ap, int)) {
			m->quirk |= QUIRK_FUNKIT;
		} else {
			m->quirk &= ~QUIRK_FUNKIT;
		}
		break;
	default:
		ret = -1;
	}

	va_end(ap);

	return 0;
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
	p->inject_event[channel].flag = 1;
}
 
