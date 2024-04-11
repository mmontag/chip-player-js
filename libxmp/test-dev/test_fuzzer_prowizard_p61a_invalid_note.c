#include "test.h"

/* These inputs caused out-of-bounds reads in the The Player 6.1a
 * depacker due to a missing note bounds check. All 5 tests triggered
 * in different places due to multiple note packing cases in this format.
 */

TEST(test_fuzzer_prowizard_p61a_invalid_note)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note2");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note3");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note4");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note5.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	ret = xmp_load_module(opaque, "data/f/prowizard_p61a_invalid_note6");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
