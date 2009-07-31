
#include <stdio.h>
#include <stdlib.h>
#include "xmp.h"

extern struct xmp_drv_info drv_wav;


int main(int argc, char **argv)
{
	xmp_context ctx1, ctx2;
	struct xmp_module_info mi1, mi2;
	struct xmp_options *opt1, *opt2;
	int res1, res2;

	setbuf(stdout, NULL);

	xmp_drv_register(&drv_wav);

	ctx1 = xmp_create_context();
	ctx2 = xmp_create_context();

	opt1 = xmp_get_options(ctx1);
	opt1->drv_id = "wav";
	opt1->outfile = "w1.wav";
	xmp_init(ctx1, argc, argv);
	if (xmp_open_audio(ctx1) < 0) {
		fprintf(stderr, "%s: can't open audio device", argv[0]);
		exit(1);
	}

	opt2 = xmp_get_options(ctx2);
	opt2->drv_id = "wav";
	opt2->outfile = "w2.wav";
	xmp_init(ctx2, argc, argv);
	if (xmp_open_audio(ctx2) < 0) {
		fprintf(stderr, "%s: can't open audio device", argv[0]);
		exit(1);
	}

	if (xmp_load_module(ctx1, argv[1]) < 0) {
		fprintf(stderr, "%s: error loading %s\n", argv[0], argv[1]);
		exit(1);
	}

	if (xmp_load_module(ctx2, argv[2]) < 0) {
		fprintf(stderr, "%s: error loading %s\n", argv[0], argv[2]);
		exit(1);
	}

	xmp_get_module_info(ctx1, &mi1);
	xmp_get_module_info(ctx2, &mi2);

	printf("1: %s (%s)\n", mi1.name, mi1.type);
	printf("2: %s (%s)\n", mi2.name, mi2.type);

	xmp_player_start(ctx1);
	xmp_player_start(ctx2);
	res1 = res2 = 0;

	while (res1 == 0 || res2 == 0) {
		if (res1 == 0) {
			res1 = xmp_player_frame(ctx1);
			xmp_play_buffer(ctx1);
		}
		if (res2 == 0) {
			res2 = xmp_player_frame(ctx2);
			xmp_play_buffer(ctx2);
		}
	}

	xmp_player_end(ctx1);
	xmp_player_end(ctx2);

	xmp_release_module(ctx1);
	xmp_release_module(ctx2);

	xmp_close_audio(ctx1);
	xmp_close_audio(ctx2);

	xmp_free_context(ctx1);
	xmp_free_context(ctx2);

	return 0;
}
