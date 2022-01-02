#include "test.h"

/* Similar to the OpenMPT test FineVolColSlide.it but this test
 * makes sure multiple channels can volslide during a pattern delay.
 * The prior handling only allowed the first channel to apply the
 * volslide.
 */

TEST(test_effect_it_fine_vol_row_delay)
{
	compare_mixer_data(
		"data/FineVolRowDelayMultiple.it",
		"data/FineVolRowDelayMultiple.data");
}
END_TEST
