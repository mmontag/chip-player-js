#include "test.h"

int itsex_decompress8(FILE *module, void *dst, int len, char it215);


TEST(test_depack_it_sample_8bit)
{
	FILE *f, *fo;
	int ret;
	char dest[10000];

	f = fopen("data/it-sample-8bit.raw", "rb");
	fail_unless(f != NULL, "can't open data file");

	fo = fopen(TMP_FILE, "wb");
	fail_unless(fo != NULL, "can't open output file");

	ret = itsex_decompress8(f, dest, 4879, 0);
	fail_unless(ret == 0, "decompression fail");
	fwrite(dest, 1, 4879, fo);

	fclose(fo);
	fclose(f);
system("cp .test test8.raw");

	f = fopen(TMP_FILE, "rb");
	ret = check_md5(TMP_FILE, "299c9144ae2349b90b430aafde8d799a");
	fail_unless(ret == 0, "MD5 error");

	fclose(f);
}
END_TEST
