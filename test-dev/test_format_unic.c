#include "test.h"

TEST(test_format_unic)
{
	xmp_context opaque;
	struct xmp_frame_info info;
	struct xmp_channel_info *ci;
	int time, row, frame, chan, period, volume, ins, pan, smp;
	char line[200];
	FILE *f;
	int ret, i, j;

	f = fopen("data/format_unic.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/EaglePlayerIntro.mod");
	printf("%d\n", ret);
	fail_unless(ret == 0, "load module");
	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 500; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);

		for (j = 0; j < 4; j++) {
			ci = &info.channel_info[j];
			fgets(line, 200, f);
			sscanf(line, "%d %d %d %d %d %d %d %d %d",
					&time, &row, &frame, &chan, &period,
					&volume, &ins, &pan, &smp);

printf("info.time=%d time=%d\n", info.time, time);
			fail_unless(info.time  == time,  "time mismatch");
			fail_unless(info.row   == row,   "row mismatch");
			fail_unless(info.frame == frame, "frame mismatch");
			fail_unless(ci->sample == smp,   "sample mismatch");
		}
	}

	fgets(line, 200, f);
	fail_unless(feof(f), "not end of data file");

	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
