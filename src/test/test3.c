
#include <stdio.h>
#include <stdlib.h>
#include "xmp.h"

extern struct xmp_drv_info drv_smix;

/*
 * Test case for tunenet plugin crash
 *
 * from	Chris Young <cdyoung@ntlworld.com>
 * to	Claudio Matsuoka <cmatsuoka@gmail.com>
 * date	Sat, Aug 1, 2009 at 7:48 AM
 *
 * The general sequence called will be something like:
 *
 * initplayer
 * openplayer [file 1]
 * decodeframeplayer [file 1]
 * openplayer [file 2]
 * decodeframeplayer [file 2]
 * decodeframeplayer [file 1] <-- crash?
 * closeplayer [file 1]
 * decodeframeplayer [file 2] <-- crash?
 * closeplayer [file 2]
 * exitplayer
 */

int main(int argc, char **argv)
{
	xmp_context ctx1, ctx2;
	struct xmp_module_info mi1, mi2;
	int res1, res2;

	setbuf(stdout, NULL);

	xmp_drv_register(&drv_smix);

	/* open player 1 */
	ctx1 = xmp_create_context();
	xmp_init(ctx1, argc, argv);
	if (xmp_open_audio(ctx1) < 0) {
		fprintf(stderr, "%s: can't open audio device", argv[0]);
		exit(1);
	}
	if (xmp_load_module(ctx1, argv[1]) < 0) {
		fprintf(stderr, "%s: error loading %s\n", argv[0], argv[1]);
		exit(1);
	}
	xmp_get_module_info(ctx1, &mi1);
	printf("1: %s (%s)\n", mi1.name, mi1.type);

	/* play a bit of file 1 */
	xmp_player_start(ctx1);
	res1 = xmp_player_frame(ctx1);

	/* open player 2 */
	ctx2 = xmp_create_context();
	xmp_init(ctx2, argc, argv);
	if (xmp_open_audio(ctx2) < 0) {
		fprintf(stderr, "%s: can't open audio device", argv[0]);
		exit(1);
	}
	if (xmp_load_module(ctx2, argv[2]) < 0) {
		fprintf(stderr, "%s: error loading %s\n", argv[0], argv[2]);
		exit(1);
	}
	xmp_get_module_info(ctx2, &mi2);
	printf("2: %s (%s)\n", mi2.name, mi2.type);

	/* play file 2 */
	xmp_player_start(ctx2);
	res2 = xmp_player_frame(ctx2);

	/* play file 1 again */
	res1 = xmp_player_frame(ctx1);
	
	/* close player 1 */
	xmp_player_end(ctx1);

	/* play file 2 again */
	res1 = xmp_player_frame(ctx2);
	xmp_player_end(ctx2);

	xmp_release_module(ctx1);
	xmp_release_module(ctx2);

	xmp_close_audio(ctx1);
	xmp_close_audio(ctx2);

	xmp_free_context(ctx1);
	xmp_free_context(ctx2);

	return 0;
}
