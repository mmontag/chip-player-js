#include "test.h"

int itsex_decompress16(FILE *module, void *dst, int len, char it215);


TEST(test_depack_it_sample_16bit)
{
	FILE *f, *fo;
	int ret;
	char dest[10000];

	f = fopen("data/it-sample-16bit.raw", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = itsex_decompress16(f, dest, 4646, 0);
	fail_unless(ret == 1, "decompression fail");
	fwrite(dest, 1, 9292, fo);

	fclose(fo);
	fclose(f);

	f = fopen(TMP_FILE, "rb");
	ret = check_md5(TMP_FILE, "1e2395653f9bd7838006572d8fcdb646");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
