#include "test.h"

/* This input caused slow loads in the UMX loader due to containing a
 * large invalid number of type names.
 */

TEST(test_fuzzer_umx_invalid_names)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_umx_invalid_names.umx.xz");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
