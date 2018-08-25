#include "test.h"

/*
 Note Off + instrument / portamento combinations.
*/

TEST(test_openmpt_it_wnoteoff)
{
	compare_mixer_data(
		"openmpt/it/wnoteoff.it",
		"openmpt/it/wnoteoff.data");
}
END_TEST
