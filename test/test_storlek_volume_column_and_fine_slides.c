#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
Volume column and fine slides

Impulse Tracker's handling of volume column pitch slides along with its normal
effect memory is rather odd. While the two do share their effect memory, fine
slides are not handled in the volume column.

When this test is played 100% correctly, the note will slide very slightly
downward, way up, and then slightly back down.
*/

TEST(test_storlek_volume_column_and_fine_slides)
{
	xmp_context opaque;
	struct xmp_module_info info;
	struct xmp_channel_info *ci = &info.channel_info[0];
	int time, row, frame, period, volume;
	FILE *f;

	f = fopen("data/storlek_06.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/storlek_06.it");
	xmp_player_start(opaque, 44100, 0);

	while (1) {

		xmp_player_frame(opaque);
		xmp_player_get_info(opaque, &info);
		if (info.loop_count > 0)
			break;

		fscanf(f, "%d %d %d %d %d", &time, &row, &frame, &period, &volume);
		fail_unless(info.time  == time,   "time mismatch");
		fail_unless(info.row   == row,    "row mismatch");
		fail_unless(info.frame == frame,  "frame mismatch");
		fail_unless(ci->period == period, "period mismatch");
		fail_unless(ci->volume == volume, "volume mismatch");
	}
}
END_TEST
