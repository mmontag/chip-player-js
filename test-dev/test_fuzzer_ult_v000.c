#include "test.h"

/* This input caused an out of bounds read due to the Ultra Tracker
 * loader allowing "MAS_UTrack_V000" as an ID. This doesn't seem to
 * be an actual magic string used by any ULT modules.
 */

TEST(test_fuzzer_ult_v000)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_ult_v000.ult");
	fail_unless(ret == -XMP_ERROR_FORMAT, "module load");

	xmp_free_context(opaque);
}
END_TEST
