#include "test.h"

/*
08 - Out-of-range note delays

This test is to make sure note delay is handled correctly if the delay value
is out of range. The correct behavior is to act as if the entire row is empty.
Some players ignore the delay value and play the note on the first tick.

Oddly, Impulse Tracker does save the instrument number, even if the delay value
is out of range. I'm assuming this is a bug; nevertheless, if a player is going
to claim 100% IT compatibility, it needs to copy the bugs as well.

When played correctly, this should play the first three notes using the square
wave sample, with equal time between the start and end of each note, and the
last note should be played with the noise sample.
*/

TEST(test_storlek_08_out_of_range_delays)
{
	compare_mixer_data(
		"data/storlek_08.it",
		"data/storlek_08.data");
}
END_TEST
