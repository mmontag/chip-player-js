#include "test.h"

/* Misc. fuzzer input tests that don't really fit anywhere else.
 */

TEST(test_fuzzer_misc)
{
	xmp_context opaque;
	/* 0x84 is specifically to check for a Coconizer bug. */
	const char buf[9] = "\x84ZCDEFGH";
	char msg[32];
	int ret;
	int i;

	opaque = xmp_create_context();

	/* Zero-length inputs previously caused out-of-bounds reads. */
	ret = xmp_load_module_from_memory(opaque, (const void *)1, 0);
	fail_unless(ret == -XMP_ERROR_INVALID, "module load (0 length)");

	/* Tiny inputs caused uninitialized reads in some test functions. */
	for (i = 1; i < sizeof(buf); i++) {
		sprintf(msg, "module load (%d length)", i);
		ret = xmp_load_module_from_memory(opaque, buf, i);
		fail_unless(ret == -XMP_ERROR_FORMAT, msg);
	}

	xmp_free_context(opaque);
}
END_TEST
