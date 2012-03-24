#include "test.h"

int decrunch_arc(FILE *f, FILE *fo);


TEST(test_depack_spark)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/038984", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_arc(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 32648, "decompression size error");

	ret = check_md5(TMP_FILE, "1aecc3cbfdae12a76000cd048ba8fcb3");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
