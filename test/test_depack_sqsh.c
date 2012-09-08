#include "test.h"


TEST(test_depack_sqsh)
{
	xmp_context c;
	struct xmp_module_info info;
	int ret;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");
	ret = xmp_load_module(c, "data/PRU2.PDX-Perihelion");
	fail_unless(ret == 0, "can't load module");

	xmp_player_start(c, 44100, 0);
	xmp_player_get_info(c, &info);

	ret = compare_md5(info.mod->md5, "70e931a4c9835cd60493ae3d22b0cea621");
	fail_unless(ret == 0, "MD5 error");
}
END_TEST
