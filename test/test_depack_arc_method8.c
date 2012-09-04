#include "test.h"

int decrunch_arc(FILE *f, FILE *fo);


TEST(test_depack_arc_method8)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/arc-method8-rle", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_arc(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 108648, "decompression size error");

	ret = check_md5(TMP_FILE, "64d67d1d5d123c6542a8099255ad8ca2");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
