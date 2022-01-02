#include "test.h"

/* Test Ultra Tracker persistent tone portamento.
 */

TEST(test_effect_ult_toneporta)
{
	compare_mixer_data(
		"data/porta.ult",
		"data/porta_ult.data");
}
END_TEST
