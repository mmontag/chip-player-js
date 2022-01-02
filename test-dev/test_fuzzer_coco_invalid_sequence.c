#include "test.h"

/* This input caused heap corruption in the Coconizer loader due to
 * a missing bounds check when loading the sequence table.
 */

TEST(test_fuzzer_coco_invalid_sequence)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_coco_invalid_sequence");
	fail_unless(ret == 0 || ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
