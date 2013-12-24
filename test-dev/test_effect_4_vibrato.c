#include "test.h"
#include "../src/effects.h"
#include <math.h>

/*
Periodtable for Tuning 0, Normal
  C-1 to B-1 : 856,808,762,720,678,640,604,570,538,508,480,453
  C-2 to B-2 : 428,404,381,360,339,320,302,285,269,254,240,226
  C-3 to B-3 : 214,202,190,180,170,160,151,143,135,127,120,113

Amiga limits: 907 to 108
*/

#define PERIOD ((int)round(1.0 * info.channel_info[0].period / 4096))

static int vals[] = {
	143, 143, 144, 146, 147, 148,
	149, 149, 150, 150, 150, 149,
	254, 254, 257, 259, 261, 261,
	265, 265, 262, 258, 254, 249,
	453, 453, 459, 464, 467, 468,
	467, 467, 464, 459, 453, 446,
	808, 808, 819, 823, 819, 808,
	791, 791, 784, 791, 808, 824,
	1439, 1439, 1461, 1455, 1429, 1415,
	1427, 1427, 1465, 1455, 1415, 1417
};

static int vals2[] = {
	143, 144, 146, 147, 148, 149,
	150, 150, 150, 150, 148, 146,
	254, 257, 259, 261, 261, 261,
	262, 258, 254, 249, 245, 242,
	453, 459, 464, 467, 468, 467,
	464, 459, 453, 446, 441, 438,
	808, 819, 823, 819, 808, 796,
	784, 791, 808, 824, 831, 824,
	1439, 1461, 1455, 1429, 1415, 1429,
	1460, 1462, 1422, 1412, 1450, 1467
};

static int vals3[] = {
	143, 146, 149, 151, 154, 156,
	157, 158, 158, 157, 154, 150,
	254, 260, 265, 268, 269, 268,
	270, 263, 254, 244, 237, 231,
	453, 465, 475, 482, 484, 482,
	475, 465, 453, 440, 430, 423,
	808, 830, 839, 830, 808, 785,
	760, 774, 808, 841, 855, 841,
	1439, 1483, 1472, 1420, 1391, 1420,
	1481, 1485, 1405, 1386, 1461, 1496
};


TEST(test_effect_4_vibrato)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	new_event(ctx, 0, 0, 0, 80, 1, 0, FX_VIBRATO, 0x24, 0, 0);
	new_event(ctx, 0, 1, 0, 0, 0, 0, FX_VIBRATO, 0x34, 0, 0);
	new_event(ctx, 0, 2, 0, 70, 0, 0, FX_VIBRATO, 0x44, 0, 0);
	new_event(ctx, 0, 3, 0, 0, 0, 0, FX_VIBRATO, 0x46, 0, 0);
	new_event(ctx, 0, 4, 0, 60, 0, 0, FX_VIBRATO, 0x48, 0, 0);
	new_event(ctx, 0, 5, 0, 0, 0, 0, FX_VIBRATO, 0x00, 0, 0);
	new_event(ctx, 0, 6, 0, 50, 0, 0, FX_VIBRATO, 0x88, 0, 0);
	new_event(ctx, 0, 7, 0, 0, 0, 0, FX_VIBRATO, 0x8c, 0, 0);
	new_event(ctx, 0, 8, 0, 40, 0, 0, FX_VIBRATO, 0xcc, 0, 0);
	new_event(ctx, 0, 9, 0, 0, 0, 0, FX_VIBRATO, 0xff, 0, 0);

	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 10 * 6; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(PERIOD == vals[i], "vibrato error");
	}

	/* check vibrato in all frames flag */

	xmp_restart_module(opaque);
	set_quirk(ctx, QUIRK_VIBALL, READ_EVENT_MOD);

	for (i = 0; i < 10 * 6; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(PERIOD == vals2[i], "vibrato error");
	}

	/* check deep vibrato flag */

	xmp_restart_module(opaque);
	set_quirk(ctx, QUIRK_DEEPVIB, READ_EVENT_MOD);

	for (i = 0; i < 10 * 6; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(PERIOD == vals3[i], "deep vibrato error");
	}
}
END_TEST
