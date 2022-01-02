#include "test.h"

/* This input caused signed overflows in the XANN depacker due to
 * using signed ints to calculate sample loop starts that were
 * previously bounded with uint32s.
 */

TEST(test_fuzzer_prowizard_xann_invalid)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_xann_invalid");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
