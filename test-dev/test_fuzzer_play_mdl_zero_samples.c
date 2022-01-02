#include "test.h"

/* This input caused crashes in read_event_ft2 due to a missing check
 * on subinstrument sample IDs greater than the module sample count.
 */

TEST(test_fuzzer_play_mdl_zero_samples)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_mdl_zero_samples.mdl", sequence, 4000, 0, 0);
}
END_TEST
