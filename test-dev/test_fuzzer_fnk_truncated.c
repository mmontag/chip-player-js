#include "test.h"

/* This input caused UMRs in the FNK loader due to not checking
 * the hio_read return value when reading pattern events.
 */

TEST(test_fuzzer_fnk_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_fnk_truncated.fnk");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
