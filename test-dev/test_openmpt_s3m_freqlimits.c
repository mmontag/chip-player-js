#include "test.h"

/*
  Scream Tracker 3 stops playback if the period is too low (the frequency is
  too high), but there is no maximum threshold above which playback is cut.
*/

TEST(test_openmpt_s3m_freqlimits)
{
	compare_mixer_data(
		"openmpt/s3m/FreqLimits.s3m",
		"openmpt/s3m/FreqLimits.data");
}
END_TEST
