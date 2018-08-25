#include "test.h"

/*
  ScreamTracker 3 limits the final output period to be at least 64, i.e.
  when playing a note that is too high or when sliding the period lower
  than 64, the output period will simply be clamped to 64. However, when
  reaching a period of 0 through slides, the output on the channel should
  be stopped.
 */

TEST(test_openmpt_s3m_periodlimit)
{
	compare_mixer_data(
		"openmpt/s3m/PeriodLimit.s3m",
		"openmpt/s3m/PeriodLimit.data");
}
END_TEST
