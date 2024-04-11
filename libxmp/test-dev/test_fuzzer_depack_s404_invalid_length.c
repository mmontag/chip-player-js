#include "test.h"

/* Make sure that inputs with an impossibly high ratio for StoneCracker 4.04
 * compression get rejected before allocating a lot of RAM.
 */

TEST(test_fuzzer_depack_s404_invalid_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_s404_invalid_length");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depacking");

	xmp_free_context(opaque);
}
END_TEST
