#include "test.h"

/* This input caused invalid double to integer conversions due to using
 * extremely low frequencies combined with PERIOD_CSPD.
 */

TEST(test_fuzzer_play_669_low_freq)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	5, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_669_low_freq.669", sequence, 4000, 0, 0);
}
END_TEST
