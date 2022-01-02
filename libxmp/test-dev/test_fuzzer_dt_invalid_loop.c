#include "test.h"

/* This input caused signed integer overflows in the DT loader due
 * to using ints instead of unsigned ints for calculating the sample
 * loop end. The sample values in this format seem to be badly
 * behaved in general, hence the lack of checks on these.
 */

TEST(test_fuzzer_dt_invalid_loop)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_dt_invalid_loop.dtm");
	fail_unless(ret == 0, "module load");

	xmp_free_context(opaque);
}
END_TEST
