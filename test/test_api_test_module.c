#include "test.h"
#include <errno.h>

TEST(test_api_test_module)
{
	struct xmp_test_info tinfo;
	int ret, err;

	ret = xmp_test_module("data", &tinfo);
	err = errno;
	fail_unless(ret == -XMP_ERROR_SYSTEM, "dir error test module fail");
	fail_unless(err == EISDIR, "errno test module fail");

	ret = xmp_test_module("data/test.mmcmp", &tinfo);
	fail_unless(ret == 0, "XM test module fail");
	fail_unless(strcmp(tinfo.name, "playboy") == 0, "XM module name fail");
	fail_unless(strcmp(tinfo.type, "Fast Tracker II (XM)") == 0, "XM module type fail");

	ret = xmp_test_module("data/ode2ptk.mod", &tinfo);
	fail_unless(ret == 0, "MOD test module fail");
	fail_unless(strcmp(tinfo.name, "Ode to Protracker") == 0, "MOD module name fail");
	fail_unless(strcmp(tinfo.type, "Protracker (MOD)") == 0, "MOD module type fail");

	ret = xmp_test_module("data/storlek_01.it", &tinfo);
	fail_unless(ret == 0, "IT test module fail");
	fail_unless(strcmp(tinfo.name, "arpeggio + pitch slide") == 0, "IT module name fail");
	fail_unless(strcmp(tinfo.type, "Impulse Tracker (IT)") == 0, "IT module type fail");

	ret = xmp_test_module("data/xzdata", &tinfo);
	fail_unless(ret == 0, "S3M test module fail");
	fail_unless(strcmp(tinfo.name, "Inspiration") == 0, "S3M module name fail");
	fail_unless(strcmp(tinfo.type, "Scream Tracker 3 (S3M)") == 0, "S3M module type fail");

}
END_TEST
