#include "test.h"

/* This input caused large allocations in the ARC/ArcFS LZW decoder
 * due to having a large original length field.
 */

TEST(test_fuzzer_depack_arc_large_output)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arc_large_output");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
