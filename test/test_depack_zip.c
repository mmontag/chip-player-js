#include "test.h"

int decrunch_zip(FILE *f, FILE *fo);


TEST(test_depack_zip)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/zipdata1", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_zip(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 13845, "decompression size error");

	ret = check_md5(TMP_FILE, "a0b5bedbe15e1053ba6bd5645898e6c5");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
