#include "test.h"

/* This input crashed the Digital Tracker loader due to duplicate
 * PATT chunks corrupting the channel and track counts, causing heap
 * corruption when generating empty patterns.
 */

TEST(test_fuzzer_dt_duplicate_chunk)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dt_duplicate_chunk.dtm");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
