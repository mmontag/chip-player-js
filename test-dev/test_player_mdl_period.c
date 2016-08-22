#include "test.h"

/*
Saga Musix reports:

"More frequency trouble!
 After the discovery of how frequency slides work in 669, I present some
 new findings about the MDL format: Frequency slides are not linear, but
 they are also independent of the middle-C frequency.

 Regardless of what the middle-C frequency is, portamentos and vibratos
 behave as if the middle-C frequency was 8363 Hz (i.e. like normal Amiga
 slides)."
*/

TEST(test_player_mdl_period)
{
	compare_mixer_data(
		"data/PERIOD.MDL",
		"data/period_mdl.data");
}
END_TEST
