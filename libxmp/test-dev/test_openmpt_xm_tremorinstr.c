#include "test.h"

/*
 Instrument numbers reset tremor count.
*/

TEST(test_openmpt_xm_tremorinstr)
{
	compare_mixer_data(
		"openmpt/xm/TremorInstr.xm",
		"openmpt/xm/TremorInstr.data");
}
END_TEST
