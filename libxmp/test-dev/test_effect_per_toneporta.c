#include "test.h"
#include "effects.h"

/*
Periodtable for Tuning 0, Normal
  C-1 to B-1 : 856,808,762,720,678,640,604,570,538,508,480,453
  C-2 to B-2 : 428,404,381,360,339,320,302,285,269,254,240,226
  C-3 to B-3 : 214,202,190,180,170,160,151,143,135,127,120,113

Amiga limits: 907 to 108
*/

TEST(test_effect_per_toneporta)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	new_event(ctx, 0, 0, 0, 49, 1, 0, 0, 0, 0, 0);
	new_event(ctx, 0, 1, 0, 72, 1, 0, FX_PER_TPORTA, 1, 0, 0);
	new_event(ctx, 0, 55, 0, 0, 0, 0, FX_PER_TPORTA, 0, 0, 0);

	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 60 * 6; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
	}
	fail_unless(PERIOD == 586, "slide error");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
