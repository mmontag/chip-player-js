#include "test.h"

/* This input caused uninitialized reads in the IMS test function due
 * to not checking the hio_read return value when reading the magic.
 */

TEST(test_fuzzer_ims_truncated_magic)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ims_truncated_magic.ims");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
