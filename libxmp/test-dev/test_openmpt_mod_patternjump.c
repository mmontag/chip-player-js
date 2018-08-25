#include "test.h"

/*
 A Bxx command should reset the effect of a Dxx command that is left of
 the Bxx command. You should hear a voice saying “success”.
*/

TEST(test_openmpt_mod_patternjump)
{
	compare_mixer_data(
		"openmpt/mod/PatternJump.mod",
		"openmpt/mod/PatternJump.data");
}
END_TEST
