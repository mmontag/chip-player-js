#include "test.h"

/* This input caused out-of-bounds reads in the tone portamento
 * effects handlers due to missing validity checks on the current
 * key and portamento key values, which can be -1.
 */

TEST(test_fuzzer_play_stm_bad_note_toneporta)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	3, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_stm_bad_note_toneporta.stm", sequence, 4000, 0, 0);
}
END_TEST
