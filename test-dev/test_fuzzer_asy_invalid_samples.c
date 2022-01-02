#include "test.h"


TEST(test_fuzzer_asy_invalid_samples)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* The ASYLUM format only supports up to 64 samples, reject higher counts.
	 */
	ret = xmp_load_module(opaque, "data/f/load_asy_invalid_samples.amf");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	/* Despite using 32-bit sample length fields, the ASYLUM format is
	 * converted from MOD and should never have samples longer than 128k.
	 * (All of the original ASYLUM module samples seem to be <64k.)
	 */
	ret = xmp_load_module(opaque, "data/f/load_asy_invalid_samples2.amf");
	fail_unless(ret = -XMP_ERROR_LOAD, "module load (2)");

	xmp_free_context(opaque);
}
END_TEST
