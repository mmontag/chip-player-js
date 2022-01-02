#include "test.h"

/* This input caused hangs in the MASI loader due to invalid lengths
 * getting interpreted as reverse seeks.
 */

TEST(test_fuzzer_masi_seek_loop)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_masi_seek_loop.psm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
