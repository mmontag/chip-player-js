#include "test.h"

/* Inputs to test some bounds checks for the DI header.
 */

TEST(test_fuzzer_prowizard_di_invalid_offsets)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused signed integer overflows due to incorrect
	 * ordering of bounds checks for the offsets fields. */
	ret = xmp_load_module(opaque, "data/f/prowizard_di_invalid_offsets");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	/* Coverage for the second offsets check. */
	ret = xmp_load_module(opaque, "data/f/prowizard_di_invalid_offsets2");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	/* Coverage for the third offsets check. */
	ret = xmp_load_module(opaque, "data/f/prowizard_di_invalid_offsets3");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
