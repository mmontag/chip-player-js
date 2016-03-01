#include "test.h"

/*
 Tone portamento speed should be set even if there's no target note.
*/

TEST(test_effect_it_l00_nosuck)
{
	compare_mixer_data(
		"data/L00_nosuck.it",
		"data/L00_nosuck.data");
}
END_TEST
