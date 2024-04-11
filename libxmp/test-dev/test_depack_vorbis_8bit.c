#include "test.h"


TEST(test_depack_vorbis_8bit)
{
	int i, ret;
	void *buf;
	int8 *pcm8;
	long size;
	xmp_context c;
	struct xmp_module_info info;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");

	ret = xmp_load_module(c, "data/jerry-boleti.oxm");
	fail_unless(ret == 0, "can't load module");

	xmp_start_player(c, 44100, 0);
	xmp_get_module_info(c, &info);

	read_file_to_memory("data/sample4.raw", &buf, &size);
	fail_unless(buf != NULL, "can't open raw data file");

	pcm8 = (int8 *)info.mod->xxs[4].data;

	for (i = 0; i < 5492; i++) {
		if (pcm8[i] != ((int8 *)buf)[i])
			fail_unless(abs(pcm8[i] - ((int8 *)buf)[i]) <= 1, "data error");
	}

	xmp_release_module(c);
	xmp_free_context(c);
	free(buf);
}
END_TEST
