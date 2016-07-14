#include "test.h"

TEST(test_player_med_hold)
{
	compare_mixer_data(
		"data/hold.med",
		"data/med_hold.data");
}
END_TEST
