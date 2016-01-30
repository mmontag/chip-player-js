#include "test.h"

/*
 Like in some other trackers (e.g. Impulse Tracker), arpeggio notes are
 supposed to be relative to the current note frequency, i.e. the arpeggio
 should still sound as intended after executing a portamento. However, this
 is not quite as simple as in Impulse Tracker, since the base note is first
 rounded (up or down) to the nearest semitone and then the arpeggiated note
 is added.
*/

TEST(test_openmpt_xm_arpslide)
{
	compare_mixer_data(
		"openmpt/xm/ArpSlide.xm",
		"openmpt/xm/ArpSlide.data");
}
END_TEST
