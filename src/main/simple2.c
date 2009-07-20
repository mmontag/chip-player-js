
#include <stdio.h>
#include <string.h>
#include "xmp.h"

static struct xmp_module_info mi;

#define BUFFER_SIZE 1024

struct buffer_data {
	int pos;
	int size;
	void *data;
};

struct buffer_data dst = { 0 , 0 };
struct buffer_data src = { 0 , 0 };


static void callback(void *b, int i)
{
	src.data = b;
	src.size = i;
}

static int fill_buffer(xmp_context ctx, void *buffer, int size)
{
	int want, have;

	dst.data = buffer;
	dst.size = size;

	want = dst.size - dst.pos;
	have = src.size - src.pos;

	if (have <= 0) {
		if (xmp_player_frame(ctx) < 0)
			return -1;
		have = src.size - src.pos;
	}

	if (want > have) {
		while (want > have) {
			memcpy(dst.data + dst.pos, src.data + src.pos, have);
			dst.pos += have;
			src.pos += have;

			want = dst.size - dst.pos;
			have = src.size - src.pos;

			if (have <= 0) {
				if (xmp_player_frame(ctx) < 0)
					return -1;
				have = src.size - src.pos;
			}
		}
	} else {
		memcpy(dst.data + dst.pos, src.data + src.pos, want);
		dst.pos += want;
		src.pos += want;
	}
}

int main(int argc, char **argv)
{
	int i;
	void *buffer;
	xmp_context ctx;

	buffer = malloc(BUFFER_SIZE);

	ctx = xmp_create_context();
	xmp_init(ctx, argc, argv);

	xmp_verbosity_level(ctx, 0);
	xmp_init_callback(ctx, callback);

	for (i = 1; i < argc; i++) {
		if (xmp_load_module(ctx, argv[i]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}
		xmp_get_module_info(ctx, &mi);
		printf("%s (%s)\n", mi.name, mi.type);

		xmp_player_start(ctx);
		while (fill_buffer(ctx, buffer, BUFFER_SIZE) == 0) {
			/* do something with the buffer */			
		}
		xmp_player_end(ctx);

		xmp_release_module(ctx);
	}

	xmp_free_context(ctx);

	return 0;
}
