#include "test.h"

TEST(test_api_set_player)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/test.xm");
	xmp_start_player(opaque, 8000, XMP_FORMAT_MONO);

	/* invalid */
	ret = xmp_set_player(opaque, -2, 0);
	fail_unless(ret < 0, "error setting invalid parameter");

	/* mixer */
	ret = xmp_set_player(opaque, XMP_PLAYER_INTERP, XMP_INTERP_NEAREST);
	fail_unless(ret == 0, "can't set XMP_INTERP_NEAREST");
	ret = xmp_get_player(opaque, XMP_PLAYER_INTERP);
	fail_unless(ret == XMP_INTERP_NEAREST, "can't get XMP_INTERP_NEAREST");

	ret = xmp_set_player(opaque, XMP_PLAYER_INTERP, XMP_INTERP_LINEAR);
	fail_unless(ret == 0, "can't set XMP_INTERP_LINEAR");
	ret = xmp_get_player(opaque, XMP_PLAYER_INTERP);
	fail_unless(ret == XMP_INTERP_LINEAR, "can't get XMP_INTERP_LINEAR");

	ret = xmp_set_player(opaque, XMP_PLAYER_INTERP, -2);
	fail_unless(ret < 0, "error setting invalid interpolation");

	ret = xmp_get_player(opaque, XMP_PLAYER_INTERP);
	fail_unless(ret == XMP_INTERP_LINEAR, "invalid interpolation value set");

	/* dsp */
	ret = xmp_set_player(opaque, XMP_PLAYER_DSP, 255);
	fail_unless(ret == 0, "can't set XMP_PLAYER_DSP");
	ret = xmp_get_player(opaque, XMP_PLAYER_DSP);
	fail_unless(ret == 255, "can't get XMP_PLAYER_DSP");

	/* mix */
	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, 0);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_get_player(opaque, XMP_PLAYER_MIX);
	fail_unless(ret == 0, "can't get XMP_PLAYER_MIX");

	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, 100);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_get_player(opaque, XMP_PLAYER_MIX);
	fail_unless(ret == 100, "can't get XMP_PLAYER_MIX");

	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, -100);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_get_player(opaque, XMP_PLAYER_MIX);
	fail_unless(ret == -100, "can't get XMP_PLAYER_MIX");

	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, 50);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_get_player(opaque, XMP_PLAYER_MIX);
	fail_unless(ret == 50, "can't get XMP_PLAYER_MIX");

	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, 101);
	fail_unless(ret < 0, "error setting invalid mix");
	ret = xmp_set_player(opaque, XMP_PLAYER_MIX, -101);
	fail_unless(ret < 0, "error setting invalid mix");

	ret = xmp_get_player(opaque, XMP_PLAYER_MIX);
	fail_unless(ret == 50, "invalid mix values set");

	/* amp */
	ret = xmp_set_player(opaque, XMP_PLAYER_AMP, 0);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_get_player(opaque, XMP_PLAYER_AMP);
	fail_unless(ret == 0, "can't get XMP_PLAYER_AMP");

	ret = xmp_set_player(opaque, XMP_PLAYER_AMP, 3);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_get_player(opaque, XMP_PLAYER_AMP);
	fail_unless(ret == 3, "can't get XMP_PLAYER_AMP");

	ret = xmp_set_player(opaque, XMP_PLAYER_AMP, 2);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_get_player(opaque, XMP_PLAYER_AMP);
	fail_unless(ret == 2, "can't get XMP_PLAYER_AMP");

	ret = xmp_set_player(opaque, XMP_PLAYER_AMP, 101);
	fail_unless(ret < 0, "error setting invalid amp");
	ret = xmp_set_player(opaque, XMP_PLAYER_AMP, -1);
	fail_unless(ret < 0, "error setting invalid amp");

	ret = xmp_get_player(opaque, XMP_PLAYER_AMP);
	fail_unless(ret == 2, "invalid amp values set");

	/* flags */
	ret = xmp_set_player(opaque, XMP_PLAYER_FLAGS, 0);
	fail_unless(ret == 0, "error setting flags");
	ret = xmp_get_player(opaque, XMP_PLAYER_FLAGS);
	fail_unless(ret == 0, "can't get XMP_PLAYER_FLAGS");

	ret = xmp_set_player(opaque, XMP_PLAYER_FLAGS, XMP_FLAGS_VBLANK | XMP_FLAGS_FX9BUG | XMP_FLAGS_FIXLOOP);
	fail_unless(ret == 0, "error setting flags");
	ret = xmp_get_player(opaque, XMP_PLAYER_FLAGS);
	fail_unless(ret == (XMP_FLAGS_VBLANK | XMP_FLAGS_FX9BUG | XMP_FLAGS_FIXLOOP), "can't get XMP_PLAYER_FLAGS");
}
END_TEST
