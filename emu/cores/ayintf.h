#ifndef __AYINTF_H__
#define __AYINTF_H__

#include "../EmuStructs.h"

#ifndef SNDDEV_SELECT
// undefine one of the variables to disable the cores
#define EC_AY8910_MAME		// enable AY8910 core from MAME
#define EC_AY8910_EMU2149	// enable EMU2149 core (from NSFPlay)
#endif


// cfg.chipType: chip type
//	AY8910 variants
#define AYTYPE_AY8910	0x00
#define AYTYPE_AY8912	0x01
#define AYTYPE_AY8913	0x02
#define AYTYPE_AY8930	0x03
#define AYTYPE_AY8914	0x04
//	YM2149 variants
#define AYTYPE_YM2149	0x10
#define AYTYPE_YM3439	0x11
#define AYTYPE_YMZ284	0x12
#define AYTYPE_YMZ294	0x13
//	OPN/OPNA SSG
#define AYTYPE_YM2203	0x20
#define AYTYPE_YM2608	0x21
#define AYTYPE_YM2610	0x22

// cfg.chipFlags: pin26 state
#define YM2149_PIN26_HIGH	0x00
#define YM2149_PIN26_LOW	0x10	// additional clock divider /2

#define AY8910_ZX_STEREO	0x80

typedef struct ay8910_config
{
	DEV_GEN_CFG _genCfg;
	
	UINT8 chipType;
	UINT8 chipFlags;
} AY8910_CFG;


#define OPT_AY8910_PCM3CH_DETECT	0x01	// enable 3-channel PCM detection and disable per-channel panning in that case


extern const DEV_DEF* devDefList_AY8910[];

#endif	// __AYINTF_H__
