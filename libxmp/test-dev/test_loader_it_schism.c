#include "test.h"

/* Test Schism Tracker version datestamping.
 */

TEST(test_loader_it_schism)
{
	xmp_context opaque;
	struct xmp_module_info info;
	FILE *f;
	int ret;

	f = fopen("data/format_it_schism.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/format_it_schism.it");
	fail_unless(ret == 0, "module load");

	xmp_get_module_info(opaque, &info);

	ret = compare_module(info.mod, f);
	fail_unless(ret == 0, "format not correctly loaded");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
