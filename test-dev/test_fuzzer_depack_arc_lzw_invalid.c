#include "test.h"

/* This input caused out of bounds reads in the ARC depacker when
 * attempting to find the first character of some invalid strings.
 */

TEST(test_fuzzer_depack_arc_lzw_invalid)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arc_lzw_invalid");
	fail_unless(ret == -XMP_ERROR_FORMAT, "depacking");

	xmp_free_context(opaque);
}
END_TEST
