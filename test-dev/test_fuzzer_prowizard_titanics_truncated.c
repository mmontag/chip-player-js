#include "test.h"

/* This input caused an out-of-bounds read in the Titanics test function
 * due to not bounding its pattern address reads.
 */

TEST(test_fuzzer_prowizard_titanics_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_titanics_truncated");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
