#include "test.h"

/*
 The autovibrato sweep is not reset when using portamento.
*/

TEST(test_openmpt_it_autovibrato_reset)
{
	compare_mixer_data(
		"openmpt/it/Autovibrato-Reset.it",
		"openmpt/it/Autovibrato-Reset.data");
}
END_TEST
