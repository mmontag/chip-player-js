#include "test.h"

/* This input caused undefined behavior in the XM loader due to
 * having a broken instrument header size bounds check.
 */

TEST(test_fuzzer_xm_negative_instsize)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_xm_negative_instsize.xm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
