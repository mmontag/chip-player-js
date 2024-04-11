#include "test.h"
#undef TEST
#include "../src/player.h"
#include "../src/mixer.h"
#include "../src/virtual.h"

static void _compare_mixer_data(const char *mod, const char *data, int loops, int ignore_rv)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct module_data *m;
        struct player_data *p;
        struct mixer_voice *vi;
	struct xmp_frame_info fi;
	int time, row, frame, chan, period, note, ins, vol, pan, pos0, cutoff;
	char line[200];
	FILE *f;
	int i, voc, ret;

	f = fopen(data, "r");
	fail_unless(f != NULL, "can't open data file");

	opaque = xmp_create_context();
	fail_unless(opaque != NULL, "can't create context");

	ret = xmp_load_module(opaque, mod);
	fail_unless(ret == 0, "can't load module");

	ctx = (struct context_data *)opaque;
	m = &ctx->m;
	p = &ctx->p;

	xmp_start_player(opaque, 44100, 0);
	xmp_set_player(opaque, XMP_PLAYER_MIX, 100);

	while (1) {
		xmp_play_frame(opaque);
		xmp_get_frame_info(opaque, &fi);
		if (fi.loop_count >= loops)
			break;

		for (i = 0; i < m->mod.chn; i++) {
			struct xmp_channel_info *ci = &fi.channel_info[i];
			struct channel_data *xc = &p->xc_data[i];
			int num;

			voc = map_channel(p, i);
			if (voc < 0 || TEST_NOTE(NOTE_SAMPLE_END))
				continue;

			vi = &p->virt.voice_array[voc];

#if 1
			fail_unless(fgets(line, 200, f) != NULL, "early EOF");
			num = sscanf(line, "%d %d %d %d %d %d %d %d %d %d %d",
				&time, &row, &frame, &chan, &period,
				&note, &ins, &vol, &pan, &pos0, &cutoff);

			/* Allow some error in values derived from floating point math. */
			fail_unless(abs(fi.time - time) <= 1, "time mismatch");
			fail_unless(fi.row     == row,    "row mismatch");
			fail_unless(fi.frame   == frame,  "frame mismatch");
			fail_unless(i          == chan,   "channel mismatch");
			fail_unless(ci->period == period, "period mismatch");
			fail_unless(vi->note   == note,   "note mismatch");
			fail_unless(vi->ins    == ins,    "instrument");
			if (!ignore_rv) {
				fail_unless(vi->vol == vol, "volume mismatch");
				fail_unless(vi->pan == pan, "pan mismatch");
			}
			fail_unless(abs(vi->pos0 - pos0) <= 1, "position mismatch");
			if (num >= 11) {
				fail_unless(vi->filter.cutoff == cutoff,
							  "cutoff mismatch");
			}
#else
			fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d\n",
				fi.time, fi.row, fi.frame, i, ci->period,
				vi->note, vi->ins, vi->vol, vi->pan, vi->pos0, vi->filter.cutoff);
#endif
		}
		
	}

	if (fgets(line, 200, f) != NULL)
		fail_unless(line[0] == '\0', "not end of data file");
	//fail_unless(feof(f), "not end of data file");

	xmp_end_player(opaque);
	xmp_release_module(opaque);
	xmp_free_context(opaque);
}

void compare_mixer_data(const char *mod, const char *data)
{
	_compare_mixer_data(mod, data, 1, 0);
}

void compare_mixer_data_loops(const char *mod, const char *data, int loops)
{
	_compare_mixer_data(mod, data, loops, 0);
}

void compare_mixer_data_no_rv(const char *mod, const char *data)
{
	_compare_mixer_data(mod, data, 1, 1);
}
