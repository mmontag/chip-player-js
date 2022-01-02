#include "test.h"

/*
 * When a note with a new instrument is played and the volume
 * envelope is in fadeout, the volume envelope should reset.
 * If the pan/pitch envelopes DO have carry set, they should
 * continue playing and NOT be reset.
 */

TEST(test_player_it_fade_env_reset_carry)
{
        compare_mixer_data(
                "data/it_fade_env_reset_carry.it",
                "data/it_fade_env_reset_carry.data");
}
END_TEST
