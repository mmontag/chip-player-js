#include "test.h"

/*
 An instrument number with no note or next to a ^^^ should always be remembered
 for the next instrument-less note, even if sample playback is stopped.
*/

TEST(test_openmpt_it_stoppedinstrswap)
{
	compare_mixer_data(
		"openmpt/it/StoppedInstrSwap.it",
		"openmpt/it/StoppedInstrSwap.data");
}
END_TEST
