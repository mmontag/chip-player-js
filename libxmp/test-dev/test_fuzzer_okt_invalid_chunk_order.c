#include "test.h"

/* This input caused freeing of uninitialized pointers due to
 * allowing SLEN and PBOD chunks before a CMOD chunk.
 */

TEST(test_fuzzer_okt_invalid_chunk_order)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_okt_invalid_chunk_order.okt");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
