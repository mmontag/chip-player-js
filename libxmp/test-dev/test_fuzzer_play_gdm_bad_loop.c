#include "test.h"

/* This input caused invalid double to integer conversions due to using
 * bad loop values for invalid (unloaded) samples. These would result in
 * negative integer frame sample counts and other oddities.
 */

TEST(test_fuzzer_play_gdm_bad_loop)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	3, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_gdm_bad_loop.gdm", sequence, 4000, 0, 0);
}
END_TEST
