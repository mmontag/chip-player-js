/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "common.h"
#include "player.h"
#include "virtual.h"
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

void hmn_set_arpeggio(struct channel_data *xc, int arp)
{
	memcpy(xc->arpeggio.val, megaarp[arp], 16);
	xc->arpeggio.size = 16;
}

void hmn_extras(struct context_data *ctx, int chn, struct channel_data *xc,
		int new_note)
{
	struct module_data *m = &ctx->m;
	struct xmp_instrument *xxi;
	int pos, waveform, volume;

	if (new_note) {
		xc->extra.hmn.datapos = 0;
	}

	xxi = &m->mod.xxi[xc->ins];
	pos = xc->extra.hmn.datapos;
	waveform = HMN_EXTRA(m->mod.xxi[xc->ins])->data[pos];
	volume = HMN_EXTRA(m->mod.xxi[xc->ins])->progvolume[pos] & 0x7f;

	if (waveform < xxi->nsm && xxi->sub[waveform].sid != xc->smp) {
		xc->smp = xxi->sub[waveform].sid;
		virt_setsmp(ctx, chn, xc->smp);
	}

	pos++;
	if (pos > HMN_EXTRA(m->mod.xxi[xc->ins])->dataloopend)
		pos = HMN_EXTRA(m->mod.xxi[xc->ins])->dataloopstart;

	xc->extra.hmn.datapos = pos;
	xc->extra.hmn.volume = volume;
}
