#include "test.h"

/* This input caused crashes in the mixer due to reading invalid BPMs
 * of 0 from invalid orders. The root cause of this was xmp_start_player
 * resetting p->ord for 0 channel modules(?!) combined with an invalid
 * pattern at order 0 (which is normally skipped prior to that check).
 */

TEST(test_fuzzer_play_med4_0_chn_invalid_ord)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_med4_0_chn_invalid_ord.med", sequence, 4000, 0, 0);
}
END_TEST
