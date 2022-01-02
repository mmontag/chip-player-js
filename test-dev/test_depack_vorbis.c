#include "test.h"


TEST(test_depack_vorbis)
{
	int i, ret;
	void *buf;
	int16 *pcm16;
	long size;
	xmp_context c;
	struct xmp_module_info info;

	c = xmp_create_context();
	fail_unless(c != NULL, "can't create context");

	ret = xmp_load_module(c, "data/beep.oxm");
	fail_unless(ret == 0, "can't load module");

	xmp_start_player(c, 44100, 0);
	xmp_get_module_info(c, &info);

	read_file_to_memory("data/beep.raw", &buf, &size);
	fail_unless(buf != NULL, "can't open raw data file");

	pcm16 = (int16 *)info.mod->xxs[0].data;
	if (is_big_endian()) { /* convert little-endian to host-endian */
		convert_endian((unsigned char *)buf, size / 2);
	}

	for (i = 0; i < (9376 / 2); i++) {
		if (pcm16[i] != ((int16 *)buf)[i])
			fail_unless(abs(pcm16[i] - ((int16 *)buf)[i]) <= 1, "data error");
	}

	xmp_release_module(c);
	xmp_free_context(c);
	free(buf);
}
END_TEST
