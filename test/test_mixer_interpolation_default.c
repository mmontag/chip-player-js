#include "test.h"

TEST(test_mixer_interpolation_default)
{
	xmp_context opaque;
	int interp;

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/test.xm");
	xmp_player_start(opaque, 8000, XMP_FORMAT_MONO);
	interp = xmp_mixer_get(opaque, XMP_MIXER_INTERP);
	fail_unless(interp == XMP_INTERP_LINEAR, "default not linear");
}
END_TEST
