#include "test.h"


TEST(test_depack_zip)
{
	xmp_context c;
	struct xmp_module_info info;
	int ret;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");
	ret = xmp_load_module(c, "data/zipdata1");
	fail_unless(ret == 0, "can't load module");

	xmp_player_start(c, 44100, 0);
	xmp_player_get_info(c, &info);

	ret = compare_md5(info.mod->md5, "a0b5bedbe15e1053ba6bd5645898e6c5");
	fail_unless(ret == 0, "MD5 error");
}
END_TEST
