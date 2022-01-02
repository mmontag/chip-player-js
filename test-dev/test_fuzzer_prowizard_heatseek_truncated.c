#include "test.h"

/* This input caused an out-of-bounds read in the Heatseeker depacker
 * due to a broken bounds check. Only occurs when using xmp_load_module_from_memory.
 */

#define BUFFER_SIZE 2048

TEST(test_fuzzer_prowizard_heatseek_truncated)
{
	xmp_context opaque;
	FILE *f;
	char *buffer;
	size_t len;
	int ret;

	buffer = malloc(BUFFER_SIZE);
	fail_unless(buffer != NULL, "buffer alloc");

	f = fopen("data/f/prowizard_heatseek_truncated", "rb");
	fail_unless(f != NULL, "open file");
	len = fread(buffer, 1, BUFFER_SIZE, f);
	fclose(f);

	fail_unless(len > 0 && len < BUFFER_SIZE, "read error");

	opaque = xmp_create_context();
	ret = xmp_load_module_from_memory(opaque, buffer, len);
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
	free(buffer);
}
END_TEST
