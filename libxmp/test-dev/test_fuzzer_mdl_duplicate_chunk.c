#include "test.h"

/* This input crashed the MDL loader due to containing a
 * duplicate IN chunk, which corrupted the channels count.
 * MDLs should never contain duplicate chunks.
 */

TEST(test_fuzzer_mdl_duplicate_chunk)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_duplicate_chunk.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
