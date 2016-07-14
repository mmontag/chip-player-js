#include "test.h"

/*
04 - Compatible Gxx on

If this test is played correctly, the first note will slide up and back down
once, and the final series should play four distinct notes. If the first note
slides up again, either (a) the player is testing the flag incorrectly (Gxx
memory is only linked if the flag is not set), or (b) the effect memory values
are not set to zero at start of playback.
*/

TEST(test_storlek_04_compatible_gxx_on)
{
	compare_mixer_data(
		"data/storlek_04.it",
		"data/storlek_04.data");
}
END_TEST
