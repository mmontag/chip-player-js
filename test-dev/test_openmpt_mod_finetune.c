#include "test.h"

/*
 ProTracker’s E5x finetune handling is a bit weird. It is also evaluated if
 there is no note next to the command, and the command is also affected by
 3xx portamentos. 
*/

TEST(test_openmpt_mod_finetune)
{
	compare_mixer_data(
		"openmpt/mod/finetune.mod",
		"openmpt/mod/finetune.data");
}
END_TEST
