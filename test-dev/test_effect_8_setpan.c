#include "test.h"
#include "../src/effects.h"


static int vals[] = {
	0, 0, 0,		/* C-5 1 F03   play instrument w/ pan left */
	255, 255, 255,		/* --- - 8FF   set pan to right */
	255, 255, 255,		/* F-5 - ---   new note keeps previous pan */
	0, 0, 0			/* --- 1 ---   new inst resets pan to left*/
};

TEST(test_effect_8_setpan)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	/* set instrument 1 pan to left */
	ctx->m.mod.xxi[0].sub[0].pan = 0;

	/* slide down & up with memory */
	new_event(ctx, 0, 0, 0, 60, 1, 0, FX_SPEED,  0x03, 0, 0);
	new_event(ctx, 0, 1, 0,  0, 0, 0, FX_SETPAN, 0xff, 0, 0);
	new_event(ctx, 0, 2, 0, 65, 0, 0, 0x00,      0x00, 0, 0);
	new_event(ctx, 0, 3, 0,  0, 1, 0, 0x00,      0x00, 0, 0);

	/* check FT2 event reader */
	set_quirk(ctx, QUIRKS_FT2, READ_EVENT_FT2);

	/* play it */
	xmp_start_player(opaque, 44100, 0);

	/* set mix to 100% pan separation */
	xmp_set_player(opaque, XMP_PLAYER_MIX, 100);

	for (i = 0; i < 4 * 3; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(info.channel_info[0].pan == vals[i], "pan error");

	}
}
END_TEST
