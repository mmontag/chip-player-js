#include "test.h"

/*
 Rows on which a row delay (SEx) effect is placed have multiple “first ticks”,
 i.e. you should set your “first tick flag” on every tick that is a multiple
 of the song speed (or speed + tick delay if you support tick delays in your
 S3M player). In this test module, the note pitch is changed multiple times
 per row, depending on the row delay values. 
*/

TEST(test_openmpt_s3m_patterndelaysretrig)
{
	compare_mixer_data(
		"openmpt/s3m/PatternDelaysRetrig.s3m",
		"openmpt/s3m/PatternDelaysRetrig.data");
}
END_TEST
