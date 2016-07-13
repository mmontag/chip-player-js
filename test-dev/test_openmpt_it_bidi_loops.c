#include "test.h"

/*
 In Impulse Tracker’s software mixer, ping-pong loops are shortened by one
 sample. This does not happen with the GUS hardware driver, but I assume that
 the software drivers were more popular due to the limitations of the GUS, so
 OpenMPT emulates this behaviour.
*/

TEST(test_openmpt_it_bidi_loops)
{
	compare_mixer_data(
		"openmpt/it/Bidi-Loops.it",
		"openmpt/it/Bidi-Loops.data");
}
END_TEST
