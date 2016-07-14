#include "test.h"

/*
02 - Arpeggio with no value

If this test plays correctly, both notes will sound the same, bending downward
smoothly. Incorrect (but perhaps acceptable, considering the unlikelihood of
this combination of pitch bend and a meaningless arpeggio) handling of the
arpeggio effect will result in a "stutter" on the second note, but the final
pitch should be the same for both notes. Really broken players will mangle the
pitch slide completely due to the arpeggio resetting the pitch on every third
tick.
*/

TEST(test_storlek_02_arpeggio_no_value)
{
	compare_mixer_data(
		"data/storlek_02.it",
		"data/storlek_02.data");
}
END_TEST
