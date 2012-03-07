#include "test.h"

int decrunch_mmcmp(FILE *f, FILE *fo);


TEST(test_depack_mmcmp)
{
	FILE *f, *fo;
	int ret;
	struct stat st;

	f = fopen("data/mod.chip.mmcmp", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = decrunch_mmcmp(f, fo);
	fail_unless(ret == 0, "decompression fail");

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	fstat(fileno(f), &st);
	fail_unless(st.st_size == 10432, "decompression size error");

	ret = check_md5(TMP_FILE, "a94b233e18dcf26e0c6170c79ef90d16");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
