#include "test.h"

int decrunch_s404(FILE *f, FILE *fo);


TEST(test_depack_s404)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/aon.wingsofdeath1.stc", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_s404(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 31360, "decompression size error");

	ret = check_md5(TMP_FILE, "ab45d5c5427617d6ee6e4260710edaf1");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
