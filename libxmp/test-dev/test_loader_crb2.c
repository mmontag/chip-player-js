#include "test.h"

/* An alternate test file for Heatseeker 1.0.
 * This test was added under the wrong assumption that the first test
 * did not exist. Since parts of this loader are a little sketchy,
 * having two test files shouldn't hurt.
 */

TEST(test_loader_crb2)
{
	xmp_context opaque;
	struct xmp_module_info info;
	FILE *f;
	int ret;

	f = fopen("data/format_crb_2.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/m/CRB.Icicle_Beat");
	fail_unless(ret == 0, "module load");

	xmp_get_module_info(opaque, &info);

	ret = compare_module(info.mod, f);
	fail_unless(ret == 0, "format not correctly loaded");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
