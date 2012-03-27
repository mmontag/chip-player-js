#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
Arpeggio with no value

If this test plays correctly, both notes will sound the same, bending downward
smoothly. Incorrect (but perhaps acceptable, considering the unlikelihood of
this combination of pitch bend and a meaningless arpeggio) handling of the
arpeggio effect will result in a "stutter" on the second note, but the final
pitch should be the same for both notes. Really broken players will mangle the
pitch slide completely due to the arpeggio resetting the pitch on every third
tick.
*/

TEST(test_storlek_arpeggio_no_value)
{
	xmp_context opaque;
	struct xmp_module_info info;
	struct xmp_channel_info *ci = &info.channel_info[0];
	int time, row, frame, period, volume;
	FILE *f;

	f = fopen("data/storlek_02.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/storlek_02.it");
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
