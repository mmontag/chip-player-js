#include "test.h"

/* This input caused an endless loop in the inflate algorithm due to
 * missing EOF checks.
 */

TEST(test_fuzzer_depack_muse_truncated2)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_muse_truncated2.j2b");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
