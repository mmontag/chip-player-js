#include "test.h"

/*
 Fade-Porta.it already tests the general case of portamento picking up a
 stopped note (portamento should just be ignored in this case), but there is
 an edge case when the note just stopped on the previous tick. In this case,
 OpenMPT did previously behave differently and still execute the portamento
 effect.
*/

TEST(test_openmpt_it_portajuststoppednote)
{
	compare_mixer_data(
		"openmpt/it/PortaJustStoppedNote.it",
		"openmpt/it/PortaJustStoppedNote.data");
}
END_TEST
