#include "test.h"


TEST(test_depack_arcfs)
{
	xmp_context c;
	struct xmp_module_info info;
	int ret;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");
	ret = xmp_load_module(c, "data/arcfsdata");
	fail_unless(ret == 0, "can't load module");

	xmp_player_start(c, 44100, 0);
	xmp_player_get_info(c, &info);

	ret = compare_md5(info.mod->md5, "1c41df267ebb8febe5e3d8a7e45bad61");
	fail_unless(ret == 0, "MD5 error");
}
END_TEST
