#include "test.h"

TEST(test_player_it_keyoff)
{
	compare_mixer_data(
		"data/test_keyoff.it",
		"data/test_keyoff.data");
}
END_TEST
