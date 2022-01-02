#include "test.h"

TEST(test_mixer_mono_8bit_nearest)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct mixer_data *s;
	struct xmp_frame_info info;
	FILE *f;
	int i, j, val;

#ifndef MIXER_GENERATE
	f = fopen("data/mixer_8bit_nearest.data", "r");
#else
	f = fopen("mixer_8bit_nearest.data", "w");
#endif

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	s = &ctx->s;

	xmp_load_module(opaque, "data/test.xm");

	for (i = 0; i < 5; i++) {
		new_event(ctx, 0, i, 0, 20 + i * 20, 1, 0, 0x0f, 2, 0, 0);
	}

	xmp_start_player(opaque, 8000, XMP_FORMAT_MONO);
	xmp_set_player(opaque, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST);

	for (i = 0; i < 10; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		for (j = 0; j < info.buffer_size / 2; j++) {
#ifndef MIXER_GENERATE
			int ret = fscanf(f, "%d", &val);
			fail_unless(ret == 1, "read error");
			fail_unless(s->buf32[j] == val, "mixing error");
#else
			fprintf(f, "%d\n", s->buf32[j]);
#endif
		}
	}

#ifdef MIXER_GENERATE
	fail_unless(0, "MIXER_GENERATE is enabled");
#endif
	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
