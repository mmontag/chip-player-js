#include "test.h"

/*
 Two samples played back at the same frequency should always step
 through their sample frames at the same rate, regardless of the
 lengths of their loops. Example: a sample with a loop of 16 frames
 and a sample with a loop of 32 frames should repeat their loops at
 exactly a rate of 2:1 if played at the same frequency.
*/

TEST(test_player_sample_sync)
{
	compare_mixer_data(
		"data/sample_sync.it",
		"data/sample_sync.data");
}
END_TEST
