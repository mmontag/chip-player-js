#include "test.h"

TEST(test_effect_far_slide)
{
	compare_mixer_data(
		"data/far_effect1.far",
		"data/far_effect1.data");
}
END_TEST
