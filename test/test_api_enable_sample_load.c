#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "test.h"

TEST(test_api_enable_sample_load)
{
	xmp_context ctx;
	struct xmp_module_info mi;
	int ret;

	ctx = xmp_create_context();

	/* disable sample load */
	xmp_enable_sample_load(ctx, 0);
	ret = xmp_load_module(ctx, "data/test.xm");
	fail_unless(ret == 0, "load file");
	xmp_get_module_info(ctx, &mi);
	fail_unless(mi.mod->xxs[0].data == NULL, "disable sample 0 load");
	fail_unless(mi.mod->xxs[1].data == NULL, "disable sample 1 load");
	xmp_release_module(ctx);

	/* enable sample load */
	xmp_enable_sample_load(ctx, 1);
	ret = xmp_load_module(ctx, "data/test.xm");
	fail_unless(ret == 0, "load file");
	xmp_get_module_info(ctx, &mi);
	fail_unless(mi.mod->xxs[0].data != NULL, "enable sample 0 load");
	fail_unless(mi.mod->xxs[1].data != NULL, "enable sample 1 load");
	xmp_release_module(ctx);
}
END_TEST
