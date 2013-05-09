#include "test.h"


TEST(test_player_virtual_channel)
{
	xmp_context opaque;
	struct xmp_frame_info fi;
	struct context_data *ctx;
	int i, j, used;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);
	set_instrument_volume(ctx, 0, 0, 22);
	set_instrument_volume(ctx, 1, 0, 33);
	set_instrument_nna(ctx, 0, 0, XMP_INST_NNA_CONT, XMP_INST_DCT_OFF,
							XMP_INST_DCA_CUT);

	for (i = 0; i < 33; i++) {
		for (j = 0; j < 4; j++) {
			new_event(ctx, 0, i, j, 60, 1, 44, 0x0f, 1, 0, 0);
		}
	}
	set_quirk(ctx, QUIRKS_IT, READ_EVENT_IT);

	xmp_start_player(opaque, 44100, 0);

	for (i = 0; i < 33; i++) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &fi);

		used = (i + 1) * 4;
		if (used > 128)
			used = 128;

		fail_unless(fi.virt_used == used, "number of virtual channels");
	}
}
END_TEST
