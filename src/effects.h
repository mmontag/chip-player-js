#ifndef LIBXMP_EFFECTS_H
#define LIBXMP_EFFECTS_H

/* Protracker effects */
#define FX_ARPEGGIO	0x00
#define FX_PORTA_UP	0x01
#define FX_PORTA_DN	0x02
#define FX_TONEPORTA	0x03
#define FX_VIBRATO	0x04
#define FX_TONE_VSLIDE  0x05
#define FX_VIBRA_VSLIDE	0x06
#define FX_TREMOLO	0x07
#define FX_OFFSET	0x09
#define FX_VOLSLIDE	0x0a
#define FX_JUMP		0x0b
#define FX_VOLSET	0x0c
#define FX_BREAK	0x0d
#define FX_EXTENDED	0x0e
#define FX_SPEED	0x0f

/* Protracker extended effects */
#define EX_F_PORTA_UP	0x01
#define EX_F_PORTA_DN	0x02
#define EX_GLISS	0x03
#define EX_VIBRATO_WF	0x04
#define EX_FINETUNE	0x05
#define EX_PATTERN_LOOP	0x06
#define EX_TREMOLO_WF	0x07
#define EX_RETRIG	0x09
#define EX_F_VSLIDE_UP	0x0a
#define EX_F_VSLIDE_DN	0x0b
#define EX_CUT		0x0c
#define EX_DELAY	0x0d
#define EX_PATT_DELAY	0x0e
#define EX_INVLOOP	0x0f

/* Fast tracker effects */
#define FX_SETPAN	0x08

/* Fast Tracker II effects */
#define FX_GLOBALVOL	0x20
#define FX_GVOL_SLIDE	0x21
#define FX_KEYOFF	0x22
#define FX_ENVPOS	0x23
#define FX_MASTER_PAN	0x24
#define FX_PANSLIDE	0x25
#define FX_MULTI_RETRIG	0x26
#define FX_TREMOR	0x27
#define FX_XF_PORTA	0x28
#define FX_VOLSLIDE_2	0x29

/* S3M effects */
#define FX_S3M_SPEED	0x30	/* S3M */
#define FX_S3M_BPM	0x31	/* S3M */
#define FX_S3M_ARPEGGIO	0x32
#define FX_FINE_VIBRATO	0x33	/* S3M/PTM/IMF/LIQ */

/* IT effects */
#define FX_TRK_VOL      0x40
#define FX_TRK_VSLIDE   0x41
#define FX_TRK_FVSLIDE  0x42
#define FX_IT_INSTFUNC	0x43
#define FX_FLT_CUTOFF	0x44
#define FX_FLT_RESN	0x45
#define FX_FINE2_VIBRA	0x46
#define FX_IT_BPM	0x47
#define FX_IT_ROWDELAY	0x48
#define FX_IT_PANSLIDE	0x49
#define FX_PANBRELLO	0x4a
#define FX_PANBRELLO_WF	0x4b

#ifndef XMP_CORE_PLAYER
/* PTM effects */
#define FX_NSLIDE_DN	0x71	/* IMF/PTM note slide down */
#define FX_NSLIDE_UP	0x72	/* IMF/PTM note slide up */
#define FX_NSLIDE_R_UP	0x73	/* PTM note slide down with retrigger */
#define FX_NSLIDE_R_DN	0x74	/* PTM note slide up with retrigger */

/* MED effects */
#define FX_HOLD_DECAY	0x75
#define FX_SETPITCH	0x76
#define FX_MED_DECAY	0x77	/* MMD hold/decay */

/* Oktalyzer effects */
#define FX_OKT_ARP3	0x80
#define FX_OKT_ARP4	0x81
#define FX_OKT_ARP5	0x82
#define FX_NSLIDE2_DN	0x83
#define FX_NSLIDE2_UP	0x84
#define FX_F_NSLIDE_DN	0x85
#define FX_F_NSLIDE_UP	0x86

/* His Master's Noise effects */
#define FX_MEGAARP	0x88	/* Smaksak effect 7: MegaArp */

/* Persistent effects -- for 669, FNK and FAR */
#define FX_PER_PORTA_DN	0x90
#define FX_PER_PORTA_UP	0x91
#define FX_PER_TPORTA	0x92
#define FX_PER_VIBRATO	0x93
#define FX_PER_VSLD_UP	0x94
#define FX_PER_VSLD_DN	0x95
#define FX_SPEED_CP	0x96
#define FX_PER_CANCEL	0x97

/* Extra effects */
#define FX_VOLSLIDE_UP	0xa0	/* SFX, MDL */
#define FX_VOLSLIDE_DN	0xa1
#define FX_F_VSLIDE	0xa2	/* IMF/MDL */
#define FX_CHORUS	0xa3	/* IMF */
#define FX_REVERB	0xa4	/* IMF */
#endif

#define FX_F_VSLIDE_UP	0xb0	/* MMD */
#define FX_F_VSLIDE_DN	0xb1	/* MMD */
#define FX_F_PORTA_UP	0xb2	/* MMD */
#define FX_F_PORTA_DN	0xb3	/* MMD */
#define FX_FINETUNE	0xb4
#define FX_PATT_DELAY	0xb5	/* MMD */

#endif /* LIBXMP_EFFECTS_H */
