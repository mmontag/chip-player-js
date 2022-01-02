#include "test.h"
#include "../src/effects.h"

TEST(test_mixer_mono_16bit_linear_filter)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct mixer_data *s;
	struct xmp_frame_info info;
	FILE *f;
	int i, j, val;

#ifndef MIXER_GENERATE
	f = fopen("data/mixer_16bit_linear_filter.data", "r");
#else
	f = fopen("mixer_16bit_linear_filter.data", "w");
#endif

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	s = &ctx->s;

	xmp_load_module(opaque, "data/test.it");

	new_event(ctx, 0, 0, 0, 30, 2, 0, 0x0f, 2, FX_FLT_CUTOFF, 50);
	new_event(ctx, 0, 1, 0, 30, 2, 0, 0x0f, 2, FX_FLT_CUTOFF, 120);

	xmp_start_player(opaque, 22050, XMP_FORMAT_MONO);

	for (i = 0; i < 4; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		for (j = 0; j < info.buffer_size / 2; j++) {
#ifndef MIXER_GENERATE
			int ret = fscanf(f, "%d", &val);
			fail_unless(ret == 1, "read error");
			fail_unless(abs(s->buf32[j] - val) <= 1, "mixing error");
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
