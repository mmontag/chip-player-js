#include "test.h"

/* This input caused signed overflows in the Pha test function due to
 * a badly constructed bounds check on pattern addresses.
 */

TEST(test_fuzzer_prowizard_pha_invalid_paddr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pha_invalid_paddr");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
