#include "test.h"
#include "unxz/xz.h"

int decrunch_xz(FILE *f, FILE *fo);


TEST(test_depack_xz)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	xz_crc32_init();

	f = fopen("data/xzdata", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_xz(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 17968, "decompression size error");

	ret = check_md5(TMP_FILE, "0350baf25b96d6d125f537c63f03e3db");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
