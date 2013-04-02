#include "test.h"
#include "../src/effects.h"

static int vals[] = {
	240, 240, 240, 240, 240, 240, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16,
	238, 239, 240, 240, 240, 240, 238, 239,
	241, 242, 241, 240, 238, 239, 241, 242,

	16, 16, 16, 16, 14, 15, 17, 18,
	17, 16, 14, 15, 16, 16, 16, 16,
	14, 15, 17, 18, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16
};


TEST(test_effect_masterpan)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i, ret;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

	ret = xmp_load_module(opaque, "data/vcol_g.it");
	fail_unless(ret == 0, "can't load module");
	
	xmp_start_player(opaque, 44100, 0);

	/* set mix to 100% pan separation */
	xmp_set_player(opaque, XMP_PLAYER_MIX, 100);

	for (i = 0; i < 64; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);

		fail_unless(info.channel_info[0].pan == vals[i], "pan error");

		xmp_play_frame(opaque);
		xmp_play_frame(opaque);
		xmp_play_frame(opaque);
	}
}
END_TEST
