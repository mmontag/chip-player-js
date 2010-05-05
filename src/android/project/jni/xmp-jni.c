/* Simple interface adaptor for jni */

#include <stdlib.h>
#include "xmp.h"

extern struct xmp_drv_info drv_smix;

static xmp_context ctx;
static struct xmp_options *opt;
static struct xmp_module_info mi;

int j_init()
{
	xmp_drv_register(&drv_smix);
	ctx = xmp_create_context();
	xmp_init(ctx, 0, NULL);
	opt = xmp_get_options(ctx);
	opt->verbosity = 0;

	if (xmp_open_audio(ctx) < 0) {
		xmp_deinit(ctx);
		xmp_free_context(ctx);
		return -1;
	}

	return 0;
}

int j_deinit()
{
	xmp_close_audio(ctx);
	xmp_deinit(ctx);
	xmp_free_context(ctx);
}

int j_load(char *name)
{
	if (xmp_load_module(ctx, name) < 0)
		return -1;
	xmp_get_module_info(ctx, &mi);
}

int j_release()
{
	xmp_release_module(ctx);
}

int j_play()
{
	xmp_player_start(ctx);
}

int j_stop()
{
	xmp_player_end(ctx);
}

int j_play_frame()
{
	xmp_player_frame(ctx);
}

void j_get_buffer(void **data, int *size)
{
	xmp_get_buffer(ctx, data, size);
}

