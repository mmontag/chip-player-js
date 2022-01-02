#include "test.h"

/* These inputs caused out-of-bounds writes to the track data array in
 * The Player 4.x depacker due to missing bounds checks on invalid packing.
 */

TEST(test_fuzzer_prowizard_p40_invalid_packing)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_p40_invalid_packing");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
