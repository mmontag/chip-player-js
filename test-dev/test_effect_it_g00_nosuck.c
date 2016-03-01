#include "test.h"

/*
 Tone portamento speed should be set even if there's no target note.
*/

TEST(test_effect_it_g00_nosuck)
{
	compare_mixer_data(
		"data/G00_nosuck.it",
		"data/G00_nosuck.data");
}
END_TEST
