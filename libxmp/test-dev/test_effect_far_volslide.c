#include "test.h"

TEST(test_effect_far_volslide)
{
	compare_mixer_data(
		"data/far_effectA.far",
		"data/far_effectA.data");
}
END_TEST
