/* Extended Module Player
 * Copyright (C) 1997-2011 Claudio Matsuoka and Hipolito Carraro Jr
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

static void synth_setpatch(int c, uint8 *data)
{
}

static void synth_setnote(int c, int note, int bend)
{
}

static void synth_setvol(int c, int vol)
{
}

static int synth_init(int freq)
{
	return 0;
}

static int synth_deinit()
{
	return 0;
}

static int synth_reset()
{
	return 0;
}

static void synth_mixer(int *tmp_bk, int count, int vl, int vr, int stereo)
{
}

struct xmp_synth_info synth_null = {
	synth_init,
	synth_deinit,
	synth_reset,
	synth_setpatch,
	synth_setnote,
	synth_setvol,
	synth_mixer
};
