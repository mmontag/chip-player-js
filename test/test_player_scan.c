#include "test.h"

TEST(test_player_scan)
{
	xmp_context opaque;
	struct xmp_module_info info;

	opaque = xmp_create_context();
	xmp_load_module(opaque, "data/ode2ptk.mod");
	xmp_player_start(opaque, 44100, 0);
	xmp_player_get_info(opaque, &info);
	fail_unless(info.total_time == 85472, "incorrect total time");
	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
