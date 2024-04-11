#include "test.h"

/* This input caused UMRs in the Galaxy 4.0 loader due to
 * not checking the hio_read return value for the module title.
 */

TEST(test_fuzzer_gal4_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gal4_truncated");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
