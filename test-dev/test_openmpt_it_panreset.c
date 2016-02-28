#include "test.h"

/*
 Contrary to XM, the default instrument and sample panning should only be
 reset when a note is encountered, not when an instrument number (without
 note) is encountered. The two channels of this module should be panned
 identically.
*/

TEST(test_openmpt_it_panreset)
{
	compare_mixer_data(
		"openmpt/it/PanReset.it",
		"openmpt/it/PanReset.data");
}
END_TEST
