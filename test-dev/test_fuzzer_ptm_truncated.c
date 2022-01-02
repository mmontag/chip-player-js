#include "test.h"

/* This input caused uninitialized reads in the PTM loader due to
 * a missing EOF check after loading the channel settings/order tables.
 */

TEST(test_fuzzer_ptm_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ptm_truncated.ptm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
