#include "test.h"

/* Inputs that caused crashes in libxmp's Ogg Vorbis decoder (stb-vorbis).
 */

TEST(test_fuzzer_xm_vorbis_crash)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* This input OXM caused NULL dereferences in stb-vorbis.
	 */
	ret = xmp_load_module(opaque, "data/f/load_xm_vorbis_crash.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (1)");

	/* This input OXM caused double frees in stb-vorbis due to
	 * a libxmp fix conflicting with upstream. */
	ret = xmp_load_module(opaque, "data/f/load_xm_vorbis_crash2.oxm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (2)");

	xmp_free_context(opaque);
}
END_TEST
