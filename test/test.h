#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xmp.h>
#include "md5.h"

#include "../src/common.h"

#define TMP_FILE ".test"

#define TEST_FUNC(x) int _test_func_##x(void)

#undef TEST
#define TEST(x) TEST_FUNC(x) {

#define END_TEST \
	return 0; }

#define fail_unless(x, y) do { \
	if (!(x)) { printf("%d: %s: ", __LINE__, y); return -1; } \
} while (0)

int map_virt_channel(struct player_data *, int);
int play_frame(struct context_data *);


int check_md5(char *, char *);
void create_simple_module(struct context_data *, int, int);
void set_instrument_volume(struct context_data *, int, int, int);
void set_quirk(struct context_data *, int, int);
void new_event(struct context_data *, int, int, int, int, int, int, int, int, int, int);

#define declare_test(x) TEST_FUNC(x)
#include "all_tests.c"
#undef declare_test
