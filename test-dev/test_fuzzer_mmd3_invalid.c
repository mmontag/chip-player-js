#include "test.h"

/* Checks for numerous read error, seek error, and invalid data cases
 * in the OctaMED MMD2/3 loader.
 */

static void mmd3_invalid_helper(xmp_context opaque, const char *filename)
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

TEST(test_fuzzer_mmd3_invalid)
{
	xmp_context opaque;

	opaque = xmp_create_context();

	/* This input caused uninititalized reads in the MMD2/3 loader due to
	 * not checking for an EOF after reading instrument names.
	 */
	mmd3_invalid_helper(opaque, "data/f/load_mmd3_truncated.med");

	/* This input caused memory corruption in the MMD2/3 loader due to
	 * junk data at an invalid block array table offset getting
	 * interpreted as valid block offsets.
	 */
	mmd3_invalid_helper(opaque, "data/f/load_mmd3_invalid_blockarr.med");

	/* Don't load MMD2/3s with too many channels. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_channel_count.med");

	/* MMD2/3 is still limited to 63 instruments. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_instrument_count.med");

	/* Invalid exp pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_expdata.med");

	/* Truncated exp/invalid InstrExt pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_expdata2.med");

	/* Truncated exp/invalid songname pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_expdata3.med");

	/* Invalid smplarr pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_smplarr.med");

	/* Truncated smplarr array. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_smplarr2.med");

	/* Invalid instrument pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_instptr.med");

	/* Truncated InstrExt array. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_instrext.med");

	/* Truncated InstrInfo array. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_instrinfo.med");

	/* Invalid blockarr pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_blockarr.med");

	/* Truncated blockarr array. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_blockarr2.med");

	/* Invalid block pointer. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_blockptr.med");

	/* Invalid block line count. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_blocklines.med");

	/* Truncated block. */
	mmd3_invalid_helper(opaque, "data/f/load_mmd2_invalid_block.med");

	xmp_free_context(opaque);
}
END_TEST
