#include "test.h"

/*
 If an arpeggio parameter exceeds the Amiga frequency range, ProTracker wraps
 it around. In fact, the first note that is too high (which would be C-7 in
 OpenMPT or C-4 in ProTracker) becomes inaudible, C#7 / C#4 becomes C-4 / C-1,
 and so on. OpenMPT won't play the first case correctly, but that's rather
 complicated to emulate and probably not all that important.
*/

TEST(test_openmpt_mod_arpwraparound)
{
	compare_mixer_data(
		"openmpt/mod/ArpWraparound.mod",
		"openmpt/mod/ArpWraparound.data");
}
END_TEST
