#include "test.h"

/*
 Any kind of Note Cut (SCx or ^^^) should stop the sample and not set its
 volume to 0. A subsequent volume command cannot continue the sample, but a
 note, even without an instrument number can do so. When played back correctly,
 the module should stay silent.
*/

TEST(test_openmpt_it_noteoffinstr)
{
	compare_mixer_data(
		"openmpt/it/NoteOffInstr.it",
		"openmpt/it/NoteOffInstr.data");
}
END_TEST
