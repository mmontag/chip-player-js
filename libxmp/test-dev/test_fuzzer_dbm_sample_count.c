#include "test.h"

/* This input caused leaks in the DBM loader due to allocating
 * too many samples (~16400). If the maximum number of samples is
 * increased above this, this input loads.
 */

TEST(test_fuzzer_dbm_sample_count)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dbm_sample_count.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
