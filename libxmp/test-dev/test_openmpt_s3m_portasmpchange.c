#include "test.h"

/*
 If the sample number next to a portamento effect differs from the previous
 number, the old sample should be kept, but the new sample's default volume
 should still be applied.
*/

TEST(test_openmpt_s3m_portasmpchange)
{
	compare_mixer_data(
		"openmpt/s3m/PortaSmpChange.s3m",
		"openmpt/s3m/PortaSmpChange.data");
}
END_TEST
