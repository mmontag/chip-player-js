#include "test.h"

/* This input crashed the MDL loader due to containing an IN
 * chunk after the PA/TR chunks, causing the allocated patterns
 * and tracks to differ from the pattern and track counts.
 */

TEST(test_fuzzer_mdl_invalid_chunk_order)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_chunk_order.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
