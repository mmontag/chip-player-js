#include "test.h"

/* This IT uses a sample with a bidi sustain loop and a non-bidi regular loop.
 * This was able to cause crashes in libxmp due to its bad bidi and sustain support.
 */

TEST(test_fuzzer_play_it_sustain_bidi)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	2, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_it_sustain_bidi.it", sequence, 4000, 0, 0);
}
END_TEST
