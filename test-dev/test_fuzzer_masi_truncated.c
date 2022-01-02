#include "test.h"


TEST(test_fuzzer_masi_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused uninitialized reads in the MASI loader
	 * due to a missing EOF check when checking the pattern count.
	 */
	ret = xmp_load_module(opaque, "data/f/load_masi_truncated.psm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	/* This input caused uninitialized reads in the MASI loader
	 * due to a missing EOF check when reading the TITL chunk.
	 */
	ret = xmp_load_module(opaque, "data/f/load_masi_truncated2.psm");
	fail_unless(ret == 0, "module load (2)");

	xmp_free_context(opaque);
}
END_TEST
