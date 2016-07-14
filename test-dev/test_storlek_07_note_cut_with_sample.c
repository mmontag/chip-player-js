#include "test.h"

/*
07 - Note cut with sample

Some players ignore sample numbers next to a note cut. When handled correctly,
this test should play a square wave, cut it, and then play the noise sample.

If this test is not handled correctly, make sure samples are checked regardless
of the note's value.
*/

TEST(test_storlek_07_note_cut_with_sample)
{
	compare_mixer_data(
		"data/storlek_07.it",
		"data/storlek_07.data");
}
END_TEST
