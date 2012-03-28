#include "test.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
Sample change with no note

If a sample number is given without a note, Impulse Tracker will play the old
note with the new sample. This test should play the same beat twice, exactly
the same way both times. Players which do not handle sample changes correctly
will produce various interesting (but nonetheless incorrect!) results for the
second measure.
*/

TEST(test_storlek_sample_change_no_note)
{
	xmp_context opaque;
	struct xmp_module_info info;
	struct xmp_channel_info *ci = &info.channel_info[0];
	int time, row, frame, chan, period, volume, ins;
	char line[200];
	FILE *f;

	f = fopen("data/storlek_09.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/storlek_09.it");
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
