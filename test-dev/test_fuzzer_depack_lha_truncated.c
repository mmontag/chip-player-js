#include "test.h"

/* This input caused an out-of-bounds write in the LHA depacker
 * due to interpreting EOFs as valid input data.
 */

TEST(test_fuzzer_depack_lha_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_lha_truncated.lha");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
