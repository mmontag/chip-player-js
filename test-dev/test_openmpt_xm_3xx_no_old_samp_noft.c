#include "test.h"

/*
 Two tests in one: An offset effect that points beyond the sample end should
 stop playback on this channel. The note must not be picked up by further
 portamento effects.

 Skale Tracker doesn't emulate this FT2 quirk. This test changes the tracker
 ID to something not recognized as FT2-compatible. Armada Tanks game music
 doesn't play correctly with this quirk enabled.
*/

TEST(test_openmpt_xm_3xx_no_old_samp_noft)
{
	compare_mixer_data(
		"openmpt/xm/3xx-no-old-samp-noft.xm",
		"openmpt/xm/3xx-no-old-samp-noft.data");
}
END_TEST
