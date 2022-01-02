#include "test.h"
#include "../src/mixer.h"
#include "../src/virtual.h"

/*
21 - Pitch slide limits

Impulse Tracker always increases (or decreases, of course) the pitch of a
note with a pitch slide, with no limit on either the pitch of the note or
the amount of increment.

An odd side effect of this test is the harmonic strangeness resulting from
playing frequencies well above the Nyquist frequency. Different players
will seem to play the notes at wildly different pitches depending on the
interpolation algorithms and resampling rates used. Even changing the mixer
driver in Impulse Tracker will result in different apparent playback. The
important part of the behavior (and about the only thing that's fully
consistent) is that the frequency is changed at each step.
*/

TEST(test_storlek_21_pitch_slide_limits)
{
	compare_mixer_data(
		"data/storlek_21.it",
		"data/storlek_21.data");
}
END_TEST
