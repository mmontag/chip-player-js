#include "test.h"

/* Relies on a quirk in Digital Symphony's LZW encoder where
 * the EOF is encoded with the old code length if the code
 * length is increased by the code immediately before the EOF.
 */

TEST(test_loader_sym_lzwquirk)
{
	xmp_context opaque;
	struct xmp_module_info info;
	FILE *f;
	int ret;

	f = fopen("data/format_sym_lzwquirk.data", "r");

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/m/newdance.dsym");
	fail_unless(ret == 0, "module load");

	xmp_get_module_info(opaque, &info);

	ret = compare_module(info.mod, f);
	fail_unless(ret == 0, "format not correctly loaded");

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
