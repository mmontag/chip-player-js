#include "test.h"

/*
 ProTracker’s portamento behaviour is somewhere between FT2 and IT:

 - A new note (with no portamento command next to it) does not reset the
   portamento target. That is, if a previous portamento has not finished yet,
   calling 3xx or 5xx after the new note will slide it towards the old target.
 - Once the portamento target period is reached, the target is reset. This
   means that if the period is modified by another slide (e.g. 1xx or 2xx),
   a following 3xx will not slide back to the original target.
*/

TEST(test_openmpt_mod_portatarget)
{
	compare_mixer_data(
		"openmpt/mod/PortaTarget.mod",
		"openmpt/mod/PortaTarget.data");
}
END_TEST
