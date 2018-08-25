#include "test.h"

/*
10 - Pattern loop

The pattern loop effect is quite complicated to handle, especially when dealing
with multiple channels. Possibly the most important fact to realize is that no
channel's pattern loop affects any other channel — each loop's processing
should be entirely contained to the channel the effect is in.

Another trouble spot that some players have is dealing correctly with strange
situations such as two consecutive loopback effects, e.g.:

000 | ... .. .. SB0
001 | ... .. .. SB1
002 | ... .. .. SB1
003 | ... .. .. .00

To prevent this from triggering an infinite loop, Impulse Tracker sets the
loopback point to the next row after the last SBx effect. This, the player flow
for the above fragment should be 0, 1, 0, 1, 2, 2, 3.

Another point to notice is when a loop should finish if two channels have SBx
effects on the same row:

000 | ... .. .. SB0 | ... .. .. SB0
001 | ... .. .. SB1 | ... .. .. SB2

What should happen here is the rows continue to loop as long as ANY of the
loopback counters are nonzero, or in other words, the least common multiple of
the total loop counts for each channel. In this case, the rows will play six
times — two loops for the first channel, and three for the second.

When correctly played, this test should produce a drum beat, slightly
syncopated. The entire riff repeats four times, and should sound the same all
four times.
*/

TEST(test_storlek_10_pattern_loop)
{
	compare_mixer_data(
		"data/storlek_10.it",
		"data/storlek_10.data");
}
END_TEST
