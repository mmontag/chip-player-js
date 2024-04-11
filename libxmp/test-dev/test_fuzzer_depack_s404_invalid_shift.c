#include "test.h"

/* This input caused shifts by invalid exponents in the S404 depacker.
 */

TEST(test_fuzzer_depack_s404_invalid_shift)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_s404_invalid_shift");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
