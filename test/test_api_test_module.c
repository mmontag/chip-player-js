#include "test.h"
#include <errno.h>

TEST(test_api_test_module)
{
	struct xmp_test_info tinfo;
	int ret, err;

	/* directory */
	ret = xmp_test_module("data", &tinfo);
	err = errno;
	fail_unless(ret == -XMP_ERROR_SYSTEM, "directory fail");
	fail_unless(err == EISDIR, "errno test module fail");

	/* nonexistent file */
	ret = xmp_test_module("foo--bar", &tinfo);
	err = errno;
	fail_unless(ret == -XMP_ERROR_SYSTEM, "nonexistent file fail");
	fail_unless(err == ENOENT, "errno test module fail");

	/* unsupported format */
	ret = xmp_test_module("data/storlek_01.data", &tinfo);
	fail_unless(ret == -XMP_ERROR_FORMAT, "unsupported format fail");

	/* file too small */
	ret = xmp_test_module("data/sample-16bit.raw", &tinfo);
	fail_unless(ret == -XMP_ERROR_FORMAT, "small file fail");

	/* null info */
	ret = xmp_test_module("data/storlek_05.it", NULL);
	fail_unless(ret == 0, "null info test fail");

	/* XM */
	ret = xmp_test_module("data/test.mmcmp", &tinfo);
	fail_unless(ret == 0, "XM test module fail");
	fail_unless(strcmp(tinfo.name, "playboy") == 0, "XM module name fail");
	fail_unless(strcmp(tinfo.type, "Fast Tracker II (XM)") == 0, "XM module type fail");

	/* MOD */
	ret = xmp_test_module("data/ode2ptk.mod", &tinfo);
	fail_unless(ret == 0, "MOD test module fail");
	fail_unless(strcmp(tinfo.name, "Ode to Protracker") == 0, "MOD module name fail");
	fail_unless(strcmp(tinfo.type, "Protracker (MOD)") == 0, "MOD module type fail");

	/* IT */
	ret = xmp_test_module("data/storlek_01.it", &tinfo);
	fail_unless(ret == 0, "IT test module fail");
	fail_unless(strcmp(tinfo.name, "arpeggio + pitch slide") == 0, "IT module name fail");
	fail_unless(strcmp(tinfo.type, "Impulse Tracker (IT)") == 0, "IT module type fail");

	/* S3M */
	ret = xmp_test_module("data/xzdata", &tinfo);
	fail_unless(ret == 0, "S3M test module fail");
	fail_unless(strcmp(tinfo.name, "Inspiration") == 0, "S3M module name fail");
	fail_unless(strcmp(tinfo.type, "Scream Tracker 3 (S3M)") == 0, "S3M module type fail");

	/* Prowizard */
	ret = xmp_test_module("data/PRU1.intro-electro", &tinfo);
	fail_unless(ret == 0, "Prowizard test module fail");
	fail_unless(strcmp(tinfo.name, "intro-electro") == 0, "Prowizard module name fail");
	fail_unless(strcmp(tinfo.type, "Prorunner 1.0") == 0, "Prowizard module type fail");
}
END_TEST
