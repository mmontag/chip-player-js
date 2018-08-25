#include "test.h"

/*
 Even if a tremor effect muted the sample on a previous row, volume commands
 should be able to override this effect. Instrument numbers should reset the
 tremor effect as well. Currently, OpenMPT only supports the latter.
*/

TEST(test_openmpt_xm_tremorrecover)
{
	compare_mixer_data(
		"openmpt/xm/TremorRecover.xm",
		"openmpt/xm/TremorRecover.data");
}
END_TEST
