/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "xmp.h"
#include "common.h"
#include "synth.h"

static void synth_setpatch(struct context_data *ctx, int c, uint8 *data)
{
}

static void synth_setnote(struct context_data *ctx, int c, int note, int bend)
{
}

static void synth_setvol(struct context_data *ctx, int c, int vol)
{
}

static void synth_seteffect(struct context_data *ctx, int c, int type, int val)
{
}

static int synth_init(struct context_data *ctx, int freq)
{
	return 0;
}

static int synth_deinit(struct context_data *ctx)
{
	return 0;
}

static int synth_reset(struct context_data *ctx)
{
	return 0;
}

static void synth_mixer(struct context_data *ctx, int *tmp_bk, int count, int vl, int vr, int stereo)
{
}

const struct synth_info synth_null = {
	synth_init,
	synth_deinit,
	synth_reset,
	synth_setpatch,
	synth_setnote,
	synth_setvol,
	synth_seteffect,
	synth_mixer
};
