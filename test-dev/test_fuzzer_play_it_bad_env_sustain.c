#include "test.h"

/* This IT contains an instrument envelope with a sustain end value of
 * 255, well past the number of envelope points. libxmp should ignore
 * the envelope sustain in this case instead of crashing.
 */

TEST(test_fuzzer_play_it_bad_env_sustain)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	8, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_it_bad_env_sustain.it", sequence, 4000, 0, 0);
}
END_TEST
