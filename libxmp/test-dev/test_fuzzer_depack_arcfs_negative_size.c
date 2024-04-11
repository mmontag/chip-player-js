#include "test.h"

/* This input caused high RAM usage in the ArcFS depacker due to
 * allowing negative file sizes.
 */

TEST(test_fuzzer_depack_arcfs_negative_size)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arcfs_negative_size");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
