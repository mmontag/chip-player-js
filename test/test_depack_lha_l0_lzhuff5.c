#include "test.h"

int decrunch_lha(FILE *f, FILE *fo);


TEST(test_depack_lha_l0_lzhuff5)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/l0_lzhuff5", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_lha(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 46056, "decompression size error");

	ret = check_md5(TMP_FILE, "d62117b9d24b152b225bdb7be24d5c5c");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST


