#include "test.h"

/*
01 - Arpeggio and pitch slide

Some players handle arpeggio incorrectly, storing and manipulating the original
pitch of the note instead of modifying the current pitch. While this has no
effect with "normal" uses of arpeggio, it causes strange problems when
combining arpeggio and a pitch bend.

When this test is played correctly, the first note will portamento upward,
arpeggiate for a few rows, and stay at the higher pitch. If this is played
incorrectly, it is most likely because the arpeggio effect is not checking the
current pitch of the note.

The second note should have a "stepping" effect. Make sure all three notes of
the arpeggio are being altered correctly by the volume-column pitch slide, not
just the base note. Also, the pitch after all effects should be approximately
the same as the third note.

Lastly, note that the arpeggio is set on the first row, prior to the note.
Certain players (notably, Modplug) erroneously ignore arpeggio values if no
note is playing.
*/

TEST(test_storlek_01_arpeggio_pitch_slide)
{
	compare_mixer_data(
		"data/storlek_01.it",
		"data/storlek_01.data");
}
END_TEST
