#include "test.h"

/* This input caused uninitialized reads in the AMF loader due to
 * a missing hio_read return value check.
 */

TEST(test_fuzzer_amf_truncated2)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_amf_truncated2.amf");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
