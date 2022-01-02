#include "test.h"

TEST(test_effect_far_vibrato_per)
{
	compare_mixer_data(
		"data/far_effect9.far",
		"data/far_effect9.data");
}
END_TEST
