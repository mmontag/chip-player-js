#include "test.h"

TEST(test_effect_far_toneporta)
{
	compare_mixer_data(
		"data/far_effect3.far",
		"data/far_effect3.data");
}
END_TEST
