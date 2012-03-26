#include "test.h"

int decrunch_lzx(FILE *f, FILE *fo);


TEST(test_depack_lzx)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/lzx_data", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_lzx(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 46259, "decompression size error");

	ret = check_md5(TMP_FILE, "8f67503c6b9084422caefe1a50090d88");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
