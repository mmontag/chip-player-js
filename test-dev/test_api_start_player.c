#include "test.h"

TEST(test_api_start_player)
{
	xmp_context ctx;
	int ret;

	ctx = xmp_create_context();
	xmp_load_module(ctx, "data/ode2ptk.mod");

	fail_unless(XMP_MIN_SRATE == 4000, "min sample rate value");
	fail_unless(XMP_MAX_SRATE == 49170, "max sample rate value");

	/* valid sampling rates */
	ret = xmp_start_player(ctx, XMP_MIN_SRATE, 0);
	fail_unless(ret == 0, "min sample rate failed");
	xmp_end_player(ctx);

	ret = xmp_start_player(ctx, XMP_MAX_SRATE, 0);
	fail_unless(ret == 0, "max sample rate failed");
	xmp_end_player(ctx);

	/* invalid sampling rates */
	ret = xmp_start_player(ctx, XMP_MIN_SRATE - 1, 0);
	fail_unless(ret == -XMP_ERROR_INVALID, "min sample rate limit failed");
	xmp_end_player(ctx);

	ret = xmp_start_player(ctx, XMP_MAX_SRATE + 1, 0);
	fail_unless(ret == -XMP_ERROR_INVALID, "max sample rate limit failed");
	xmp_end_player(ctx);

	/* TODO: Should test internal and system errors too */

}
END_TEST
