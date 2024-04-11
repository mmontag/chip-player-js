#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../include/xmp.h"

#ifdef __cplusplus
extern "C"
#endif
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	xmp_context opaque = xmp_create_context();
	FILE *f;

	/* Fuzz loaders. */
	if (xmp_load_module_from_memory(opaque, data, size) == 0)
	{
		/* Fuzz playback. */
		struct xmp_module_info info;
		int interp, mono, i;

		/* Derive config from the MD5 for now... :( */
		xmp_get_module_info(opaque, &info);
		interp = info.md5[7] * 3U / 256;
		mono = (info.md5[3] & 1) ^ (info.md5[14] >> 7);

		switch (interp) {
		case 0:
			interp = XMP_INTERP_NEAREST;
			break;
		case 1:
			interp = XMP_INTERP_LINEAR;
			break;
		default:
			interp = XMP_INTERP_SPLINE;
			break;
		}
		xmp_start_player(opaque, XMP_MIN_SRATE, mono ? XMP_FORMAT_MONO : 0);
		xmp_set_player(opaque, XMP_PLAYER_INTERP, interp);

		/* TODO Saga Musix also recommends performing different types of seeking. */
		for (i = 0; i < 64; i++)
			xmp_play_frame(opaque);

		xmp_release_module(opaque);
	}

	/* Fuzz depackers. */
	f = fmemopen((void *)data, size, "rb");
	if (f != NULL)
	{
		struct xmp_test_info info;
		xmp_test_module_from_file(f, &info);
		fclose(f);
	}

	xmp_free_context(opaque);
	return 0;
}
