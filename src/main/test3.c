
#include <stdio.h>
#include "xmp.h"

static xmp_context ctx1, ctx2;
static struct xmp_module_info mi1, mi2;

int main(int argc, char **argv)
{
	int i;

	setbuf(stdout, NULL);

	ctx1 = xmp_create_context();
	ctx2 = xmp_create_context();

	xmp_init(ctx1, argc, argv);
	xmp_init(ctx2, argc, argv);

	xmp_verbosity_level(ctx1, 0);
	if (xmp_open_audio(ctx1) < 0) {
		fprintf(stderr, "%s: can't open audio device", argv[0]);
		return -1;
	}

	for (i = 1; i < argc; i++) {
		if (xmp_load_module(ctx1, argv[i]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}
		xmp_get_module_info(ctx1, &mi);
		printf("%s (%s)\n", mi.name, mi.type);
		xmp_play_module(ctx1);
		xmp_release_module(ctx1);
		printf("\n");
	}
	xmp_close_audio(ctx1);
	xmp_free_context(ctx1);

	return 0;
}
