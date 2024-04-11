#include "test.h"

TEST(test_api_set_position)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct player_data *p;
	int state, ret;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	p = &ctx->p;

	state = xmp_get_player(opaque, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	ret = xmp_set_position(opaque, 0);
	fail_unless(ret == -XMP_ERROR_STATE, "state check error");

 	create_simple_module(ctx, 2, 2);
	libxmp_free_scan(ctx);
	set_order(ctx, 0, 0);
	set_order(ctx, 1, 1);
	set_order(ctx, 2, 0);
	set_order(ctx, 3, 0xff); /* End marker. */
	set_quirk(ctx, QUIRK_MARKER, READ_EVENT_IT);

	libxmp_prepare_scan(ctx);
	libxmp_scan_sequences(ctx);

	xmp_start_player(opaque, 44100, 0);
	fail_unless(p->ord == 0, "didn't start at pattern 0");

	ret = xmp_set_position(opaque, 2);
	fail_unless(ret == 2, "return value error");
	xmp_play_frame(opaque);
	fail_unless(p->ord == 2, "didn't set position 2");

	ret = xmp_set_position(opaque, 3);
	fail_unless(ret == 3, "return value error (marker position)");
	xmp_play_frame(opaque);
	fail_unless(p->ord == 0, "didn't wrap to position 0");

	ret = xmp_set_position(opaque, 4);
	fail_unless(ret == -XMP_ERROR_INVALID, "return value error");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
