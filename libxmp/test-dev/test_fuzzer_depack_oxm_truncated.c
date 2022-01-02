#include "test.h"

/* These inputs caused hangs, leaks, or crashes in stb_vorbis due to
 * missing or broken EOF checks in start_decoder.
 */

TEST(test_fuzzer_depack_oxm_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_oxm_truncated.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	ret = xmp_load_module(opaque, "data/f/depack_oxm_truncated2.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	ret = xmp_load_module(opaque, "data/f/depack_oxm_truncated3.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	ret = xmp_load_module(opaque, "data/f/depack_oxm_truncated4.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	xmp_free_context(opaque);
}
END_TEST
