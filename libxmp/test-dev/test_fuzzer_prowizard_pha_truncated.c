#include "test.h"

/* This input caused an out-of-bounds read in the Pha depacker due to
 * missing bounds checks when reading from the pattern data array.
 */

TEST(test_fuzzer_prowizard_pha_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pha_truncated");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
