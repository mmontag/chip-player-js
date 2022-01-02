#include "test.h"

/* This input caused uninitialized reads in the DBM loader due to
 * not checking the instrument name hio_read return value.
 */

TEST(test_fuzzer_dbm_truncated_inst)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dbm_truncated_inst.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
