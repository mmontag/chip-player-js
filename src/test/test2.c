
#include <stdio.h>
#include <stdlib.h>
#include "xmp.h"
#include "sound.h"

static char *note_name[] = {
	"C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B "
};

static void display_data(struct xmp_module_info *mi)
{
	int i;

	printf("%02x| ", mi->row);
	for (i = 0; i < mi->mod->chn; i++) {
		int track = mi->mod->xxp[mi->pattern]->index[i];
		struct xmp_event *event = &mi->mod->xxt[track]->event[mi->row];

		if (event->note > 0x80) {
			printf("=== ");
		} else if (event->note > 0) {
			int note = event->note - 1;
			printf("%s%d ", note_name[note % 12], note / 12);
		} else {
			printf("--- ");
		}

		if (event->ins > 0) {
			printf("%02X", event->ins);
		} else {
			printf("--");
		}

		printf("|");
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	static xmp_context ctx;
	static struct xmp_module_info mi[2];
	int current, prev;
	int i;

	if (sound_init(44100, 2) < 0) {
		fprintf(stderr, "%s: can't initialize sound\n", argv[0]);
		exit(1);
	}

	ctx = xmp_create_context();

	for (i = 1; i < argc; i++) {
		if (xmp_load_module(ctx, argv[i]) < 0) {
			fprintf(stderr, "%s: error loading %s\n", argv[0],
				argv[i]);
			continue;
		}

		if (xmp_player_start(ctx) == 0) {

			/* Show module data */

			xmp_player_get_info(ctx, &mi[0]);
			printf("%s (%s)\n", mi[0].mod->name, mi[0].mod->type);
			mi[0].order = -1;
			mi[0].row = -1;
			current = 0;

			/* Play module */

			while (xmp_player_frame(ctx) == 0) {
				prev = current;
				current ^= 1;

				xmp_player_get_info(ctx, &mi[current]);
				sound_play(mi[current].buffer,
						mi[current].buffer_size);

				if (mi[current].order != mi[prev].order) {
					printf("\n%02x:%02x\n",
					       mi[current].order,
					       mi[current].pattern);
				}
				if (mi[current].row != mi[prev].row) {
					display_data(&mi[current]);
				}
			}
			xmp_player_end(ctx);
		}

		xmp_release_module(ctx);
		printf("\n");
	}

	xmp_free_context(ctx);
	sound_deinit();

	return 0;
}
