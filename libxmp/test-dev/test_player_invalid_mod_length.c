#include "test.h"

TEST(test_player_invalid_mod_length)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct module_data *m;
	struct xmp_frame_info info;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	m = &ctx->m;

 	create_simple_module(ctx, 2, 2);
	new_event(ctx, 0, 0, 0, 49, 1, 0, 0, 0, 0, 0);
	m->mod.len = 0;

	xmp_start_player(opaque, 44100, 0);

	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &info);

	/* Fix length so the mod is freed correctly. */
	m->mod.len = 2;
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
