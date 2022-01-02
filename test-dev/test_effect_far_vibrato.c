#include "test.h"

TEST(test_effect_far_vibrato)
{
	compare_mixer_data(
		"data/far_effect6.far",
		"data/far_effect6.data");
}
END_TEST
