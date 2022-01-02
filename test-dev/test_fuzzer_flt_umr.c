#include "test.h"

/* This input caused uninitialized reads in the FLT loader due to
 * missing return value checks on hio_read.
 */

TEST(test_fuzzer_flt_umr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_flt_umr.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
