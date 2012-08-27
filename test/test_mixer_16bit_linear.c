#include "test.h"

TEST(test_mixer_16bit_linear)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct mixer_data *s;
	struct xmp_module_info info;
	FILE *f;
	int i, j, val;

	f = fopen("data/mixer_16bit_linear.data", "r");

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	s = &ctx->s;

	xmp_load_module(opaque, "data/test.xm");

	for (i = 0; i < 5; i++) {
		new_event(ctx, 0, i, 0, 20 + i * 20, 2, 0, 0x0f, 2, 0, 0);
	}

	xmp_player_start(opaque, 8000, XMP_FORMAT_MONO);
	xmp_mixer_set(opaque, XMP_MIXER_INTERP, XMP_INTERP_LINEAR);

	for (i = 0; i < 10; i++) {
		xmp_player_frame(opaque);
		xmp_player_get_info(opaque, &info);
		for (j = 0; j < info.buffer_size / 2; j++) {
			fscanf(f, "%d", &val);
			fail_unless(s->buf32[j] == val, "mixing error");
		}
	}

	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
