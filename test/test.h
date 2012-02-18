#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xmp.h>
#include "md5.h"

#define TMP_FILE ".test"

#define TEST_FUNC(x) int _test_func_##x(void)

#define TEST(x) TEST_FUNC(x) {

#define END_TEST \
	return 0; }

#define fail_unless(x, y) do { \
	if (!(x)) { printf("%s", y); return -1; } \
} while (0)


int check_md5(char *, char *);

TEST_FUNC(test_pp);
TEST_FUNC(test_sqsh);
TEST_FUNC(test_s404);
TEST_FUNC(test_mmcmp);
