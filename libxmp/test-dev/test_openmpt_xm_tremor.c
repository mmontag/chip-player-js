#include "test.h"

/*
 The tremor counter is not updated on the first tick, and the counter is only
 ever reset after a phase switch (from on to off or vice versa), so the best
 technique to implement tremor is to count down to zero and memorize the
 current phase (on / off), and when you reach zero, switch to the other phase
 by reading the current tremor parameter. Keep in mind that T00 recalls the
 tremor effect memory, but the phase length is always incremented by one,
 i.e. T12 means “on for two ticks, off for three”.
*/

TEST(test_openmpt_xm_tremor)
{
	compare_mixer_data(
		"openmpt/xm/Tremor.xm",
		"openmpt/xm/Tremor.data");
}
END_TEST
