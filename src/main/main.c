/* A simple frontend for xmp */

#include <stdio.h>
#include <stdlib.h>
#include <xmp.h>
//#include "sound.h"

static void shuffle(int argc, char **argv)
{
	int i, j;
	char *x;

	for (i = 1; i < argc; i++) {
		j = 1 + rand() % (argc - 1);
		x = argv[i];
		argv[i] = argv[j];
		argv[j] = x;
	}
}

int main(int argc, char **argv)
{
	static xmp_context ctx;
	static struct xmp_module_info mi;
	int i;
	int silent = 0;
	int optind = 1;

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's') {
		silent = 1;
		optind++;
	}

#if 0
	if (!silent && sound_init(44100, 2) < 0) {
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		exit(1);
	}
#endif

	ctx = xmp_create_context();

	for (i = optind; i < argc; i++) {
		printf("\nLoading %s... (%d of %d)\n",
                	argv[i], i - optind + 1, argc - optind);

		if (xmp_load_module(ctx, argv[i]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}

		if (xmp_player_start(ctx) == 0) {

			/* Show module data */

			xmp_player_get_info(ctx, &mi);

			info_mod(&mi);

			/* Play module */

			while (xmp_player_frame(ctx) == 0) {

				xmp_player_get_info(ctx, &mi);
				if (mi.loop_count > 0)
					break;

				info_frame(&mi);
#if 0
				if (!silent) {
					sound_play(mi.buffer,
						   mi.buffer_size);
				}
#endif

			}
			xmp_player_end(ctx);
		}

		xmp_release_module(ctx);
		printf("\n");
	}

	xmp_free_context(ctx);

#if 0
	if (!silent) {
		sound_deinit();
	}
#endif

	return 0;
}
