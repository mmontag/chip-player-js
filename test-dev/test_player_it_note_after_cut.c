#include "test.h"

/*
 * Note data can be recovered after a cut, such as in ub-name.it
 * after the cut (e.g. with pitch slide effect)
 *
 *     F#7 07 .. GF1
 *     ^^^ .. .. .00
 *     F-7 07 .. GF1
 */

TEST(test_player_it_note_after_cut)
{
	compare_mixer_data(
		"data/note_after_cut.it",
		"data/note_after_cut.data");
}
END_TEST
