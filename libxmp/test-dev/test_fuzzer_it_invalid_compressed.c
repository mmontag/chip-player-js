#include "test.h"

/* Inputs that caused issues in IT sample depacking.
 */

TEST(test_fuzzer_it_invalid_compressed)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input caused hangs and high RAM consumption in the IT loader
	 * due to allocating large buffers for invalid compressed samples. */
	ret = xmp_load_module(opaque, "data/f/load_it_invalid_compressed.it");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	/* This input resulted in invalid shift exponents in read_bits. */
	ret = xmp_load_module(opaque, "data/f/load_it_invalid_compressed2.it");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
