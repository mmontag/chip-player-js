/*
 * Common functions for MMD0/1 and MMD2/3 loaders
 * Tempo fixed by Francis Russell
 */

#include "med.h"
#include "load.h"

char *mmd_inst_type[] = {
	"HYB",			/* -2 */
	"SYN",			/* -1 */
	"SMP",			/*  0 */
	"I5O",			/*  1 */
	"I3O",			/*  2 */
	"I2O",			/*  3 */
	"I4O",			/*  4 */
	"I6O",			/*  5 */
	"I7O",			/*  6 */
	"EXT",			/*  7 */
};

int mmd_get_8ch_tempo(int tempo)
{
	const int tempos[10] = {
		47, 43, 40, 37, 35, 32, 30, 29, 27, 26
	};

	if (tempo > 0) {
		tempo = tempo > 10 ? 10 : tempo;
		return tempos[tempo-1];
	} else {
		return tempo;
	}
}

void mmd_xlat_fx(struct xxm_event *event, int bpm_on, int bpmlen, int med_8ch)
{
	if (event->fxt > 0x0f) {
		event->fxt = event->fxp = 0;
		return;
	}

	switch (event->fxt) {
	case 0x05:		/* Old vibrato */
		event->fxp = (LSN(event->fxp) << 4) | MSN(event->fxp);
		break;
	case 0x06:		/* Not defined in MED 3.2 */
	case 0x07:		/* Not defined in MED 3.2 */
		break;
	case 0x08:		/* Set hold/decay */
		break;
	case 0x09:		/* Set secondary tempo */
		event->fxt = FX_TEMPO;
		break;
	case 0x0d:		/* Volume slide */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0e:		/* Synth JMP */
		break;
	case 0x0f:
		if (event->fxp == 0x00) {	/* Jump to next block */
			event->fxt = 0x0d;
			break;
		} else if (event->fxp <= 0xf0) {
			event->fxt = FX_S3M_BPM;
                        event->fxp = med_8ch ? mmd_get_8ch_tempo(event->fxp) : (bpm_on ? event->fxp/bpmlen : event->fxp);
			break;
		} else switch (event->fxp) {
		case 0xf1:	/* Play note twice */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 3;
			break;
		case 0xf2:	/* Delay note */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_DELAY << 4) | 3;
			break;
		case 0xf3:	/* Play note three times */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_RETRIG << 4) | 2;
			break;
		case 0xf8:	/* Turn filter off */
		case 0xf9:	/* Turn filter on */
		case 0xfa:	/* MIDI pedal on */
		case 0xfb:	/* MIDI pedal off */
		case 0xfd:	/* Set pitch */
		case 0xfe:	/* End of song */
			event->fxt = event->fxp = 0;
			break;
		case 0xff:	/* Note cut */
			event->fxt = FX_EXTENDED;
			event->fxp = (EX_CUT << 4) | 3;
			break;
		default:
			event->fxt = event->fxp = 0;
		}
		break;
	}
}
