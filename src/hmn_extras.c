/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include <stdlib.h>
#include "common.h"
#include "player.h"
#include "virtual.h"
#include "effects.h"
#include "hmn_extras.h"

static uint8 megaarp[16][16] = {
	{  0,  3,  7, 12, 15, 12,  7,  3,  0,  3,  7, 12, 15, 12,  7,  3 },
	{  0,  4,  7, 12, 16, 12,  7,  4,  0,  4,  7, 12, 16, 12,  7,  4 },
	{  0,  3,  8, 12, 15, 12,  8,  3,  0,  3,  8, 12, 15, 12,  8,  3 },
	{  0,  4,  8, 12, 16, 12,  8,  4,  0,  4,  8, 12, 16, 12,  8,  4 },
	{  0,  5,  8, 12, 17, 12,  8,  5,  0,  5,  8, 12, 17, 12,  8,  5 },
	{  0,  5,  9, 12, 17, 12,  9,  5,  0,  5,  9, 12, 17, 12,  9,  5 },
	{ 12,  0,  7,  0,  3,  0,  7,  0, 12,  0,  7,  0,  3,  0,  7,  0 },
	{ 12,  0,  7,  0,  4,  0,  7,  0, 12,  0,  7,  0,  4,  0,  7,  0 },

	{  0,  3,  7,  3,  7, 12,  7, 12, 15, 12,  7, 12,  7,  3,  7,  3 },
	{  0,  4,  7,  4,  7, 12,  7, 12, 16, 12,  7, 12,  7,  4,  7,  4 },
	{ 31, 27, 24, 19, 15, 12,  7,  3,  0,  3,  7, 12, 15, 19, 24, 27 },
	{ 31, 28, 24, 19, 16, 12,  7,  4,  0,  4,  7, 12, 16, 19, 24, 28 },
	{  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12,  0, 12 },
	{  0, 12, 24, 12,  0, 12, 24, 12,  0, 12, 24, 12,  0, 12, 24, 12 },
	{  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3,  0,  3 },
	{  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4,  0,  4 }
};


int hmn_linear_bend(struct context_data *ctx, struct channel_data *xc)
{
	return 0;
}

void hmn_play_extras(struct context_data *ctx, struct channel_data *xc, int chn,
		int new_note)
{
	struct module_data *m = &ctx->m;
	struct hmn_channel_extras *ce = xc->extra;
	struct xmp_instrument *xxi;
	int pos, waveform, volume;

	if (new_note) {
		ce->datapos = 0;
	}

	xxi = &m->mod.xxi[xc->ins];
	pos = ce->datapos;
	waveform = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->data[pos];
	volume = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->progvolume[pos] & 0x7f;

	if (waveform < xxi->nsm && xxi->sub[waveform].sid != xc->smp) {
		xc->smp = xxi->sub[waveform].sid;
		virt_setsmp(ctx, chn, xc->smp);
	}

	pos++;
	if (pos > HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->dataloopend)
		pos = HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins])->dataloopstart;

	ce->datapos = pos;
	ce->volume = volume;
}

int hmn_new_instrument_extras(struct xmp_instrument *xxi)
{
	xxi->extra = calloc(1, sizeof(struct hmn_instrument_extras));
	if (xxi->extra == NULL)
		return -1;
	HMN_INSTRUMENT_EXTRAS((*xxi))->magic = HMN_EXTRAS_MAGIC;

	return 0;
}

int hmn_new_channel_extras(struct channel_data *xc)
{
	xc->extra = calloc(1, sizeof(struct hmn_channel_extras));
	if (xc->extra == NULL)
		return -1;
	HMN_CHANNEL_EXTRAS((*xc))->magic = HMN_EXTRAS_MAGIC;

	return 0;
}

void hmn_reset_channel_extras(struct channel_data *xc)
{
	memset((char *)xc->extra + 4, 0, sizeof(struct hmn_channel_extras) - 4);
}

void hmn_release_channel_extras(struct channel_data *xc)
{
	free(xc->extra);
}

int hmn_new_module_extras(struct module_data *m)
{
	m->extra = calloc(1, sizeof(struct hmn_module_extras));
	if (m->extra == NULL)
		return -1;
	HMN_MODULE_EXTRAS((*m))->magic = HMN_EXTRAS_MAGIC;

	return 0;
}

void hmn_release_module_extras(struct module_data *m)
{
	free(m->extra);
}

void hmn_extras_process_fx(struct context_data *ctx, struct channel_data *xc,
			   int chn, uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	switch (fxt) {
	case FX_MEGAARP:
		memcpy(xc->arpeggio.val, megaarp[fxp], 16);
		xc->arpeggio.size = 16;
		break;
	}
}
