#include "test.h"

/* These inputs caused leaks in the MDL loader due to duplicate IS
 * chunks being allowed if the first chunk contained a sample count
 * of 0.
 */

TEST(test_fuzzer_mdl_duplicate_is_chunk)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_duplicate_i0_chunk.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/load_mdl_duplicate_is_chunk.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
