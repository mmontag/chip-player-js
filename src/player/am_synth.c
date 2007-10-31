/* Extended Module Player
 * Copyright (C) 1996-2007 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xmpi.h"
#include "player.h"

/*
 * Envelope parameters:
 *
 * FQ    This waves base frequency. $1 is very low, $4 is average and $20 is
 *       quite high.
 * L0    Start amplitude for the envelope
 * A1L   Attack level
 * A1S   The speed that the amplitude changes to the attack level, $1 is slow
 *       and $40 is fast.
 * A2L   Secondary attack level, for those who likes envelopes...
 * A2S   Secondary attack speed.
 * DS    The speed that the amplitude decays down to the:
 * SL    Sustain level. There is remains for the time set by the
 * ST    Sustain time.
 * RS    Release speed. The speed that the amplitude falls from ST to 0.
 */

void xmp_am_synth(struct xmp_context *ctx, int chn, struct xmp_channel *xc)
{
    struct xmp_player_context *p = &ctx->p;

}


/*
 * mt_amwaveforms:
 * 	dc.b	0,25,49,71,90,106,117,125
 * 	dc.b	127,125,117,106,90,71,49,25
 * 	dc.b	0,-25,-49,-71,-90,-106,-117
 * 	dc.b	-125,-127,-125,-117,-106
 * 	dc.b	-90,-71,-49,-25
 * 	dc.b	-128,-120,-112,-104,-96,-88,-80,-72,-64,-56,-48
 * 	dc.b	-40,-32,-24,-16,-8,0,8,16,24,32,40,48,56,64,72,80
 * 	dc.b	88,96,104,112,120
 * 	blk.b	16,-128
 * 	blk.b	16,127
 * mt_noisewave:
 * 	blk.b	32,0
 */

