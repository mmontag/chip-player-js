#include "test.h"

TEST(test_player_it_noteoff_nosuck)
{
	compare_mixer_data(
		"data/NoteOff_nosuck.it",
		"data/NoteOff_nosuck.data");
}
END_TEST
