#include "test.h"

/*
 Like many other trackers, ProTracker does not advance the vibrato position
 on the first tick of the row. However, it also does not apply the vibrato
 offset on the first tick, which results in somewhat funky-sounding vibratos.
 OpenMPT uses this behaviour only in ProTracker 1/2 mode. The same applies to
 the tremolo effect.
*/

TEST(test_openmpt_mod_vibratoreset)
{
	compare_mixer_data(
		"openmpt/mod/VibratoReset.mod",
		"openmpt/mod/VibratoReset.data");
}
END_TEST
