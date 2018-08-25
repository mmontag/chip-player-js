#include "test.h"

/*
 Scream Tracker 3 uses the last non-zero effect parameter as a memory for
 most effects: Dxy, Kxy, Lxy, Exx, Fxx, Ixy, Jxy, Qxy, Rxy, Sxy. Other
 effects may have their own memory or share it with another command (such
 as Hxy / Uxy).
*/

TEST(test_openmpt_s3m_parammemory)
{
	compare_mixer_data(
		"openmpt/s3m/ParamMemory.s3m",
		"openmpt/s3m/ParamMemory.data");
}
END_TEST
