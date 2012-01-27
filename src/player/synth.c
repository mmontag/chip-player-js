/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xmp.h"
#include "common.h"
#include "synth.h"

static void synth_setpatch(struct xmp_context *ctx, int c, uint8 *data)
{
}

static void synth_setnote(struct xmp_context *ctx, int c, int note, int bend)
{
}

static void synth_setvol(struct xmp_context *ctx, int c, int vol)
{
}

static void synth_seteffect(struct xmp_context *ctx, int c, int type, int val)
{
}

static int synth_init(struct xmp_context *ctx, int freq)
{
	return 0;
}

static int synth_deinit(struct xmp_context *ctx)
{
	return 0;
}

static int synth_reset(struct xmp_context *ctx)
{
	return 0;
}

static void synth_mixer(struct xmp_context *ctx, int *tmp_bk, int count, int vl, int vr, int stereo)
{
}

struct xmp_synth_info synth_null = {
	synth_init,
	synth_deinit,
	synth_reset,
	synth_setpatch,
	synth_setnote,
	synth_setvol,
	synth_seteffect,
	synth_mixer
};
