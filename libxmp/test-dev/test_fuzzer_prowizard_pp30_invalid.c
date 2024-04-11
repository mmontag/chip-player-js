#include "test.h"

/* This input caused out-of-bounds reads in the ProPacker 3.0
 * test function due to not requesting ANY data.
 */

TEST(test_fuzzer_prowizard_pp30_invalid)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pp30_invalid.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
