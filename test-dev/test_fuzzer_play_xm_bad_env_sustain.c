#include "test.h"

/* This XM has a bad sustain point. Previously, libxmp turned off
 * the entire envelope when encountering these, but correcting this
 * to only turn off sustain revealed a badly guarded check.
 */

TEST(test_fuzzer_play_xm_bad_env_sustain)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	4, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_xm_bad_env_sustain.xm", sequence, 4000, 0, 0);
}
END_TEST
