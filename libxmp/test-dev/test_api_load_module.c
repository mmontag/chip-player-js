#include <errno.h>
#include "test.h"

TEST(test_api_load_module)
{
	xmp_context ctx;
	int state, ret;
	FILE *f;

	ctx = xmp_create_context();

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* module doesn't exist */
	ret = xmp_load_module(ctx, "/doesntexist");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "module doesn't exist");
	fail_unless(xmp_syserrno() == ENOENT, "errno code");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* is directory */
	ret = xmp_load_module(ctx, "data");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "try to load directory");
	fail_unless(xmp_syserrno() == EISDIR, "errno code");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

#if 0
	/* no read permission */
	creat(".read_test", 0111);
	ret = xmp_load_module(ctx, ".read_test");
	fail_unless(ret == -XMP_ERROR_SYSTEM, "no read permission");
	fail_unless(xmp_syserrno() == EACCES, "errno code");
	unlink(".read_test");
#endif

	/* small file */
	// Crashes with MSVC.
	//creat(".read_test", 0644);
	f = fopen(".read_test", "wb");
	if (f)
		fclose(f);
	ret = xmp_load_module(ctx, ".read_test");
	fail_unless(ret == -XMP_ERROR_FORMAT, "small file");
	unlink(".read_test");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* invalid format */
	ret = xmp_load_module(ctx, "Makefile.in");
	fail_unless(ret == -XMP_ERROR_FORMAT, "invalid format");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* corrupted compressed file */
	ret = xmp_load_module(ctx, "data/corrupted.gz");
	fail_unless(ret == -XMP_ERROR_DEPACK, "depack error fail");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* corrupted module */
	ret = xmp_load_module(ctx, "data/adlib.s3m-corrupted");
	fail_unless(ret == -XMP_ERROR_LOAD, "depack error fail");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* valid file */
	ret = xmp_load_module(ctx, "data/test.it");
	fail_unless(ret == 0, "load file");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_LOADED, "state error");

	/* load again without unloading */
	ret = xmp_load_module(ctx, "data/test.xm");
	fail_unless(ret == 0, "reload file");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_LOADED, "state error");

	/* valid file (<256 bytes) */
	ret = xmp_load_module(ctx, "data/small.gdm");
	fail_unless(ret == 0, "load file <256 bytes");

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_LOADED, "state error");

	/* unload */
	xmp_release_module(ctx);

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	/* double release should be safe. */
	xmp_release_module(ctx);

	state = xmp_get_player(ctx, XMP_PLAYER_STATE);
	fail_unless(state == XMP_STATE_UNLOADED, "state error");

	xmp_free_context(ctx);
}
END_TEST
