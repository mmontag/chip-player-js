#include "test.h"

/* This IT uses loop (SB2) and row delay (SEE) on the very first row
 * of the module. This actually crashed libxmp (in ASan)!
 */

TEST(test_fuzzer_play_it_row_0_loop_row_delay)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	8, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_it_row_0_loop_row_delay.it", sequence, 4000, 0, 0);
}
END_TEST
