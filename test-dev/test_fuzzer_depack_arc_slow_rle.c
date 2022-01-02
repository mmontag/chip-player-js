#include "test.h"

/* This input caused slow loads in the ARC/ArcFS LZW decoder due to
 * not aborting long codes and RLE strings early when at the end of
 * the output stream.
 */

TEST(test_fuzzer_depack_arc_slow_rle)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arc_slow_rle");
	fail_unless(ret == -XMP_ERROR_FORMAT, "depacking");

	xmp_free_context(opaque);
}
END_TEST
