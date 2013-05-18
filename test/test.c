#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmp.h"

int main()
{
	FILE *f;
	int ret;
	unsigned char *buf;
	xmp_context c;
	struct xmp_frame_info info;
	long time;

	c = xmp_create_context();
	if (c == NULL)
		goto err;

	ret = xmp_load_module(c, "test.itz");
	if (ret != 0) {
		printf("can't load module\n");
		goto err;
	}

	xmp_get_frame_info(c, &info);
	if (info.total_time != 4800) {
		printf("estimated replay time error\n");
		goto err;
	}

	xmp_start_player(c, 22050, 0);
	xmp_set_player(c, XMP_PLAYER_MIX, 100);
	xmp_set_player(c, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);

	f = fopen("test.raw", "rb");
	if (f == NULL) {
		printf("can't open raw data file\n");
		goto err;
	}

	buf = malloc(XMP_MAX_FRAMESIZE);
	if (buf == NULL) {
		printf("can't alloc raw buffer\n");
		goto err;
	}

	printf("Testing ");
	fflush(stdout);
	time = 0;

	while (1) {
		xmp_play_frame(c);
		xmp_get_frame_info(c, &info);
		if (info.loop_count > 0)
			break;

		time += info.frame_time;

		ret = fread(buf, 1, info.buffer_size, f);
		if (ret != info.buffer_size) {
			printf("error reading raw buffer\n");
			goto err;
		}

		if (memcmp(buf, info.buffer, info.buffer_size) != 0) {
			printf("replay error\n");
			goto err;
		}

		printf(".");
		fflush(stdout);
	}

	if (time / 1000 != info.total_time) {
		printf("replay time error\n");
		goto err;
	}

	fclose(f);

	printf(" pass\n");

	exit(0);

    err:
	printf(" fail\n");
	exit(1);
}
