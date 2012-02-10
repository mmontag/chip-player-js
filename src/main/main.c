/* A simple frontend for xmp */

#define HAVE_TERMIOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <xmp.h>
//#include "sound.h"
#include "common.h"

extern int optind;

static void cleanup()
{
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);

	sound_deinit();
	reset_tty();
	printf("\n");
	exit(EXIT_FAILURE);
}


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
	xmp_context ctx;
	struct xmp_module_info mi;
	struct options options;
	int i;
	int silent = 0;
	int first;
#ifndef WIN32
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	srand(tv.tv_usec);
#else
	srand(GetTickCount());
#endif

	memset(&options, 0, sizeof (struct options));
	get_options(argc, argv, &options);

	if (options.random) {
		shuffle(argc - optind + 1, &argv[optind - 1]);
	}

	if (!silent && sound_init(44100, 2) < 0) {
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGFPE, cleanup);
	signal(SIGSEGV, cleanup);
	signal(SIGQUIT, cleanup);

	set_tty();

	ctx = xmp_create_context();

	for (first = optind; optind < argc; optind++) {
		printf("\nLoading %s... (%d of %d)\n",
			argv[optind], optind - first + 1, argc - first);

		if (xmp_load_module(ctx, argv[optind]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}

		if (xmp_player_start(ctx, options.start, 44100, 0) == 0) {
			int new_mod = 1;

			/* Mute channels */

			for (i = 0; i < XMP_MAX_CHANNELS; i++) {
				xmp_channel_mute(ctx, i, options.mute[i]);
			}

			/* Show module data */

			xmp_player_get_info(ctx, &mi);

			info_mod(&mi);

			/* Play module */

			while (xmp_player_frame(ctx) == 0) {

				xmp_player_get_info(ctx, &mi);
				if (mi.loop_count > 0)
					break;

				info_frame(&mi, new_mod);
				if (!silent) {
					sound_play(mi.buffer, mi.buffer_size);
				}

				new_mod = 0;
				options.start = 0;

			}
			xmp_player_end(ctx);
		}

		xmp_release_module(ctx);
		printf("\n");
	}
	xmp_free_context(ctx);

	reset_tty();

	if (!silent) {
		sound_deinit();
	}

	exit(EXIT_SUCCESS);
}
