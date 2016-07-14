#include "test.h"

/*
05 - Gxx, fine slides, effect memory

(Note: make sure the Compatible Gxx flag is handled correctly before performing
this test.)

EFx and FFx are handled as fine slides even if the effect value was set by a
Gxx effect. If this test is played correctly, the pitch should bend downward a
full octave, and then up almost one semitone. If the note is bent way upward
(and possibly out of control, causing the note to stop), the player is not
handling the effect correctly.
*/

TEST(test_storlek_05_gxx_fine_slides_memory)
{
	compare_mixer_data(
		"data/storlek_05.it",
		"data/storlek_05.data");
}
END_TEST
