#include "test.h"

/* This ASYLUM module contains a bunch of invalid effects.
 * libxmp was previously loading these without any conversion and could
 * crash with a division by zero from ST 2.6 tempo effect misuse.
 */

TEST(test_fuzzer_play_asylum_bad_effects)
{
	static const struct playback_sequence sequence[] =
	{
		{ PLAY_FRAMES,	4, 0 },
		{ PLAY_END,	0, 0 }
	};
	compare_playback("data/f/play_asylum_bad_effects.amf", sequence, 4000, 0, 0);
}
END_TEST
