#include "test.h"

/* This input caused scan_module to attempt to endlessly scan
 * a pattern.
 */

TEST(test_fuzzer_mod_scan_row_limit)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_mod_scan_row_limit.mod.xz");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
