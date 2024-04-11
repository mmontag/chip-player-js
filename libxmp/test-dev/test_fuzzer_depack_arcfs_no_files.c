#include "test.h"

/* This input caused uninitialized reads in the ArcFS depacker due to
 * not returning an error value when no usable files are found in the
 * archive header.
 */

TEST(test_fuzzer_depack_arcfs_no_files)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_arcfs_no_files");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
