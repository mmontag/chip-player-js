#include "test.h"

/* This input caused leaks in the Megatracker loader due to
 * loading patterns beyond the maximum number supported by libxmp.
 */

TEST(test_fuzzer_mgt_patterns_bound)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mgt_patterns_bound.mgt");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
