#include "test.h"

/*
Periodtable for Tuning 0, Normal
  C-1 to B-1 : 856,808,762,720,678,640,604,570,538,508,480,453
  C-2 to B-2 : 428,404,381,360,339,320,302,285,269,254,240,226
  C-3 to B-3 : 214,202,190,180,170,160,151,143,135,127,120,113

Amiga limits: 907 to 108
*/

TEST(test_effect_2_slide_down)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i, j, k;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	new_event(ctx, 0, 0, 0, 84, 1, 0, 2, 2, 0, 0);
	for (i = 1; i < 60; i++)
		new_event(ctx, 0, i, 0, 0, 0, 0, 2, 0, 0, 0);

	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 60; i++) {
		k = 113 + i * 10;
		for (j = 0; j < 6; j++) {
			xmp_play_frame(opaque);
			xmp_get_frame_info(opaque, &info);
			fail_unless(PERIOD == k + j * 2, "period error");
		}
	}

	/* Fine pitch bend */

	xmp_restart_module(opaque);

	new_event(ctx, 0, 0, 0, 84, 1, 0, 2, 0xf2, 0, 0);
	for (i = 1; i < 60; i++)
		new_event(ctx, 0, i, 0, 0, 0, 0, 2, 0, 0, 0);

	/* check without fine slide quirk */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &info);
	fail_unless(PERIOD == 113, "slide error (no fine slide)");
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &info);
	fail_unless(PERIOD == 355, "slide error (no fine slide)");
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &info);
	fail_unless(PERIOD == 597, "slide error (no fine slide)");

	xmp_restart_module(opaque);

	/* set fine slide quirk */
	set_quirk(ctx, QUIRK_FINEFX, READ_EVENT_MOD);

	for (i = 0; i < 60; i++) {
		k = 113 + i * 2;
		for (j = 0; j < 6; j++) {
			xmp_play_frame(opaque);
			xmp_get_frame_info(opaque, &info);
			fail_unless(PERIOD == k + 2, "fine slide error");
		}
	}

	/* Extra fine pitch bend */

	xmp_restart_module(opaque);

	new_event(ctx, 0, 0, 0, 84, 1, 0, 2, 0xe4, 0, 0);
	for (i = 1; i < 60; i++)
		new_event(ctx, 0, i, 0, 0, 0, 0, 2, 0, 0, 0);

	for (i = 0; i < 60; i++) {
		k = 113 + i * 1;
		for (j = 0; j < 6; j++) {
			xmp_play_frame(opaque);
			xmp_get_frame_info(opaque, &info);
			fail_unless(PERIOD == k + 1, "extra fine slide error");
		}
	}

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
