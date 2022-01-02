#include "test.h"

/* Test that certain flow control effects do not continue after
 * usage of xmp_set_position, xmp_next_position, and xmp_prev_position.
 * These effects include jump, break, loop, and pattern delay.
 */

enum set_position_midfx_fns
{
	FN_SET_POSITION,
	FN_NEXT_POSITION,
	FN_PREV_POSITION
};

struct set_position_midfx_data
{
	int row;
	int frame;
	int note;
	int volume;
};

static void do_set_position_midfx(xmp_context opaque,
	const char *filename, enum set_position_midfx_fns fn_to_use,
	int start_order, int frame_to_set_position, int target_order)
{
	/**
	 * The target order for each module should be the same:
	 *
	 * - 2 ticks of C-5.
	 * - 2 ticks of C-6.
	 * - 4 ticks of F-5.
	 * - note cut.
	 *
	 * If it doesn't play exactly like that then some of the player flow
	 * state persisted after the position change.
	 */
	struct set_position_midfx_data data[] =
	{
		{ 0, 0, 60, 64 },
		{ 0, 1, 60, 64 },
		{ 1, 0, 72, 64 },
		{ 1, 1, 72, 64 },
		{ 2, 0, 65, 64 },
		{ 2, 1, 65, 64 },
		{ 3, 0, 65, 64 },
		{ 3, 1, 65, 64 },
		{ 4, 0, 65,  0 },
		{ 0, 0,  0,  0 },
	};
	struct xmp_frame_info info;
	int ret, i;

	D_(D_INFO "%s, test # %d", filename, fn_to_use + 1);

	ret = xmp_load_module(opaque, filename);
	fail_unless(ret == 0, "load module");

	xmp_start_player(opaque, 44100, 0);

	ret = xmp_set_position(opaque, start_order);
	ret = ret > 0 ? ret : 0; /* Normalize -1 for start order to 0 for check. */
	fail_unless(ret == start_order, "initial set position");

	for (i = 0; i < frame_to_set_position; i++) {
		xmp_play_frame(opaque);
	}

	switch (fn_to_use) {
	case FN_SET_POSITION:
		ret = xmp_set_position(opaque, target_order);
		fail_unless(ret == target_order, "xmp_set_position");
		break;
	case FN_NEXT_POSITION:
		ret = xmp_next_position(opaque);
		fail_unless(ret == target_order, "xmp_next_position");
		break;
	case FN_PREV_POSITION:
		ret = xmp_prev_position(opaque);
		fail_unless(ret == target_order, "xmp_prev_position");
		break;
	default:
		fail_unless(0, "invalid set position function");
	}

	for (i = 0; data[i].note; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &info);

		D_(D_INFO "%d %d %d %d %d", info.pos, info.row, info.frame,
			info.channel_info[0].note, info.channel_info[0].volume);
		fail_unless(target_order == info.pos, "pos mismatch");
		fail_unless(data[i].row == info.row, "row mismatch");
		fail_unless(data[i].frame == info.frame, "frame mismatch");
		fail_unless(data[i].note == info.channel_info[0].note, "note mismatch");
		fail_unless(data[i].volume == info.channel_info[0].volume, "volume mismatch");
	}

	xmp_release_module(opaque);
}

TEST(test_api_set_position_midfx)
{
	xmp_context opaque;

	opaque = xmp_create_context();

	do_set_position_midfx(opaque, "data/set_position_mid_jump.it", FN_SET_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_jump.it", FN_NEXT_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_jump.it", FN_PREV_POSITION, 4, 3, 3);

	do_set_position_midfx(opaque, "data/set_position_mid_break.it", FN_SET_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_break.it", FN_NEXT_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_break.it", FN_PREV_POSITION, 2, 3, 1);

	do_set_position_midfx(opaque, "data/set_position_mid_loop.it", FN_SET_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_loop.it", FN_NEXT_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_loop.it", FN_PREV_POSITION, 2, 3, 1);

	/* IT pattern delay uses f->rowdelay*/
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.it", FN_SET_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.it", FN_NEXT_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.it", FN_PREV_POSITION, 2, 3, 1);

	/* Non-IT pattern delay uses f->delay */
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.xm", FN_SET_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.xm", FN_NEXT_POSITION, 0, 3, 1);
	do_set_position_midfx(opaque, "data/set_position_mid_pattdelay.xm", FN_PREV_POSITION, 2, 3, 1);

	xmp_free_context(opaque);
}
END_TEST
