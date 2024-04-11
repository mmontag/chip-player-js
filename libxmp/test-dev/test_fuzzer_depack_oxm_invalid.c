#include "test.h"


TEST(test_fuzzer_depack_oxm_invalid)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused an out-of-bounds write in stb_vorbis due to
	 * a missing negative return value check. */
	ret = xmp_load_module(opaque, "data/f/depack_oxm_invalid.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	/* This input caused undefined behavior attempting to seek past
	 * pattern data in the oxm test function. */
	ret = xmp_load_module(opaque, "data/f/depack_oxm_invalid2.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	xmp_free_context(opaque);
}
END_TEST
