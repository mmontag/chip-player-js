#include "test.h"

/*
03 - Compatible Gxx off

Impulse Tracker links the effect memories for Exx, Fxx, and Gxx together if
"Compatible Gxx" in NOT enabled in the file header. In other formats,
portamento to note is entirely separate from pitch slide up/down. Several
players that claim to be IT-compatible do not check this flag, and always store
the last Gxx value separately.

When this test is played correctly, the first note will bend up, down, and back
up again, and the final set of notes should only slide partway down. Players
which do not correctly handle the Compatible Gxx flag will not perform the
final pitch slide in the first part, or will "snap" the final notes.
*/

TEST(test_storlek_03_compatible_gxx_off)
{
	compare_mixer_data(
		"data/storlek_03.it",
		"data/storlek_03.data");
}
END_TEST
