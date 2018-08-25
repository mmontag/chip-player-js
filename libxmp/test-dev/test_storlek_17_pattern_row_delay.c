#include "test.h"

/*
17 - Pattern row delay

Impulse Tracker has a slightly idiosyncratic way of handling the row delay
and note delay effects, and it's not very clearly explained in ITTECH.TXT.
In fact, row delay is mentioned briefly, albeit in a rather obscure manner,
and it's easy to glance over it. Considering this, it comes as no surprise
that this is one of the most strangely and hackishly implemented behaviors,
so much that pretty much everyone has written it off as an IT replayer bug.

In fact, it's not really a bug, just another artifact of the way Impulse
Tracker works. See, row delay doesn't touch the tick counter at all; it sets
its own counter, which the outer loop uses to repeat the row without
processing notes. The difference seems obscure until you try to replicate
the retriggering behavior of SEx on the same row with SDx. SDx, of course,
operates on all ticks except the first, and SEx repeats a row without
 handling the first tick, so the notes and first-tick effects only happen
once, while delayed notes play over and over as many times as the SEx effect
says.

Sound weird? Not really. As I said, it's even mentioned in ITTECH.TXT. Check
the flowchart under "Effect Info", and look at the words "Row counter". This
particular variable just says how many times the row plays (without replaying
notes) before moving to the next row. That's all. You just play the row more
than once.

This test plays sets of alternating bassdrum and snare, followed by a short
drum loop. In the first part, the bassdrum should play 2, 1, 7, 5, 1, and 5
times before each respective snare drum.
*/

TEST(test_storlek_17_pattern_row_delay)
{
	compare_mixer_data(
		"data/storlek_17.it",
		"data/storlek_17.data");
}
END_TEST
