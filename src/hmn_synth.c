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

void hmn_synth(struct context_data *ctx, int chn, struct channel_data *xc,
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
