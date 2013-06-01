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
#include "hmnt_extras.h"

void hmnt_synth(struct context_data *ctx, int chn, struct channel_data *xc,
		int new_note)
{
	struct module_data *m = &ctx->m;
	struct xmp_instrument *xxi;
	int pos, waveform;


	if (m->mod.xxi[xc->ins].extra == NULL ||
		HMNT_EXTRA(m->mod.xxi[xc->ins])->magic != HMNT_EXTRAS_MAGIC)
		return;

	if (new_note) {
		xc->extra.hmnt.datapos = 0;
	}

	xxi = &m->mod.xxi[xc->ins];
	pos = xc->extra.hmnt.datapos;
	waveform = HMNT_EXTRA(m->mod.xxi[xc->ins])->data[pos];

	if (waveform < xxi->nsm && xxi->sub[waveform].sid != xc->smp) {
		xc->smp = xxi->sub[waveform].sid;
		virt_setsmp(ctx, chn, xc->smp);
	}

	pos++;
	if (pos > HMNT_EXTRA(m->mod.xxi[xc->ins])->dataloopend)
		pos = HMNT_EXTRA(m->mod.xxi[xc->ins])->dataloopstart;

	xc->extra.hmnt.datapos = pos;
}
