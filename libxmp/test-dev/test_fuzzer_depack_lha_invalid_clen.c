#include "test.h"

/* This input caused out-of-bounds writes to the c_len table in the LHA
 * depacker due to a missing check on the maximum table length.
 */

TEST(test_fuzzer_depack_lha_invalid_clen)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_lha_invalid_clen.lha");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
