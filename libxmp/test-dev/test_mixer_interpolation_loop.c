#include "test.h"

/* Data immediately prior to and after the loop should not
 * affect the playback of the loop when using interpolation.
 * The loop of the sample in this module should be completely
 * silent.
 *
 * Previously, libxmp played a buzz instead due to the sample
 * prior to the loop not being fixed with spline interpolation.
 * Similar behavior can be found in Modplug Tracker 1.16.
 */

TEST(test_mixer_interpolation_loop)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct mixer_data *s;
	struct xmp_frame_info info;
	FILE *f;
	int i, j, val, ret;

#ifndef MIXER_GENERATE
	f = fopen("data/interpolation_loop.data", "r");
#else
	f = fopen("interpolation_loop.data", "w");
#endif

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	s = &ctx->s;

	ret = xmp_load_module(opaque, "data/interpolation_loop.it");
	fail_unless(ret == 0, "load error");

	xmp_start_player(opaque, 8000, XMP_FORMAT_MONO);
	xmp_set_player(opaque, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);

	/* First frame is the only one that should contain data. */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &info);
	for (j = 0; j < info.buffer_size / 2; j++) {
#ifndef MIXER_GENERATE
		ret = fscanf(f, "%d", &val);
		fail_unless(ret == 1, "read_error");
		fail_unless(s->buf32[j] == val, "mixing error");
#else
		fprintf(f, "%d\n", s->buf32[j]);
#endif
	}

	/* Further frames should be silent. */
	for (i = 0; i < 10; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		for (j = 0; j < info.buffer_size / 2; j++)
			fail_unless(s->buf32[j] == 0, "mixing error");
	}

#ifdef MIXER_GENERATE
	fail_unless(0, "MIXER_GENERATE is enabled");
#endif
	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
