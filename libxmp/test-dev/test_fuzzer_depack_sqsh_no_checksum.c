#include "test.h"

/* This input caused uninitialized reads in the XPK depacker due to
 * the checksum being read from uninitialized (but allocated) data.
 */

TEST(test_fuzzer_depack_sqsh_no_checksum)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_sqsh_no_checksum.xpk");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
