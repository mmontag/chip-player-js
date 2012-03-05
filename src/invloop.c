#include "common.h"
#include "player.h"

/* From http://www.un4seen.com/forum/?topic=7554.0
 *
 * "Invert loop" effect replaces (!) sample data bytes within loop with their
 * bitwise complement (NOT). The parameter sets speed of altering the samples.
 * This effectively trashes the sample data. Because of that this effect was
 * supposed to be removed in the very next ProTracker versions, but it was
 * never removed.
 *
 * Prior to [Protracker 1.1A] this effect is called "Funk Repeat" and it moves
 * loop of the instrument (just the loop information - sample data is not
 * altered). The parameter is the speed of moving the loop.
 */

static const int invloop_table[] = {
	0, 5, 6, 7, 8, 10, 11, 13, 16, 19, 22, 26, 32, 43, 64, 128
};

void update_invloop(struct module_data *m, struct channel_data *xc)
{
	struct xmp_sample *xxs = &m->mod.xxs[xc->smp];
	int len;

	xc->invloop.count += invloop_table[xc->invloop.speed];

	if ((xxs->flg & XMP_SAMPLE_LOOP) && xc->invloop.count >= 128) {
		xc->invloop.count = 0;
		len = xxs->lpe - xxs->lps;	

		if (HAS_QUIRK(QUIRK_FUNKIT)) {
			/* FIXME: not implemented */
		} else {
			if (++xc->invloop.pos > len) {
				xc->invloop.pos = 0;
			}

			if (~xxs->flg & XMP_SAMPLE_16BIT) {
				xxs->data[xxs->lps + xc->invloop.pos] ^= 0xff;
			}
		}
	}
}

