#include "test.h"

/* This input caused signed overflows in the Promizer 1.8a loader
 * due to not bounding pattern addresses before trying to seek to them.
 */

TEST(test_fuzzer_prowizard_pm18a_invalid_paddr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_pm18a_invalid_paddr");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
