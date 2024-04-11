#include "test.h"

/*
 * When a note with a new instrument is played and the volume
 * envelope is in fadeout, the volume envelope should reset.
 * If the pan/pitch envelopes do NOT have carry set, they should
 * be reset independently of this check.
 */

TEST(test_player_it_fade_env_reset)
{
	compare_mixer_data(
		"data/it_fade_env_reset.it",
		"data/it_fade_env_reset.data");
}
END_TEST
