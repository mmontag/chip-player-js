/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdlib.h>
#include "common.h"
#include "period.h"
#include "player.h"
#include "hio.h"

int xmp_sfx_init(xmp_context opaque, int chn, int smp)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;

	if (ctx->state > XMP_STATE_LOADED)
		return -XMP_ERROR_STATE;

	sfx->xxi = calloc(sizeof (struct xmp_instrument), smp);
	sfx->xxs = calloc(sizeof (struct xmp_sample), smp);

	sfx->chn = chn;
	sfx->ins = sfx->smp = smp;

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
	event->note = note + 1;
	event->ins = ins + 1;
	event->vol = vol + 1;
	event->_flag = 1;

	return 0;
}

int xmp_sfx_play_sample(xmp_context opaque, int ins, int vol, int chn)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;
	struct module_data *m = &ctx->m;
	struct xmp_module *mod = &m->mod;

	if (chn >= sfx->chn || ins >= sfx->ins)
		return -XMP_ERROR_INVALID;

	return xmp_sfx_play_instrument(opaque, mod->ins + ins, 60, vol, 0);
}

int xmp_sfx_channel_pan(xmp_context opaque, int chn, int pan)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct player_data *p = &ctx->p;
	struct sfx_data *sfx = &ctx->sfx;
	struct module_data *m = &ctx->m;
	struct channel_data *xc;

	if (chn >= sfx->chn || pan < 0 || pan > 255)
		return -XMP_ERROR_INVALID;

	xc = &p->xc_data[m->mod.chn + chn];
	xc->masterpan = pan;

	return 0;
}

int xmp_sfx_load_sample(xmp_context opaque, int num, char *path)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;
	struct module_data *m = &ctx->m;
	struct xmp_instrument *xxi = &sfx->xxi[num];
	struct xmp_sample *xxs = &sfx->xxs[num];
	HIO_HANDLE *h;
	uint32 magic;
	int chn, rate, bits, size;

	h = hio_open_file(path, "rb");
	if (h == NULL)
		goto err;
		
	/* Init instrument */

	xxi->sub = calloc(sizeof(struct xmp_subinstrument), 1);
	if (xxi->sub == NULL)
		goto err1;

	xxi->vol = m->volbase;
	xxi->nsm = 1;
	xxi->sub[0].sid = num;
	xxi->sub[0].vol = xxi->vol;
	xxi->sub[0].pan = 0x80;

	/* Load sample */

	magic = hio_read32b(h);
	if (magic != 0x52494646)	/* RIFF */
		goto err2;

	hio_seek(h, 22, SEEK_SET);
	chn = hio_read16l(h);
	if (chn != 1)
		goto err2;

	rate = hio_read32l(h);

	hio_seek(h, 34, SEEK_SET);
	bits = hio_read16l(h);

	hio_seek(h, 40, SEEK_SET);
	size = hio_read32l(h) / (bits / 8);

	c2spd_to_note(rate, &xxi->sub[0].xpo, &xxi->sub[0].fin);

	xxs->len = 8 * size / bits;
	xxs->lps = 0;
	xxs->lpe = 0;
	xxs->flg = bits == 16 ? XMP_SAMPLE_16BIT : 0;

	xxs->data = malloc(size);
	if (xxs->data == NULL)
		goto err2;

	hio_seek(h, 44, SEEK_SET);
	hio_read(xxs->data, 1, size, h);
	hio_close(h);

	return 0;
	
    err2:
	free(xxi->sub);
    err1:
	hio_close(h);
    err:
	return -XMP_ERROR_INTERNAL;
}

void xmp_sfx_release_sample(xmp_context opaque, int num)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;

	free(sfx->xxs[num].data);
	free(sfx->xxi[num].sub);
}

void xmp_sfx_deinit(xmp_context opaque)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct sfx_data *sfx = &ctx->sfx;
	int i;

	for (i = 0; i < sfx->smp; i++)
		xmp_sfx_release_sample(opaque, i);

	free(sfx->xxs);
	free(sfx->xxi);
}
