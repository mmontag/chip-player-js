#include "test.h"

/* These inputs caused hangs and UMRs due to a missing EOF checks
 * in the STMIK loader.
 */

TEST(test_fuzzer_stx_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	/* Truncated pattern (hang). */
	ret = xmp_load_module(opaque, "data/f/load_stx_truncated.stx");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	/* Truncated instrument name (UMR). */
	ret = xmp_load_module(opaque, "data/f/load_stx_truncated2.stx");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
