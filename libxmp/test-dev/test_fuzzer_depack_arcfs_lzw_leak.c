#include "test.h"

/* These inputs caused leaks in the arc and ArcFS depackers due to
 * their shared LZW decoder failing to free a pointer.
 */

TEST(test_fuzzer_depack_arcfs_lzw_leak)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arc_lzw_leak");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	ret = xmp_load_module(opaque, "data/f/depack_arcfs_lzw_leak");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
