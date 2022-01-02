#include "test.h"


TEST(test_depack_rar)
{
	xmp_context c;
	struct xmp_module_info info;
	int ret;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");
	ret = xmp_load_module(c, "data/ponylips.rar");

	/* This uses an external depacker currently, so if
	 * depacking fails, it's most likely because unrar isn't
	 * available. If this happens, just let the test pass. */
	if (ret != -XMP_ERROR_DEPACK && ret != -XMP_ERROR_FORMAT) {
		FILE *f;

		fail_unless(ret == 0, "can't load module");

		xmp_get_module_info(c, &info);

		f = fopen("data/format_mod_notawow.data", "r");

		ret = compare_module(info.mod, f);
		fail_unless(ret == 0, "RARed module not correctly loaded");
	}
	xmp_release_module(c);
	xmp_free_context(c);
}
END_TEST
