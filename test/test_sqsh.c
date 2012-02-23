#include "test.h"

int decrunch_sqsh(FILE *f, FILE *fo);


TEST(test_sqsh)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/sqsh_random1", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_sqsh(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 16384, "decompression size error");

	ret = check_md5(TMP_FILE, "5f7c2705f9257eb65e38edbc08028375");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
