#include "test.h"

/* This input caused a leak in the OXM depacker due to not freeing
 * previously allocated samples when one failed.
 */

TEST(test_fuzzer_depack_oxm_pcm_leak)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/depack_oxm_pcm_leak.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "depacking");

	xmp_free_context(opaque);
}
END_TEST
