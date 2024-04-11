#include "test.h"

/* This input caused UMRs in test_module due to the ABK test
 * function not returning the module title.
 */

TEST(test_fuzzer_abk_test_title)
{
	struct xmp_test_info info;
	int ret;

	ret = xmp_test_module("data/f/test_abk_title.abk", &info);
	fail_unless(ret == 0, "module test");

	ret = strcmp(info.name, "Test Song Name");
	fail_unless(ret == 0, "module name mismatch");
	ret = strcmp(info.type, "AMOS Music Bank");
	fail_unless(ret == 0, "module type mismatch");
}
END_TEST
