/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "common.h"
#include "player.h"

int xmp_sfx_channels(xmp_context opaque, int num)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;

	if (ctx->state > XMP_STATE_LOADED)
		return -XMP_ERROR_STATE;

	sfx->chn = num;

	return 0;
}

int xmp_sfx_play_instrument(xmp_context opaque, int ins, int note, int vol, int chn)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct sfx_data *sfx = &ctx->sfx;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;
	struct xmp_event *event;

	if (ctx->state < XMP_STATE_PLAYING)
		return -XMP_ERROR_STATE;

	if (chn >= sfx->chn)
		return -XMP_ERROR_INVALID;

	event = &p->inject_event[mod->chn + chn];
	memset(event, 0, sizeof (struct xmp_event));
	event->note = note;
	event->ins = ins;
	event->vol = vol;
	event->_flag = 1;

	return 0;
}

int xmp_sfx_play_sample(xmp_context opaque, int ins, int vol, int chn)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	if (chn >= sfx->chn || ins >= m->res_ins)
		return -XMP_ERROR_INVALID;

	return xmp_sfx_play_instrument(opaque, mod->chn + chn, 61,
						mod->ins + ins, vol);
}

