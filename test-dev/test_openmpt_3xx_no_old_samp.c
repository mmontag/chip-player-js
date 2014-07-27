#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 Two tests in one: An offset effect that points beyond the sample end should
 stop playback on this channel. The note must not be picked up by further
 portamento effects.
*/

TEST(test_openmpt_3xx_no_old_samp)
{
	xmp_context opaque;
	struct xmp_frame_info info;
	int time, row, frame, chan, period, volume;
	char line[200];
	FILE *f;
	int i;

	f = fopen("data/openmpt/3xx-no-old-samp.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/openmpt/3xx-no-old-samp.xm");
	xmp_start_player(opaque, 44100, 0);

	while (1) {
		struct xmp_channel_info *ci = &info.channel_info[i];
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		if (info.loop_count > 0)
			break;

		for (i = 0; i < 2; i++) {
			struct xmp_channel_info *ci = &info.channel_info[i];

			fgets(line, 200, f);
			sscanf(line, "%d %d %d %d %d %d",
				&time, &row, &frame, &chan, &period, &volume);

			fail_unless(info.time  == time,   "time mismatch");
			fail_unless(info.row   == row,    "row mismatch");
			fail_unless(info.frame == frame,  "frame mismatch");
			fail_unless(ci->period == period, "period mismatch");
			fail_unless(ci->volume == volume, "volume mismatch");
		}
	}

	fgets(line, 200, f);
	fail_unless(feof(f), "not end of data file");

	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
