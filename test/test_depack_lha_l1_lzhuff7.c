#include "test.h"

int decrunch_lha(FILE *f, FILE *fo);


TEST(test_depack_lha_l1_lzhuff7)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/l1_lzhuff7", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_lha(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 8378, "decompression size error");

	ret = check_md5(TMP_FILE, "c993a848f57227660f8b10db1d4d874f");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
