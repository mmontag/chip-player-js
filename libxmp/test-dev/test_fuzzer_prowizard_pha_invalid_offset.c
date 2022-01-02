#include "test.h"

/* This input caused an out-of-bounds writes in the Pha Packer loader
 * due to allowing negative pattern offsets and expecting modulo to
 * return positive values for them.
 */

TEST(test_fuzzer_prowizard_pha_invalid_offset)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pha_invalid_offset");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
