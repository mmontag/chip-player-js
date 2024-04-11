#include "test.h"

static int test_module_from_memory_helper(const char* filename, struct xmp_test_info *tinfo, char* buf)
{
	FILE *f;
	int ret;
	size_t bread;

	f = fopen(filename, "rb");
	fail_unless(f != NULL, "open file");
	bread = fread(buf, 1, 64 * 1024, f);
	fclose(f);
	ret = xmp_test_module_from_memory(buf, bread, tinfo);
	return ret;
}

TEST(test_api_test_module_from_memory)
{
	struct xmp_test_info tinfo;
	int ret;
	char* buf;

	/* Sufficient to hold all file buffers */
	buf = malloc(64 * 1024);
	fail_unless(buf != NULL, "allocation error");

	/* unsupported format */
	ret = test_module_from_memory_helper("data/storlek_01.data", &tinfo, buf);
	fail_unless(ret == -XMP_ERROR_FORMAT, "unsupported format fail");

	/* file too small */
	ret = test_module_from_memory_helper("data/sample-16bit.raw", &tinfo, buf);
	fail_unless(ret == -XMP_ERROR_FORMAT, "small file fail");

	/* null info */
	ret = test_module_from_memory_helper("data/storlek_05.it", NULL, buf);
	fail_unless(ret == 0, "null info test fail");

	/* XM */
	ret = test_module_from_memory_helper("data/xm_portamento_target.xm", &tinfo, buf);
	fail_unless(ret == 0, "XM test module fail");
	fail_unless(strcmp(tinfo.type, "Fast Tracker II") == 0, "XM module type fail");

	/* MOD */
	ret = test_module_from_memory_helper("data/ode2ptk.mod", &tinfo, buf);
	fail_unless(ret == 0, "MOD test module fail");
	fail_unless(strcmp(tinfo.name, "Ode to Protracker") == 0, "MOD module name fail");
	fail_unless(strcmp(tinfo.type, "Amiga Protracker/Compatible") == 0, "MOD module type fail");

	/* IT */
	ret = test_module_from_memory_helper("data/storlek_01.it", &tinfo, buf);
	fail_unless(ret == 0, "IT test module fail");
	fail_unless(strcmp(tinfo.name, "arpeggio + pitch slide") == 0, "IT module name fail");
	fail_unless(strcmp(tinfo.type, "Impulse Tracker") == 0, "IT module type fail");

	/* Small file (<256 bytes) */
	ret = test_module_from_memory_helper("data/small.gdm", &tinfo, buf);
	fail_unless(ret == 0, "GDM (<256) test module fail");
	fail_unless(strcmp(tinfo.name, "") == 0, "GDM (<256) module name fail");
	fail_unless(strcmp(tinfo.type, "General Digital Music") == 0, "GDM (<256) module type fail");

	/* S3M (no unpacker for memory) */
	ret = test_module_from_memory_helper("data/xzdata", &tinfo, buf);
	fail_unless(ret == -XMP_ERROR_FORMAT, "S3M test module compressed fail");

	/* Prowizard */
	ret = test_module_from_memory_helper("data/m/PRU1.crack the eggshell!", &tinfo, buf);
	fail_unless(ret == 0, "Prowizard test module fail");
	fail_unless(strcmp(tinfo.name, "crack the eggshell!") == 0, "Prowizard module name fail");
	fail_unless(strcmp(tinfo.type, "Prorunner 1.0") == 0, "Prowizard module type fail");

	free(buf);
}
END_TEST
