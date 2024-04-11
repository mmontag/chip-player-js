#include "test.h"

/* This input caused leaks in the Protracker 3 loader due to
 * containing multiple PTDT chunks.
 */

TEST(test_fuzzer_pt3_ptdt_leak)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_pt3_ptdt_leak.pt36");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
