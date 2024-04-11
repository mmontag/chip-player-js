#include "test.h"

TEST(test_effect_far_noteoffset)
{
	compare_mixer_data(
		"data/far_effectC.far",
		"data/far_effectC.data");
}
END_TEST
