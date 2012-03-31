#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
11 - Infinite loop exploit

(Note: on this test, "fail" status is given for players which deadlock while
loading or calculating the duration, and is not based on actual playback
behavior. Incidentally, this will cause Impulse Tracker to freeze.)

This is a particularly evil pattern loop setup that exploits two possible
problems at the same time, and it will very likely cause any player to get
"stuck".

The first problem here is the duplicated loopback effect on the first channel;
the correct way to handle this is discussed in the previous test. The second
problem, and quite a bit more difficult to handle, is the seemingly strange
behavior after the third channel's loop plays once. What happens is the second
SB1 in the first channel "empties" its loopback counter, and when it reaches
the first SB1 again, the value is reset to 1. However, the second channel
hasn't looped yet, so playback returns to the first row. The next time around,
the second channel is done, but the first one needs to loop again — creating an
infinite loop situation. Even Impulse Tracker gets snagged by this.
*/

TEST(test_storlek_11_infinite_pattern_loop)
{
	xmp_context opaque;
	struct xmp_module_info info;
	struct xmp_channel_info *ci = &info.channel_info[0];
	int time, row, frame, chan, period, volume, ins;
	char line[200];
	FILE *f;

	f = fopen("data/storlek_11.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/storlek_11.it");
	xmp_player_start(opaque, 44100, 0);

	while (1) {
		xmp_player_frame(opaque);
		xmp_player_get_info(opaque, &info);
		if (info.loop_count > 0)
			break;

		fgets(line, 200, f);
		sscanf(line, "%d %d %d %d %d %d %d",
			&time, &row, &frame, &chan, &period, &volume, &ins);

		fail_unless(info.time  == time,   "time mismatch");
		fail_unless(info.row   == row,    "row mismatch");
		fail_unless(info.frame == frame,  "frame mismatch");
		fail_unless(ci->period == period, "period mismatch");
		fail_unless(ci->volume == volume, "volume mismatch");
		fail_unless(ci->instrument == ins, "instrument mismatch");
	}

	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
