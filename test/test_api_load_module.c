#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "test.h"

TEST(test_api_load_module)
{
	xmp_context ctx;
	int ret;

	ctx = xmp_create_context();

	/* module doesn't exist */
	ret = xmp_load_module(ctx, "/doesntexist");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "module doesn't exist");
	fail_unless(errno == ENOENT, "errno code");

	/* is directory */
	ret = xmp_load_module(ctx, "/");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "try to load directory");
	fail_unless(errno == EISDIR, "errno code");

	/* no read permission */
	creat(".read_test", 0111);
	ret = xmp_load_module(ctx, ".read_test");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "no read permission");
	fail_unless(errno == EACCES, "errno code");
	unlink(".read_test");

	/* small file */
	creat(".read_test", 0644);
	ret = xmp_load_module(ctx, ".read_test");
	fail_unless(ret == -XMP_ERROR_FORMAT, "small file");
	unlink(".read_test");

	/* invalid format */
	ret = xmp_load_module(ctx, "Makefile");
	fail_unless(ret == -XMP_ERROR_FORMAT, "invalid format");

	/* valid file */
	ret = xmp_load_module(ctx, "data/test.xm");
	fail_unless(ret == 0, "load file");

}
END_TEST
