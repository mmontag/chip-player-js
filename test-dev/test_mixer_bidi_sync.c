#include "test.h"

/* libxmp should tries to render bidirectional samples as closely
 * to forward samples as possible. The two input modules for this
 * test have been crafted to be almost completely silent when the
 * two samples are synchronized, and libxmp should be able to keep
 * them synchronized at different rates and interpolation settings.
 */

/* Due to various former quirks of the renderer, an extended amount
 * of the input modules needs to be rendered to make sure it doesn't
 * slowly diverge over time. The inputs are 6 * 256 frames long. */
#define RENDER_FRAMES (6 * 256)

/* Checking the pre-downmix buffer, so allow at least +-(2^12) in error.
 * The lower this value can be, the better. */
#define RENDER_ERROR  (1 << 17)

static void bidi_sync_test_mode(xmp_context opaque, int rate, int interp, const char *str)
{
	struct context_data *ctx = (struct context_data *)opaque;
	struct mixer_data *s = &ctx->s;
	struct xmp_frame_info info;
	int i, j;

	xmp_start_player(opaque, 8000, XMP_FORMAT_MONO);
	xmp_set_player(opaque, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);

	for (i = 0; i < RENDER_FRAMES; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		for (j = 0; j < info.buffer_size / 2; j++) {
			fail_unless(s->buf32[j] >= -RENDER_ERROR &&
				    s->buf32[j] <= RENDER_ERROR, str);
		}
	}
}

#define bidi_sync_helper(opaque, str) do { \
	bidi_sync_test_mode(opaque, 8000,  XMP_INTERP_NEAREST, str ":nearest:8k"); \
	bidi_sync_test_mode(opaque, 8000,  XMP_INTERP_LINEAR,  str ":linear:8k"); \
	bidi_sync_test_mode(opaque, 8000,  XMP_INTERP_SPLINE,  str ":spline:8k"); \
	bidi_sync_test_mode(opaque, 11025, XMP_INTERP_NEAREST, str ":nearest:11k"); \
	bidi_sync_test_mode(opaque, 11025, XMP_INTERP_LINEAR,  str ":linear:11k"); \
} while (0)

TEST(test_mixer_bidi_sync)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	ret = xmp_load_module(opaque, "data/bidi_sync.it");
	fail_unless(ret == 0, "load error");

	bidi_sync_helper(opaque, "it");
	xmp_release_module(opaque);

	ret = xmp_load_module(opaque, "data/bidi_sync.xm");
	fail_unless(ret == 0, "load_error");

	bidi_sync_helper(opaque, "xm");
	xmp_release_module(opaque);

	xmp_free_context(opaque);
}
END_TEST
