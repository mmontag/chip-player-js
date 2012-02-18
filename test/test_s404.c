#include "test.h"

int decrunch_s404(FILE *f, FILE *fo);


TEST(test_s404)
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

	ret = check_md5(TMP_FILE, "ec481d16e96955fad40bd854f1b7efe5");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
