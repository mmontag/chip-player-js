/* A simple frontend for xmp */

#include <stdio.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#include "xmp.h"

int main(int argc, char **argv)
{
	static xmp_context ctx;
	static struct xmp_module_info mi;
	int i;
	pa_simple *s;
	pa_sample_spec ss;
	int error;
	void *buffer;
	int size;

	setbuf(stdout, NULL);

	ss.format = PA_SAMPLE_S16NE;
	ss.channels = 2;
	ss.rate = 44100;

	s = pa_simple_new(NULL,	/* Use the default server */
		  "test",	/* Our application's name */
		  PA_STREAM_PLAYBACK, NULL,	/* Use the default device */
		  "Music",	/* Description of our stream */
		  &ss,		/* Our sample format */
		  NULL,		/* Use default channel map */
		  NULL,		/* Use default buffering attributes */
		  &error);	/* Ignore error code */

	if (s == 0) {
		fprintf(stderr, "pulseaudio error: %s\n", pa_strerror(error));
		return XMP_ERR_DINIT;
	}

	ctx = xmp_create_context();
	xmp_init(ctx, argc, argv);

	for (i = 1; i < argc; i++) {
		if (xmp_load_module(ctx, argv[i]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}
		xmp_player_start(ctx);

		xmp_player_get_info(ctx, &mi);
		printf("%s (%s)\n", mi.mod->name, mi.mod->type);

		while (xmp_player_frame(ctx) == 0) {
			xmp_get_buffer(ctx, &buffer, &size);
			pa_simple_write(s, buffer, size, &error);
		}
		xmp_player_end(ctx);

		xmp_release_module(ctx);
		printf("\n");
	}

	xmp_free_context(ctx);

	return 0;
}
