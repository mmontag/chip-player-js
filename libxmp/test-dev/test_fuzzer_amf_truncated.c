#include "test.h"

/* This input caused slow loads in the AMF loader due to inadequate
 * EOF detection.
 */

TEST(test_fuzzer_amf_truncated)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_amf_truncated.amf");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
