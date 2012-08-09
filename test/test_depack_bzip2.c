#include "test.h"

int decrunch_bzip2(FILE *f, FILE *fo);


TEST(test_depack_bzip2)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/bzip2data", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_bzip2(f, fo);
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
