#include "test.h"

/* This input caused out-of-bounds reads in the Galaxy 4.0 loader
 * due to incorrectly bounded envelope point counts.
 */

TEST(test_fuzzer_gal4_env_point_bound)
{
	xmp_context opaque;
	struct xmp_module_info info;
	FILE *f;
	int ret;

	f = fopen("data/f/load_gal4_env_point_bound.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/load_gal4_env_point_bound");
	fail_unless(ret == 0, "module load");

	xmp_get_module_info(opaque, &info);

	ret = compare_module(info.mod, f);
	fail_unless(ret == 0, "format not correctly loaded");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
