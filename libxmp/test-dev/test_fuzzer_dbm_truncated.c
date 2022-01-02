#include "test.h"

TEST(test_fuzzer_dbm_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused hangs in the DBM loader due to not checking
	 * for EOF when loading pattern data.
	 */
	ret = xmp_load_module(opaque, "data/f/load_dbm_truncated.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	/* This input caused UMRs when reading the truncated song title. */
	ret = xmp_load_module(opaque, "data/f/load_dbm_truncated2.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
