#include "test.h"

/* This input caused freeing of uninitialized pointers due to a
 * duplicate of the SLEN chunk (pattern count) existing.
 */

TEST(test_fuzzer_okt_duplicate_chunk)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_okt_duplicate_chunk.okt");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
