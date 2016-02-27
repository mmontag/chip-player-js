#include "test.h"

/*
 Enabling glissando should round (up or down) the result of a tone portamento
 to the next semitone. Normal portamento up and down effects are not affected
 by this.
*/

TEST(test_openmpt_xm_glissando)
{
	compare_mixer_data(
		"openmpt/xm/Glissando.xm",
		"openmpt/xm/Glissando.data");
}
END_TEST
