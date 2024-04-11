#include "test.h"
#include "../src/effects.h"

/*
Periodtable for Tuning 0, Normal
  C-1 to B-1 : 856,808,762,720,678,640,604,570,538,508,480,453
  C-2 to B-2 : 428,404,381,360,339,320,302,285,269,254,240,226
  C-3 to B-3 : 214,202,190,180,170,160,151,143,135,127,120,113

Amiga limits: 907 to 108
*/

static int vals[] = {
	143, 143, 143, 143, 144, 144,
	144, 144, 144, 144, 144, 144,
	254, 254, 254, 255, 255, 255,
	256, 256, 256, 255, 254, 253,
	453, 453, 454, 455, 456, 456,
	456, 456, 455, 454, 453, 452,
	808, 808, 810, 811, 810, 808,
	804, 804, 803, 804, 808, 812,
	1440, 1440, 1445, 1444, 1438, 1435,
	1438, 1438, 1446, 1444, 1435, 1435
};


TEST(test_effect_fine_vibrato)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	new_event(ctx, 0, 0, 0, 80, 1, 0, FX_FINE_VIBRATO, 0x24, 0, 0);
	new_event(ctx, 0, 1, 0, 0, 0, 0, FX_FINE_VIBRATO, 0x34, 0, 0);
	new_event(ctx, 0, 2, 0, 70, 0, 0, FX_FINE_VIBRATO, 0x44, 0, 0);
	new_event(ctx, 0, 3, 0, 0, 0, 0, FX_FINE_VIBRATO, 0x46, 0, 0);
	new_event(ctx, 0, 4, 0, 60, 0, 0, FX_FINE_VIBRATO, 0x48, 0, 0);
	new_event(ctx, 0, 5, 0, 0, 0, 0, FX_FINE_VIBRATO, 0x00, 0, 0);
	new_event(ctx, 0, 6, 0, 50, 0, 0, FX_FINE_VIBRATO, 0x88, 0, 0);
	new_event(ctx, 0, 7, 0, 0, 0, 0, FX_FINE_VIBRATO, 0x8c, 0, 0);
	new_event(ctx, 0, 8, 0, 40, 0, 0, FX_FINE_VIBRATO, 0xcc, 0, 0);
	new_event(ctx, 0, 9, 0, 0, 0, 0, FX_FINE_VIBRATO, 0xff, 0, 0);

	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 10 * 6; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(PERIOD == vals[i], "fine vibrato error");
	}

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
