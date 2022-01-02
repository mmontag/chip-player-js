#include "test.h"

/* These inputs caused high amounts of RAM usage, hangs, and undefined
 * behavior in the MDL loader instrument and sample chunk handlers.
 */

TEST(test_fuzzer_mdl_invalid_sample)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();

	/* Contains an invalid pack type of 3, but the loader would allocate
	 * a giant buffer for it before rejecting it.
	 */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_pack.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (pack)");

	/* Invalid sample size for a pack type 0 sample. */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_size.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (size)");

	/* Invalid sample size for a pack type 2 sample. */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_size2.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (size2)");

	/* Negative sample length (caused crashes). */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_size3.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (size3)");

	/* Negative loop start (caused overflows). */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_loop.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (loop)");

	/* Loop start past sample length (caused overflows). */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_loop2.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (loop2)");

	/* Loop length past sample end (caused overflows). */
	ret = xmp_load_module(opaque, "data/f/load_mdl_invalid_sample_loop3.mdl");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load (loop3)");

	xmp_free_context(opaque);
}
END_TEST
