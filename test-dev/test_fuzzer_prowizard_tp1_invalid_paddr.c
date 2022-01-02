#include "test.h"

/* This input caused signed overflows in the TP1 loader due to
 * strange pattern address calculation being done in signed ints
 * instead of unsigned ints.
 */

TEST(test_fuzzer_prowizard_tp1_invalid_paddr)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_tp1_invalid_paddr");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
