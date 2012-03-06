#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	if (!(x)) { printf("%d: %s: ", __LINE__, y); return -1; } \
} while (0)


int check_md5(char *, char *);

#define declare_test(x) TEST_FUNC(x)
#include "all_tests.c"
#undef declare_test
