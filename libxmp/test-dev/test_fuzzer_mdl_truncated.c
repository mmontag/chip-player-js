#include "test.h"

/* Tests for truncated MDL modules that caused problems in the MDL loader.
 */

TEST(test_fuzzer_mdl_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused UMRs in the MDL loader due to not checking
	 * the hio_read return value for a truncated instrument name. */
	ret = xmp_load_module(opaque, "data/f/load_mdl_truncated.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	/* This input caused UMRs in the MDL loader due to not checking
	 * the hio_read return value for the format version. */
	ret = xmp_load_module(opaque, "data/f/load_mdl_truncated2.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
