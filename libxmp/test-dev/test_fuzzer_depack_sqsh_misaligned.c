#include "test.h"

/* This input caused misaligned reads in the XPK depacker due to
 * misusage of type punning.
 */

TEST(test_fuzzer_depack_sqsh_misaligned)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_sqsh_misaligned.xpk");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
