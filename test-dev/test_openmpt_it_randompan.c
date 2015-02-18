#include "test.h"

/*
 Pan swing should not be overriden by effects such as instrument panning or
 panning envelopes. Previously, pan swing was overriden in OpenMPT if the
 instrument also had a panning envelope. In this file, pan swing should be
 applied to every note.
*/

TEST(test_openmpt_it_randompan)
{
	compare_mixer_data("openmpt/it/RandomPan.it", "openmpt/it/RandomPan.data");
}
END_TEST
