#include "test.h"

/*
22 - Zero value for note cut and note delay

Impulse Tracker handles SD0 and SC0 as SD1 and SC1, respectively. (As a side
note, Scream Tracker 3 ignores notes with SD0 completely, and doesn't cut notes
at all with SC0.)

If these effects are handled correctly, the notes on the first row should
trigger simultaneously; the next pair of notes should not; and the final two
sets should both play identically and cut the notes after playing for one tick.
*/

TEST(test_storlek_22_zero_cut_and_delay)
{
	compare_mixer_data(
		"data/storlek_22.it",
		"data/storlek_22.data");
}
END_TEST
