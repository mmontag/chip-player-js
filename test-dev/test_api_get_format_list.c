#include "test.h"

TEST(test_api_get_format_list)
{
	const char *const *list;
	int i;

	list = xmp_get_format_list();
	fail_unless(list != 0, "returned NULL");

	for (i = 0; list[i] != NULL; i++) {
		fail_unless(*list[i] != 0, "empty format name");
	}

	fail_unless(i == 93, "wrong number of formats");
}
END_TEST
