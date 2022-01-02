#include "test.h"

/* Checks for numerous read error, seek error, and invalid data cases
 * in the OctaMED MMD0/1 loader.
 */

static void mmd1_invalid_helper(xmp_context opaque, const char *filename)
{
	void *buf = NULL;
	long bufsz = 0;
	int ret;

	read_file_to_memory(filename, &buf, &bufsz);
	fail_unless(buf, filename);

	ret = xmp_load_module_from_memory(opaque, buf, bufsz);
	fail_unless(ret == -XMP_ERROR_LOAD, filename);
	free(buf);
}

TEST(test_fuzzer_mmd1_invalid)
{
	xmp_context opaque;

	opaque = xmp_create_context();

	/* This input caused stack corruption in the MMD0/1 test function
	 * due to large uint32 values (in this case, EOF) being interpreted
	 * as a negative offset in libxmp_read_title.
	 */
	mmd1_invalid_helper(opaque, "data/f/load_mmd0_truncated.med");

	/* This input caused uninitialized instrument name reads. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_truncated.med");

	/* MMD0/1 is limited to 63 instruments. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_instrument_count.med");

	/* Invalid exp pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_expdata.med");

	/* Truncated exp/invalid annotxt pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_expdata2.med");

	/* Invalid smplarr pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_smplarr.med");

	/* Truncated smplarr array. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_smplarr2.med");

	/* Invalid instrument pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_instptr.med");

	/* Invalid instrument type. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_insttype.med");

	/* Invalid instrument waveform. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_instwform.med");

	/* Truncated InstrExt array. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_instrext.med");

	/* Truncated InstrInfo array. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_instrinfo.med");

	/* Invalid blockarr pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_blockarr.med");

	/* Truncated blockarr array. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_blockarr2.med");

	/* Invalid block pointer. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_blockptr.med");

	/* Invalid block line count. */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_blocklines.med");

	/* Truncated block (MMD0). */
	mmd1_invalid_helper(opaque, "data/f/load_mmd0_invalid_block.med");

	/* Truncated block (MMD1). */
	mmd1_invalid_helper(opaque, "data/f/load_mmd1_invalid_block.med");

	xmp_free_context(opaque);
}
END_TEST
