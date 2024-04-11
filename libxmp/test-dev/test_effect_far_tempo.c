#include "test.h"

TEST(test_effect_far_tempo)
{
	compare_mixer_data(
		"data/far_effectF.far",
		"data/far_effectF.data");
}
END_TEST
