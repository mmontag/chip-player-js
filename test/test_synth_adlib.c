#include "test.h"

TEST(test_synth_adlib)
{
	xmp_context opaque;
	struct xmp_module_info info;
	int i, j, k, val, ret;
	FILE *f;

	f = fopen("data/adlib.data", "r");

	opaque = xmp_create_context();
	fail_unless(opaque != NULL, "can't create context");

	ret = xmp_load_module(opaque, "data/adlibsp.rad.gz");
	fail_unless(ret == 0, "can't load module");

	xmp_player_start(opaque, 44100, 0);

	for (j = 0; j < 2; j++) {
		xmp_player_frame(opaque);

		xmp_player_get_info(opaque, &info);
		int16 *b = info.buffer;

		for (k = i = 0; i < info.buffer_size / 4; i++) {
			fscanf(f, "%d", &val);
			fail_unless(b[k++] == val, "Adlib error L");
			fail_unless(b[k++] == val, "Adlib error R");
		}
	}
	
	xmp_player_end(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
