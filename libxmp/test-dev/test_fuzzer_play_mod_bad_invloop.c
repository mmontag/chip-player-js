#include "test.h"

/* This MOD attempts to use the invert loop effect on bad samples.
 */

TEST(test_fuzzer_play_mod_bad_invloop)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_mod_bad_invloop.mod", sequence, 4000, 0, 0);
}
END_TEST
