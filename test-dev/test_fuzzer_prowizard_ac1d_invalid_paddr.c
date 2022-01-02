#include "test.h"

/* This input caused integer overflows in the ac1d depacker due to
 * computing (unused) pattern sizes from invalid pattern addresses.
 */

TEST(test_fuzzer_prowizard_ac1d_invalid_paddr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_ac1d_invalid_paddr");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
