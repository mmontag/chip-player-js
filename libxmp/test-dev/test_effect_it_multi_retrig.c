#include "test.h"

/* Tests each volume slide value for multi retrigger (Qxy)
 * and also changing both the retrigger rate and volume slide
 * rate for a playing note.
 */

TEST(test_effect_it_multi_retrig)
{
	compare_mixer_data(
		"data/it_multi_retrigger.it",
		"data/it_multi_retrigger.data");
}
END_TEST
