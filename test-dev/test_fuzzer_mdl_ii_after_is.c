#include "test.h"

/* This input caused leaks in the MDL loader due to the II handler
 * allocating sample arrays after an IS chunk already allocated them.
 */

TEST(test_fuzzer_mdl_ii_after_is)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mdl_ii_after_is.mdl");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
