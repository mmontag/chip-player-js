#include "test.h"

/* This XM attempted to index invalid instrument -1 in an effect.
 */

TEST(test_fuzzer_play_xm_bad_instrument)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_xm_bad_instrument.xm", sequence, 4000, 0, 0);
}
END_TEST
