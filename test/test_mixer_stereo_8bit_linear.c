#include "test.h"

TEST(test_mixer_stereo_8bit_linear)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct mixer_data *s;
	struct xmp_module_info info;
	FILE *f;
	int i, j, k, val;

	f = fopen("data/mixer_8bit_linear.data", "r");

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	s = &ctx->s;

	xmp_load_module(opaque, "data/test.xm");

	for (i = 0; i < 5; i++) {
		new_event(ctx, 0, i, 0, 20 + i * 20, 1, 0, 0x0f, 2, 0, 0);
	}

	xmp_player_start(opaque, 8000, 0);
	xmp_mixer_set(opaque, XMP_MIXER_INTERP, XMP_INTERP_LINEAR);

	for (i = 0; i < 10; i++) {
		xmp_player_frame(opaque);
		xmp_player_get_info(opaque, &info);
		for (k = j = 0; j < info.buffer_size / 4; j++) {
			fscanf(f, "%d", &val);
			val >>= 1;
			fail_unless(s->buf32[k++] == val, "mixing error L");
			fail_unless(s->buf32[k++] == val, "mixing error R");
		}
	}

	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
