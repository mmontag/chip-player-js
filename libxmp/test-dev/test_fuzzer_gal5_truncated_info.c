#include "test.h"

/* This input caused uninitialized reads in the Galaxy 5.0 loader due to
 * a missing return value check when reading the channel array in the INFO chunk.
 * This needs to be tested using xmp_load_module_from_memory.
 */

TEST(test_fuzzer_gal5_truncated_info)
{
	xmp_context opaque;
	void *buffer;
	long size;
	int ret;

	read_file_to_memory("data/f/load_gal5_truncated_info", &buffer, &size);
	fail_unless(buffer != NULL, "read file to memory");

	opaque = xmp_create_context();
	ret = xmp_load_module_from_memory(opaque, buffer, size);
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
	free(buffer);
}
END_TEST
