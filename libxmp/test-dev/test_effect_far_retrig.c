#include "test.h"

TEST(test_effect_far_retrig)
{
	compare_mixer_data(
		"data/far_effect4.far",
		"data/far_effect4.data");
}
END_TEST
