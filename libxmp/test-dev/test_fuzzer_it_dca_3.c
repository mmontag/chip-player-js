#include "test.h"

/* The IT loader allows duplicate check action values of 3 (which
 * are found in some .ITs) but this caused an out-of-bounds read.
 */

TEST(test_fuzzer_it_dca_3)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_it_dca_3.it");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
