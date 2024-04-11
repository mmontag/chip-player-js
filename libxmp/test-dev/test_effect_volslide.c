#include "test.h"
#include "../src/effects.h"


static int vals[] = {
	64, 62, 60, 58,		/* down 2 */
	58, 56, 54, 52,		/* memory */
	52, 53, 54, 55,		/* up 1 */
	55, 56, 57, 58,		/* memory */
	63, 63, 63, 63,		/* set 63 */
	63, 64, 64, 64,		/* up 1 */
	1, 1, 1, 1,		/* set 1 */
	1, 0, 0, 0,		/* down 1 */
	10, 10, 10, 10,		/* set 10 */
	8, 8, 8, 8,		/* fine slide down 2 */
	9, 9, 9, 9,		/* fine slide up 1 */
	10, 10, 10, 10,		/* memory */
	32, 32, 32, 32,		/* set 32 */
	32, 30, 28, 26,		/* down 2 */
	26, 29, 32, 35,		/* up 3 */
	35, 38, 41, 44		/* memory */
};

TEST(test_effect_volslide)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info info;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);

	/* slide down & up with memory */
	new_event(ctx, 0, 0, 0, 49, 1, 0, FX_VOLSLIDE_DN, 0x02, FX_SPEED, 4);
	new_event(ctx, 0, 1, 0, 0, 0, 0, FX_VOLSLIDE_DN, 0x00, 0, 0);
	new_event(ctx, 0, 2, 0, 0, 0, 0, FX_VOLSLIDE_UP, 0x01, 0, 0);
	new_event(ctx, 0, 3, 0, 0, 0, 0, FX_VOLSLIDE_UP, 0x00, 0, 0);

	/* limits */
	new_event(ctx, 0, 4, 0, 0, 0, 0, FX_VOLSET, 0x3f, 0, 0);
	new_event(ctx, 0, 5, 0, 0, 0, 0, FX_VOLSLIDE_UP, 0x01, 0, 0);
	new_event(ctx, 0, 6, 0, 0, 0, 0, FX_VOLSET, 0x01, 0, 0);
	new_event(ctx, 0, 7, 0, 0, 0, 0, FX_VOLSLIDE_DN, 0x01, 0, 0);

        /* fine effects */
	new_event(ctx, 0, 8, 0, 0, 0, 0, FX_VOLSET, 0x0a, 0, 0);
	new_event(ctx, 0, 9, 0, 0, 0, 0, FX_F_VSLIDE, 0x02, 0, 0);
	new_event(ctx, 0, 10, 0, 0, 0, 0, FX_F_VSLIDE, 0x10, 0, 0);
	new_event(ctx, 0, 11, 0, 0, 0, 0, FX_F_VSLIDE, 0x00, 0, 0);

	/* secondary volslide */
	new_event(ctx, 0, 12, 0, 0, 0, 0, FX_VOLSET, 0x20, 0, 0);
	new_event(ctx, 0, 13, 0, 0, 0, 0, FX_VOLSLIDE_2, 0x02, 0, 0);
	new_event(ctx, 0, 14, 0, 0, 0, 0, FX_VOLSLIDE_2, 0x30, 0, 0);
	new_event(ctx, 0, 15, 0, 0, 0, 0, FX_VOLSLIDE_2, 0x00, 0, 0);

	/* play it */
	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 16 * 4; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);
		fail_unless(info.channel_info[0].volume == vals[i], "volume slide error");

	}

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
