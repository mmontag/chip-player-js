#include "test.h"

/*
06 - Volume column and fine slides

Impulse Tracker's handling of volume column pitch slides along with its normal
effect memory is rather odd. While the two do share their effect memory, fine
slides are not handled in the volume column.

When this test is played 100% correctly, the note will slide very slightly
downward, way up, and then slightly back down.
*/

TEST(test_storlek_06_volume_column_and_fine_slides)
{
	compare_mixer_data(
		"data/storlek_06.it",
		"data/storlek_06.data");
}
END_TEST
