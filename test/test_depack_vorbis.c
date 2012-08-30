#include "test.h"
#include "../src/vorbis.h"


TEST(test_depack_vorbis)
{
	FILE *f;
	struct stat st;
	int i, ch, ret;
	int16 *buf, *pcm16;

	stat("data/beep.ogg", &st);

	f = fopen("data/beep.ogg", "rb");
	fail_unless(f != NULL, "can't open data file");

	buf = malloc(st.st_size);
	fail_unless(buf != NULL, "can't alloc buffer");

	fread(buf, 1, st.st_size, f);
	ret = stb_vorbis_decode_memory((uint8 *)buf, st.st_size, &ch, &pcm16);
	fail_unless(ret == 9376, "decompression fail");
	fail_unless(ch == 1, "invalid number of channels");
	fail_unless(pcm16 != NULL, "invalid buffer");

	free(buf);

	stat("data/beep.raw", &st);
	f = fopen("data/beep.raw", "rb");
	fail_unless(f != NULL, "can't open raw data file");

	buf = malloc(st.st_size);
	fail_unless(buf != NULL, "can't alloc raw buffer");
	fread(buf, 1, st.st_size, f);

	for (i = 0; i < 9376; i++) {
		if (pcm16[i] != buf[i])
			fail_unless(abs(pcm16[i] - buf[i]) >= 1, "data error");
	}

	fclose(f);

}
END_TEST
