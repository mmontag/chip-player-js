#include "test.h"

/*
 A couple of brief notes about instrument pan swing: All of the values are
 calculated with a range of 0-64. Values out of the 0-64 range are clipped.
 The swing simply defines the amount of variance from the current panning
 value.

 Given all of this, a pan swing value of 16 with a center-panned (32)
 instrument should produce values between 16 and 48; a swing of 32 with full
 right panning (64) will produce values between 0 -- technically -32 -- and 32.

 However, when a set panning effect is used along with a note, it should
 override the pan swing for that note.

 This test should play sets of notes with: Hard left panning Left-biased
 random panning Hard right panning Right-biased random panning Center panning
 with no swing Completely random values
*/

TEST(test_storlek_20_pan_swing_and_set_pan)
{
	compare_mixer_data("data/storlek_20.it", "data/storlek_20.data");
}
END_TEST
