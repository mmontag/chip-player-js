#include "test.h"

/* This input caused leaks in the MDL loader due to duplicate PA
 * chunks being allowed if the first chunk contained a pattern count
 * of 0.
 */

TEST(test_fuzzer_mdl_duplicate_pa_chunk)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_duplicate_pa_chunk.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
