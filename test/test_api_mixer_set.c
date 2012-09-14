#include "test.h"

TEST(test_api_mixer_set)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/test.xm");
	xmp_player_start(opaque, 8000, XMP_FORMAT_MONO);

	/* invalid */
	ret = xmp_mixer_set(opaque, -2, 0);
	fail_unless(ret < 0, "error setting invalid parameter");

	/* mixer */
	ret = xmp_mixer_set(opaque, XMP_MIXER_INTERP, XMP_INTERP_NEAREST);
	fail_unless(ret == 0, "can't set XMP_INTERP_NEAREST");
	ret = xmp_mixer_get(opaque, XMP_MIXER_INTERP);
	fail_unless(ret == XMP_INTERP_NEAREST, "can't get XMP_INTERP_NEAREST");

	ret = xmp_mixer_set(opaque, XMP_MIXER_INTERP, XMP_INTERP_LINEAR);
	fail_unless(ret == 0, "can't set XMP_INTERP_LINEAR");
	ret = xmp_mixer_get(opaque, XMP_MIXER_INTERP);
	fail_unless(ret == XMP_INTERP_LINEAR, "can't get XMP_INTERP_LINEAR");

	ret = xmp_mixer_set(opaque, XMP_MIXER_INTERP, -2);
	fail_unless(ret < 0, "error setting invalid interpolation");

	ret = xmp_mixer_get(opaque, XMP_MIXER_INTERP);
	fail_unless(ret == XMP_INTERP_LINEAR, "invalid interpolation value set");

	/* dsp */
	ret = xmp_mixer_set(opaque, XMP_MIXER_DSP, 255);
	fail_unless(ret == 0, "can't set XMP_MIXER_DSP");
	ret = xmp_mixer_get(opaque, XMP_MIXER_DSP);
	fail_unless(ret == 255, "can't get XMP_MIXER_DSP");

	/* mix */
	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, 0);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_mixer_get(opaque, XMP_MIXER_MIX);
	fail_unless(ret == 0, "can't get XMP_MIXER_MIX");

	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, 100);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_mixer_get(opaque, XMP_MIXER_MIX);
	fail_unless(ret == 100, "can't get XMP_MIXER_MIX");

	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, -100);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_mixer_get(opaque, XMP_MIXER_MIX);
	fail_unless(ret == -100, "can't get XMP_MIXER_MIX");

	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, 50);
	fail_unless(ret == 0, "error setting mix");
	ret = xmp_mixer_get(opaque, XMP_MIXER_MIX);
	fail_unless(ret == 50, "can't get XMP_MIXER_MIX");

	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, 101);
	fail_unless(ret < 0, "error setting invalid mix");
	ret = xmp_mixer_set(opaque, XMP_MIXER_MIX, -101);
	fail_unless(ret < 0, "error setting invalid mix");

	ret = xmp_mixer_get(opaque, XMP_MIXER_MIX);
	fail_unless(ret == 50, "invalid mix values set");

	/* amp */
	ret = xmp_mixer_set(opaque, XMP_MIXER_AMP, 0);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_mixer_get(opaque, XMP_MIXER_AMP);
	fail_unless(ret == 0, "can't get XMP_MIXER_AMP");

	ret = xmp_mixer_set(opaque, XMP_MIXER_AMP, 3);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_mixer_get(opaque, XMP_MIXER_AMP);
	fail_unless(ret == 3, "can't get XMP_MIXER_AMP");

	ret = xmp_mixer_set(opaque, XMP_MIXER_AMP, 2);
	fail_unless(ret == 0, "error setting amp");
	ret = xmp_mixer_get(opaque, XMP_MIXER_AMP);
	fail_unless(ret == 2, "can't get XMP_MIXER_AMP");

	ret = xmp_mixer_set(opaque, XMP_MIXER_AMP, 101);
	fail_unless(ret < 0, "error setting invalid amp");
	ret = xmp_mixer_set(opaque, XMP_MIXER_AMP, -1);
	fail_unless(ret < 0, "error setting invalid amp");

	ret = xmp_mixer_get(opaque, XMP_MIXER_AMP);
	fail_unless(ret == 2, "invalid amp values set");

}
END_TEST
