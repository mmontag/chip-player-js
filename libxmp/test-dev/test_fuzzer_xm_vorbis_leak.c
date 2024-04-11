#include "test.h"

/* Inputs that caused leaks in libxmp's Ogg Vorbis decoder (stb-vorbis).
 */

TEST(test_fuzzer_xm_vorbis_leak)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input OXM caused leaks of start_decoder's temporary buffers.
	 */
	ret = xmp_load_module(opaque, "data/f/load_xm_vorbis_leak.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	xmp_free_context(opaque);
}
END_TEST
