#include "test.h"

/* This input caused uninitialized reads in libxmp_read_title due to
 * that function not checking the return value of hio_read.
 */

TEST(test_fuzzer_sym_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_sym_truncated.sym");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
