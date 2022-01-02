#include "test.h"

/* This FAR attempts to cause libxmp to play a high BPM value using
 * old tempo mode. At very low sampling rates this previously crashed
 * the ramp length calculation.
 */

TEST(test_fuzzer_play_far_highbpm)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_far_highbpm.far", sequence, 4000, 0, 0);
}
END_TEST
