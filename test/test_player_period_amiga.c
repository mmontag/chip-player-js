#include "test.h"

TEST(test_player_period_amiga)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_module_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	xmp_player_start(opaque, 44100, 0);

	/* test note periods */
	new_event(ctx, 0, 0, 0, 20, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 1, 0, 30, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 2, 0, 50, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 3, 0, 60, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 4, 0, 70, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 5, 0, 80, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 6, 0, 90, 1, 0, 0x0f, 1, 0, 0);
	new_event(ctx, 0, 7, 0, 100, 1, 0, 0x0f, 1, 0, 0);

	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 18718000, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 10505122, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 3308906, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 1857060, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 1042240, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 584937, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 328285, "Bad period");
	
	xmp_player_frame(opaque);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.channel_info[0].period == 184243, "Bad period");
}
END_TEST
