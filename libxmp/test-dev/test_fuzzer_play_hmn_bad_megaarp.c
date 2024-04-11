#include "test.h"

/* This input caused out-of-bounds reads in the
 * His Master's Noise Mega-Arp effect handler.
 */

TEST(test_fuzzer_play_hmn_bad_megaarp)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_hmn_bad_megaarp.mod", sequence, 4000, 0, 0);
}
END_TEST
