#include "test.h"

int decrunch_muse(FILE *f, FILE *fo);


TEST(test_depack_j2b)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/j2b_muse_data", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_muse(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 14448, "decompression size error");

	ret = check_md5(TMP_FILE, "166d2a8f5e4fbb466fe15d820ca59265");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
