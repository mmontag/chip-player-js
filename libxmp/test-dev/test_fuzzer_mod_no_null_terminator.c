#include "test.h"

/* This input caused potential crashes in the MOD loader due to
 * attempting to use strlen on raw (non-terminated) instrument names.
 */

TEST(test_fuzzer_mod_no_null_terminator)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mod_no_null_terminator.mod");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
