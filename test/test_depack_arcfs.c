#include "test.h"

int decrunch_arcfs(FILE *f, FILE *fo);


TEST(test_depack_arcfs)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/arcfsdata", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_arcfs(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 15346, "decompression size error");

	ret = check_md5(TMP_FILE, "1c41df267ebb8febe5e3d8a7e45bad61");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
