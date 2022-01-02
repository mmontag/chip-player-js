#include "test.h"

/* This input caused hangs and excessive memory consumption in the S3M
 * loader due to invalid sample sizes.
 */

TEST(test_fuzzer_s3m_invalid_sample_size2)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_s3m_invalid_sample_size2.s3m");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
