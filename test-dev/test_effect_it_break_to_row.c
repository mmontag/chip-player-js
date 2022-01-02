#include "test.h"

/*
 IT effect Cxy has an hexadecimal parameter.
*/

TEST(test_effect_it_break_to_row)
{
	xmp_context opaque;
	struct xmp_frame_info info;
	struct xmp_module_info minfo;
	struct xmp_channel_info *ci;
	int time, row, frame, chan, period, volume, ins, pan;
	char line[200];
	char *ret;
	FILE *f;
	int i;

	f = fopen("data/break_to_row.data", "r");

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/break_to_row.it");
	xmp_get_module_info(opaque, &minfo);
	xmp_start_player(opaque, 44100, 0);

	while (1) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		if (info.loop_count > 0)
			break;

		for (i = 0; i < minfo.mod->chn; i++) {
			ret = fgets(line, 200, f);
			fail_unless(ret == line, "read error");
			sscanf(line, "%d %d %d %d %d %d %d %d", &time, &row,
				&frame, &chan, &period, &volume, &ins, &pan);

			ci = &info.channel_info[chan];

			fail_unless(info.time  == time,   "time mismatch");
			fail_unless(info.row   == row,    "row mismatch");
			fail_unless(info.frame == frame,  "frame mismatch");
			fail_unless(ci->period == period, "period mismatch");
			fail_unless(ci->volume == volume, "volume mismatch");
			fail_unless(ci->pan    == pan,    "Pan mismatch");
		}
	}

	ret = fgets(line, 200, f);
	fail_unless(ret == NULL && feof(f), "not end of data file");

	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
