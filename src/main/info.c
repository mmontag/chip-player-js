#include <stdio.h>
#include <xmp.h>

void info_mod(struct xmp_module_info *mi)
{
	int i;

	printf("Module name    : %s\n", mi->mod->name);
	printf("Module type    : %s\n", mi->mod->type);
	printf("Module length  : %d patterns\n", mi->mod->len);
	printf("Stored patterns: %d\n", mi->mod->pat);
	printf("Instruments    : %d\n", mi->mod->ins);
	printf("Samples        : %d\n", mi->mod->smp);
	printf("Channels       : %d [ ", mi->mod->chn);

	for (i = 0; i < mi->mod->chn; i++) {
		printf("%x ", mi->mod->xxc[i].pan >> 4);
	}
	printf("]\n");

	printf("Estimated time : %dmin%ds\n", (mi->time + 500) / 60000,
					((mi->time + 500) / 1000) % 60);
}


void info_frame(struct xmp_module_info *mi)
{
	static int ord = -1, tpo = -1, bpm = -1;

	if (mi->frame != 0)
		return;

	if (mi->order != ord || mi->bpm != bpm || mi->tempo != tpo) {
	        printf("\rTempo[%02X] BPM[%02X] Pos[%02X/%02X] "
			 "Pat[%02X/%02X] Row[  /  ]   0:00:00.0",
					mi->tempo, mi->bpm,
					mi->order, mi->mod->len - 1,
					mi->pattern, mi->mod->pat - 1);
		ord = mi->order;
		bpm = mi->bpm;
		tpo = mi->tempo;
		
	}

	printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b%02X/%02X] %3d:%02d:%02d.%d",
		mi->row, mi->num_rows - 1,
		(int)(mi->time / (60 * 600)), (int)((mi->time / 600) % 60),
		(int)((mi->time / 10) % 60), (int)(mi->time % 10));

	fflush(stdout);
}
