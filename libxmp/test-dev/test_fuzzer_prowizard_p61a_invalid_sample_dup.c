#include "test.h"

/* This input caused out-of-bounds reads in the The Player 6.1a
 * depacker due to a faulty check on duplicate samples.
 */

TEST(test_fuzzer_prowizard_p61a_invalid_sample_dup)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_sample_dup");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
