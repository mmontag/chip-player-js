#include "test.h"
#include "../src/effects.h"

/*
Don't reset the channel volume when When a keyoff event is used
with an instrument.

xyce-dans_la_rue.xm pattern 0E/0F:

  00 D#5 0B .. ...
  01 ... .. .. ...
  02 ... .. .. ...
  03 ... .. .. ...
  04 ... .. .. ...
  05 ... .. .. ...
  06 ... .. .. ...
  07 ... .. -2 ...
  08 ... .. -2 ...
  0A ... .. -2 ...
  0B ... .. -2 ...
  0C === 0B .. ...  
  0D ... .. .. ...

  ...

  00 === 0B .. ...  
  01 ... .. .. ...
  
*/

TEST(test_player_xm_keyoff_with_instrument)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct xmp_frame_info fi;
	int i;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;

 	create_simple_module(ctx, 2, 2);
	set_instrument_volume(ctx, 0, 0, 64);
	set_instrument_fadeout(ctx, 0, 0x400);
	new_event(ctx, 0, 0, 0, 60, 1, 0, FX_SPEED, 2, 0, 0);
	new_event(ctx, 0, 1, 0, 0, 0, 0, FX_VOLSLIDE, 8, 0, 0);
	new_event(ctx, 0, 2, 0, 0, 0, 0, FX_VOLSLIDE, 8, 0, 0);
	new_event(ctx, 0, 3, 0, XMP_KEY_FADE, 1, 0, 0, 0, 0, 0);
	new_event(ctx, 0, 40, 0, XMP_KEY_FADE, 2, 0, 0, 0, 0, 0);
	set_quirk(ctx, QUIRKS_FT2, READ_EVENT_FT2);

	xmp_start_player(opaque, 44100, 0);

	/* Row 0 */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &fi);
	fail_unless(fi.channel_info[0].note == 59, "set note");
	fail_unless(fi.channel_info[0].volume == 64, "volume");
	xmp_play_frame(opaque);

	/* Rows 1 */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &fi);
	fail_unless(fi.channel_info[0].volume == 64, "volume");
	xmp_play_frame(opaque);

	/* Row 2 */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &fi);
	fail_unless(fi.channel_info[0].volume == 56, "volume");
	xmp_play_frame(opaque);

	/* Row 3: keyoff with instrument */
	xmp_play_frame(opaque);
	xmp_get_frame_info(opaque, &fi);
	fail_unless(fi.channel_info[0].note == 59, "set note");
	fail_unless(fi.channel_info[0].volume == 63, "volume");
	xmp_play_frame(opaque);

	/* Rows 4 - 63: fadeout continues */
	for (i = 5; i < 64; i++) {
		int vol = 63 - (i - 5) * 2;
		xmp_play_frame(opaque);
		fail_unless(fi.channel_info[0].volume == (vol > 0 ? vol : 0) , "volume");
		/* instrument should not change in row 40 */
		fail_unless(fi.channel_info[0].instrument == 0, "instrument");
		xmp_get_frame_info(opaque, &fi);
		xmp_play_frame(opaque);
	}

	xmp_release_module(opaque);
	xmp_free_context(opaque);
}
END_TEST
