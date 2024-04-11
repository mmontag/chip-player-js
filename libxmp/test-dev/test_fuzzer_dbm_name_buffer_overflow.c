#include "test.h"

/* This input caused the DBM loader to read out-of-bounds stack
 * memory due to incorrect usage of strncpy.
 */

TEST(test_fuzzer_dbm_name_buffer_overflow)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dbm_name_buffer_overflow.dbm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
