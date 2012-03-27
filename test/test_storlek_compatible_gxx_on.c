#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
Compatible Gxx on

If this test is played correctly, the first note will slide up and back down
once, and the final series should play four distinct notes. If the first note
slides up again, either (a) the player is testing the flag incorrectly (Gxx
memory is only linked if the flag is not set), or (b) the effect memory values
are not set to zero at start of playback.
*/

TEST(test_storlek_compatible_gxx_on)
{
	xmp_context opaque;
	struct xmp_module_info info;
	struct xmp_channel_info *ci = &info.channel_info[0];
	int time, row, frame, period, volume;
	FILE *f;

	f = fopen("data/storlek_04.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/storlek_04.it");
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

	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
