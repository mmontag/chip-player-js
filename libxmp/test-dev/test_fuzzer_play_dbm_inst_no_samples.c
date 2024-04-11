#include "test.h"

/* This DBM plays an instrument with no attached sample and has
 * zero samples allocated. This should not crash.
 */

TEST(test_fuzzer_play_dbm_inst_no_samples)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_dbm_inst_no_samples.dbm", sequence, 4000, 0, 0);
}
END_TEST
