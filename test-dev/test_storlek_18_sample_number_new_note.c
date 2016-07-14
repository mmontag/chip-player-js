#include "test.h"

/*
18 - Sample number causes new note

A sample number encountered when no note is playing should restart the last
sample played, and at the same pitch. Additionally, a note cut should clear the
channel's state, thereby disabling this behavior.

This song should play six notes, with the last three an octave higher than the
first three.
*/

TEST(test_storlek_18_sample_number_new_note)
{
	compare_mixer_data(
		"data/storlek_18.it",
		"data/storlek_18.data");
}
END_TEST
