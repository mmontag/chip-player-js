#include "test.h"

/* Rare WOW file with an odd length due to an extra byte. */
TEST(test_loader_wow_extra)
{
	xmp_context opaque;
	struct xmp_module_info info;
	FILE *f;
	int ret;

	f = fopen("data/format_wow_extra.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/m/acidfunk.wow");
	fail_unless(ret == 0, "module load");

	xmp_get_module_info(opaque, &info);

	ret = compare_module(info.mod, f);
	fail_unless(ret == 0, "format not correctly loaded");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
