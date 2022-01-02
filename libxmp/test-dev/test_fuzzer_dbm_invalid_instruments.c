#include "test.h"

/* This input crashed the DBM loader due to a bug where the
 * instrument ID was being used to index the samples array.
 * This input also contains a very undersized INST chunk that
 * should be immediately rejected.
 */

TEST(test_fuzzer_dbm_invalid_instruments)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dbm_invalid_instruments.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
