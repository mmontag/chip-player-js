#include "test.h"

TEST(test_next_order_skip)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct player_data *p;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	p = &ctx->p;

 	create_simple_module(ctx, 2, 2);
	set_quirk(ctx, QUIRK_MARKER, READ_EVENT_MOD);

	libxmp_free_scan(ctx);
	set_order(ctx, 0, 0);
	set_order(ctx, 1, 0xfe);
	set_order(ctx, 2, 0);
	set_order(ctx, 3, 0xff);
	set_order(ctx, 4, 1);

	libxmp_prepare_scan(ctx);
	libxmp_scan_sequences(ctx);

	xmp_start_player(opaque, 44100, 0);
	xmp_play_frame(opaque);
	fail_unless(p->ord == 0, "didn't start at pattern 2");

	for (i = 0; i < 30; i++) {
		xmp_play_frame(opaque);
	}

	xmp_next_position(opaque);
	xmp_play_frame(opaque);
	fail_unless(p->ord == 2, "incorrect pattern");
	fail_unless(p->row == 0, "incorrect row");
	fail_unless(p->frame == 0, "incorrect frame");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
