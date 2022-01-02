#include "test.h"

static int test_module_from_file_helper(const char* filename, struct xmp_test_info *tinfo)
{
	FILE *f;
	int ret;

	f = fopen(filename, "rb");
	fail_unless(f != NULL, "open file");
	ret = xmp_test_module_from_file(f, tinfo);
	fclose(f);
	return ret;
}

TEST(test_api_test_module_from_file)
{
	struct xmp_test_info tinfo;
	int ret;

	/* unsupported format */
	ret = test_module_from_file_helper("data/storlek_01.data", &tinfo);
	fail_unless(ret == -XMP_ERROR_FORMAT, "unsupported format fail");

	/* corrupted compressed file */
	ret = test_module_from_file_helper("data/corrupted.gz", &tinfo);
	fail_unless(ret == -XMP_ERROR_DEPACK, "depack error fail");

	/* file too small */
	ret = test_module_from_file_helper("data/sample-16bit.raw", &tinfo);
	fail_unless(ret == -XMP_ERROR_FORMAT, "small file fail");

	/* null info */
	ret = test_module_from_file_helper("data/storlek_05.it", NULL);
	fail_unless(ret == 0, "null info test fail");

	/* XM */
	ret = test_module_from_file_helper("data/test.mmcmp", &tinfo);
	fail_unless(ret == 0, "XM test module fail");
	fail_unless(strcmp(tinfo.name, "playboy") == 0, "XM module name fail");
	fail_unless(strcmp(tinfo.type, "Fast Tracker II") == 0, "XM module type fail");

	/* MOD */
	ret = test_module_from_file_helper("data/ode2ptk.mod", &tinfo);
	fail_unless(ret == 0, "MOD test module fail");
	fail_unless(strcmp(tinfo.name, "Ode to Protracker") == 0, "MOD module name fail");
	fail_unless(strcmp(tinfo.type, "Amiga Protracker/Compatible") == 0, "MOD module type fail");

	/* IT */
	ret = test_module_from_file_helper("data/storlek_01.it", &tinfo);
	fail_unless(ret == 0, "IT test module fail");
	fail_unless(strcmp(tinfo.name, "arpeggio + pitch slide") == 0, "IT module name fail");
	fail_unless(strcmp(tinfo.type, "Impulse Tracker") == 0, "IT module type fail");

	/* S3M */
	ret = test_module_from_file_helper("data/xzdata", &tinfo);
	fail_unless(ret == 0, "S3M test module fail");
	fail_unless(strcmp(tinfo.name, "Inspiration") == 0, "S3M module name fail");
	fail_unless(strcmp(tinfo.type, "Scream Tracker 3") == 0, "S3M module type fail");

	/* Small file (<256 bytes) */
	ret = test_module_from_file_helper("data/small.gdm", &tinfo);
	fail_unless(ret == 0, "GDM (<256) test module fail");
	fail_unless(strcmp(tinfo.name, "") == 0, "GDM (<256) module name fail");
	fail_unless(strcmp(tinfo.type, "General Digital Music") == 0, "GDM (<256) module type fail");

	/* Prowizard */
	ret = test_module_from_file_helper("data/PRU1.intro-electro", &tinfo);
	fail_unless(ret == 0, "Prowizard test module fail");
	fail_unless(strcmp(tinfo.name, "intro-electro") == 0, "Prowizard module name fail");
	fail_unless(strcmp(tinfo.type, "Prorunner 1.0") == 0, "Prowizard module type fail");
}
END_TEST
