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
#include "extras.h"
#include "med_extras.h"
#include "hmn_extras.h"

/*
 * Module extras
 */

void release_module_extras(struct context_data *ctx)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_MODULE_EXTRAS(*m))
		med_release_module_extras(m);
	else if (HAS_HMN_MODULE_EXTRAS(*m))
		hmn_release_module_extras(m);
}

/*
 * Channel extras
 */

int new_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_MODULE_EXTRAS(*m)) {
		if (med_new_channel_extras(xc) < 0)
			return -1;
	} else if (HAS_HMN_MODULE_EXTRAS(*m)) {
		if (hmn_new_channel_extras(xc) < 0)
			return -1;
	}

	return 0;
}

void release_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_CHANNEL_EXTRAS(*m))
		med_release_channel_extras(xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*m))
		hmn_release_channel_extras(xc);
}

void reset_channel_extras(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;

	if (HAS_MED_CHANNEL_EXTRAS(*m))
		med_reset_channel_extras(xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*m))
		hmn_reset_channel_extras(xc);
}

/*
 * Player extras
 */

void play_extras(struct context_data *ctx, struct channel_data *xc, int chn, int new_note)
{
	struct module_data *m = &ctx->m;

	if (xc->ins >= m->mod.ins)	/* SFX instruments have no extras */
		return;

        if (HAS_MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		med_play_extras(ctx, xc, chn, new_note);
        else if (HAS_HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		hmn_play_extras(ctx, xc, chn, new_note);
}

int extras_get_volume(struct context_data *ctx, struct channel_data *xc)
{
	struct module_data *m = &ctx->m;
	int vol;

	if (xc->ins >= m->mod.ins)
		vol = xc->volume;
        else if (HAS_MED_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		vol = MED_CHANNEL_EXTRAS(*xc)->volume * xc->volume / 64;
	else if (HAS_HMN_INSTRUMENT_EXTRAS(m->mod.xxi[xc->ins]))
		vol = HMN_CHANNEL_EXTRAS(*xc)->volume * xc->volume / 64;
	else
		vol = xc->volume;

	return vol;
}

int extras_get_period(struct context_data *ctx, struct channel_data *xc)
{
	int period;

	if (HAS_MED_CHANNEL_EXTRAS(*xc))
		period = med_change_period(ctx, xc);
	else period = 0;

	return period;
}

int extras_get_linear_bend(struct context_data *ctx, struct channel_data *xc)
{
	int linear_bend;

	if (HAS_MED_CHANNEL_EXTRAS(*xc))
		linear_bend = med_linear_bend(ctx, xc);
	else if (HAS_HMN_CHANNEL_EXTRAS(*xc))
		linear_bend = hmn_linear_bend(ctx, xc);
	else
		linear_bend = 0;

	return linear_bend;
}

void extras_process_fx(struct context_data *ctx, struct channel_data *xc,
			int chn, uint8 note, uint8 fxt, uint8 fxp, int fnum)
{
	if (HAS_HMN_CHANNEL_EXTRAS(*xc))
		hmn_extras_process_fx(ctx, xc, chn, note, fxt, fxp, fnum);
}
