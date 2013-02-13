/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "xmp.h"
#include "common.h"
#include "synth.h"


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

const struct synth_info synth_null = {
	synth_init,
	synth_deinit,
	synth_reset,
};
