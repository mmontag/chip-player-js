#include "test.h"

/* This IT uses S74 Instrument Control/Set NNA Continue in sample mode,
 * which could crash libxmp instead of it being ignored.
 * This crash relied on a blank channel being available.
 */

TEST(test_fuzzer_play_it_bad_set_nna)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	4, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_it_bad_set_nna.it", sequence, 4000, 0, 0);
}
END_TEST
