#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "ayintf.h"
#ifdef EC_AY8910_MAME
#include "ay8910.h"
#endif
#ifdef EC_AY8910_EMU2149
#include "emu2149.h"
#endif


const DEV_DEF* devDefList_AY8910[] =
{
#ifdef EC_AY8910_EMU2149
	&devDef_YM2149_Emu,
#endif
#ifdef EC_AY8910_MAME
	&devDef_AY8910_MAME,
#endif
	NULL
};
