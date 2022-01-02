#include "test.h"

TEST(test_write_file_8bit)
{
	FILE *f;
	int x;

	f = fopen("write_test", "wb");
	fail_unless(f != NULL, "can't open data file");

	write8(f, 0x66);
	fclose(f);

	f = fopen("write_test", "rb");
	x = read8(f, NULL);
	fclose(f);

	fail_unless(x == 0x66, "read error");
	unlink("write_test");
}
END_TEST
