/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

/*
 * Common functions for MMD0/1 and MMD2/3 loaders
 * Tempo fixed by Francis Russell
 */

#include "med.h"
#include "loader.h"

#ifdef D_EBUG
const char *const mmd_inst_type[] = {
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
#endif

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

void mmd_xlat_fx(struct xmp_event *event, int bpm_on, int bpmlen, int med_8ch)
{
	switch (event->fxt) {
	case 0x00:
		/* ARPEGGIO 00
		 * Changes the pitch six times between three different
		 * pitches during the duration of the note. It can create a
		 * chord sound or other special effect. Arpeggio works better
		 * with some instruments than others.
		 */
		/* fall-through */
	case 0x01:
		/* SLIDE UP 01
		 * This slides the pitch of the current track up. It decreases
		 * the period of the note by the amount of the argument on each
		 * timing pulse. OctaMED-Pro can create slides automatically,
		 * but you may want to use this function for special effects.
		 */
		/* fall-through */
	case 0x02:
		/* SLIDE DOWN 02
		 * The same as SLIDE UP, but it slides down.
		 */
		/* fall-through */
	case 0x03:
		/* PORTAMENTO 03
		 * Makes precise sliding easy.
		 */
		/* fall-through */
	case 0x04:
		/* VIBRATO 04
		 * The left half of the argument is the vibrato speed, the
		 * right half is the depth. If the numbers are zeros, the
		 * previous speed and depth are used.
		 */
		/* fall-through */
	case 0x05:
		/* SLIDE + FADE 05
		 * ProTracker compatible. This command is the combination of
		 * commands 0300 and 0Dxx. The argument is the fade speed.
		 * The slide will continue during this command.
		 */
		/* fall-through */
	case 0x06:
		/* VIBRATO + FADE 06
		 * ProTracker compatible. Combines commands 0400 and 0Dxx.
		 * The argument is the fade speed. The vibrato will continue
		 * during this command.
		 */
		/* fall-through */
	case 0x07:
		/* TREMOLO 07
		 * ProTracker compatible.
		 * This command is a kind of "volume vibrato". The left
		 * number is the speed of the tremolo, and the right one is
		 * the depth. The depth must be quite high before the effect
		 * is audible.
		 */
		break;
	case 0x08:
		/* HOLD and DECAY 08
		 * This command must be on the same line as the note. The
		 * left half of the argument determines the decay and the
		 * right half the hold.
		 */
		event->fxt = event->fxp = 0;
		break;
	case 0x09:
		/* SECONDARY TEMPO 09
		 * This sets the secondary tempo (the number of timing
		 * pulses per note). The argument must be from 01 to 20.
		 */
		event->fxt = FX_SPEED;
		break;
	/* 0x0a not mentioned */
	case 0x0b:
		/* POSITION JUMP 0B
		 * The song plays up to this command and then jumps to
		 * another position in the play sequence. The song then
		 * loops from the point jumped to, to the end of the song
		 * forever. The purpose is to allow for introductions that
		 * play only once.
		 */
		/* fall-through */
	case 0x0c:
		/* SET VOLUME 0C
		 * Overrides the default volume of an instrument.
		 */
		break;
	case 0x0d:
		/* VOLUME SLIDE 0D
		 * Smoothly slides the volume up or down. The left half of
		 * the argument increases the volume. The right decreases it.
		 */
		event->fxt = FX_VOLSLIDE;
		break;
	case 0x0e:
		/* SYNTH JUMP 0E
		 * When used with synthetic or hybrid instruments, it
		 * triggers a jump in the Waveform Sequence List. The argument
		 * is the jump destination (line no).
		 */
		event->fxt = event->fxp = 0;
		break;
	case 0x0f:
		/* MISCELLANEOUS 0F
		 * The effect depends upon the value of the argument.
		 */
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
	case 0x11:
		/* SLIDE PITCH UP (only once) 11
		 * Equivalent to ProTracker command E1x.
		 * Lets you control the pitch with great accuracy. This
		 * command changes only this occurrence of the note.
		 */
		event->fxt = FX_F_PORTA_UP;
		break;
	case 0x12:
		/* SLIDE DOWN (only once) 12
		 * Equivalent to ProTracker command E2x.
		 */
		event->fxt = FX_F_PORTA_DN;
		break;
	case 0x14:
		/* VIBRATO 14
		 * ProTracker compatible. This is similar to command 04
		 * except the depth is halved, to give greater accuracy.
		 */
		event->fxt = FX_FINE2_VIBRA;
		break;
	case 0x15:
		/* SET FINETUNE 15
		 * Set a finetune value for a note, overrides the default
		 * fine tune value of the instrument.
		 */
		/* FIXME */
		event->fxt = FX_FINETUNE;
		break;
	case 0x16:
		/* LOOP 16
		 * Creates a loop within a block. 1600 marks the beginning
		 * of the loop.  The next occurrence of the 16 command
		 * designates the number of loops. Same as ProTracker E6x.
		 */
		event->fxt = FX_EXTENDED;
		if (event->fxp > 0x0f)
			event->fxp = 0x0f;
		event->fxp |= 0x60;
		break;
	case 0x18:
		/* STOP NOTE 18
		 * Cuts the note by zeroing the volume at the pulse specified
		 * in the argument value. This is the same as ProTracker
		 * command ECx.
		 */
		event->fxt = FX_EXTENDED;
		if (event->fxp > 0x0f)
			event->fxp = 0x0f;
		event->fxp |= 0xc0;
		break;
	case 0x19:
		/* SET SAMPLE START OFFSET
		 * Same as ProTracker command 9.
		 * When playing a sample, this command sets the starting
		 * offset (at steps of $100 = 256 bytes). Useful for speech
		 * samples.
		 */
		event->fxt = FX_OFFSET;
		break;
	case 0x1a:
		/* SLIDE VOLUME UP ONCE
		 * Only once ProTracker command EAx. Lets volume slide
		 * slowly once per line.
		 */
		event->fxt = FX_F_VSLIDE_UP;
		break;
	case 0x1b:
		/* VOLUME DOWN?
		 * Only once ProTracker command EBx ?
		 */
		event->fxt = FX_F_VSLIDE_DN;
		break;
	default:
		event->fxt = event->fxp = 0;
		break;
	}
}
