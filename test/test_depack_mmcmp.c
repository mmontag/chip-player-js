#include "test.h"


TEST(test_depack_mmcmp)
{
	xmp_context c;
	struct xmp_module_info info;
	int ret;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");
	ret = xmp_load_module(c, "data/test.mmcmp");
	fail_unless(ret == 0, "can't load module");

	xmp_player_start(c, 44100, 0);
	xmp_player_get_info(c, &info);

	ret = compare_md5(info.mod->md5, "2d8b03b2bce0563dfdf89613c7976fe4");
	fail_unless(ret == 0, "MD5 error");
}
END_TEST
