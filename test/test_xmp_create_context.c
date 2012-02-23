#include "test.h"

TEST(test_xmp_create_context)
{
	xmp_context ctx;

	ctx = xmp_create_context();
	fail_unless(ctx != 0, "returned NULL");
}
END_TEST
