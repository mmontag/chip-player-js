#include "test.h"

int decrunch_compress(FILE *f, FILE *fo);


TEST(test_depack_compress)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/compressdata", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_compress(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 14176, "decompression size error");

	ret = check_md5(TMP_FILE, "37b8afe62ec42a47b1237b794193e785");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
