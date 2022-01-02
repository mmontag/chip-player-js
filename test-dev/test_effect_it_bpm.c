#include "test.h"
#include "../src/effects.h"

static int vals[] = {
	80, 80, 80,	/* set tempo */
	80, 78, 76,	/* slide down 2 */
	76, 77, 78,	/* slide up 1 */
	78, 78, 78,	/* nothing */
	32, 32, 32,	/* set as 0x20 */
	32, 32, 32,	/* slide down */
	255, 255, 255,	/* set as 0xff */
	255, 255, 255,	/* slide up */

	125, 125, 125,	/* set as 0x7d */
	125, 127, 129,	/* slide up 2 */
	129, 131, 133,	/* slide up (T00) */
	133, 131, 129,	/* slide down 2 */
	129, 127, 125,	/* slide down (T00) */
	255, 255, 255,	/* set as 0xff */
	255, 253, 251,	/* slide down (T00) */
	251, 253, 255,	/* slide up 2 */
	252, 252, 252,	/* set as 0xfc */
	252, 254, 255	/* slide up (T00) */
};

TEST(test_effect_it_bpm)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	new_event(ctx, 0, 0, 0, 0, 0, 0, FX_IT_BPM, 0x50, FX_SPEED, 3);
	new_event(ctx, 0, 1, 0, 0, 0, 0, FX_IT_BPM, 0x02, 0, 0);
	new_event(ctx, 0, 2, 0, 0, 0, 0, FX_IT_BPM, 0x11, 0, 0);
	new_event(ctx, 0, 4, 0, 0, 0, 0, FX_IT_BPM, 0x20, 0, 0);
	new_event(ctx, 0, 5, 0, 0, 0, 0, FX_IT_BPM, 0x01, 0, 0);
	new_event(ctx, 0, 6, 0, 0, 0, 0, FX_IT_BPM, 0xff, 0, 0);
	new_event(ctx, 0, 7, 0, 0, 0, 0, FX_IT_BPM, 0x11, 0, 0);

	new_event(ctx, 0, 8, 0, 0, 0, 0, FX_IT_BPM, 0x7d, 0, 0);
	new_event(ctx, 0, 9, 0, 0, 0, 0, FX_IT_BPM, 0x12, 0, 0);
	new_event(ctx, 0,10, 0, 0, 0, 0, FX_IT_BPM, 0x00, 0, 0);
	new_event(ctx, 0,11, 0, 0, 0, 0, FX_IT_BPM, 0x02, 0, 0);
	new_event(ctx, 0,12, 0, 0, 0, 0, FX_IT_BPM, 0x00, 0, 0);
	new_event(ctx, 0,13, 0, 0, 0, 0, FX_IT_BPM, 0xff, 0, 0);
	new_event(ctx, 0,14, 0, 0, 0, 0, FX_IT_BPM, 0x00, 0, 0);
	new_event(ctx, 0,15, 0, 0, 0, 0, FX_IT_BPM, 0x12, 0, 0);
	new_event(ctx, 0,16, 0, 0, 0, 0, FX_IT_BPM, 0xfc, 0, 0);
	new_event(ctx, 0,17, 0, 0, 0, 0, FX_IT_BPM, 0x00, 0, 0);

	libxmp_scan_sequences(ctx);

	xmp_start_player(opaque, 44100, 0);

	xmp_get_frame_info(opaque, &info);
	fail_unless(info.total_time == 4628, "total time error");

	for (i = 0; i < 18 * 3; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(info.bpm == vals[i], "tempo setting error");
	}

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
