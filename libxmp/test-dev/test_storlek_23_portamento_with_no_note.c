#include "test.h"

/*
23 - Portamento with no note

Stray portamento effects with no target note should do nothing. Relatedly, a
portamento should clear the target note when it is reached.

The first section of this test should first play the same increasing tone three
times, with the last GFF effect not resetting the note to the base frequency;
the next part should play two rising tones at different pitches, and finish an
octave lower than it started.
*/

TEST(test_storlek_23_portamento_with_no_note)
{
	compare_mixer_data(
		"data/storlek_23.it",
		"data/storlek_23.data");
}
END_TEST
