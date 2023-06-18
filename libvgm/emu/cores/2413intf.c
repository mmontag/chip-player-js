#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "2413intf.h"
#ifdef EC_YM2413_MAME
#include "ym2413.h"
#endif
#ifdef EC_YM2413_EMU2413
#include "emu2413.h"
#endif
#ifdef EC_YM2413_NUKED
#include "nukedopll.h"
#endif


const DEV_DEF* devDefList_YM2413[] =
{
#ifdef EC_YM2413_EMU2413
	&devDef_YM2413_Emu,	// default, because it's better than MAME
#endif
#ifdef EC_YM2413_MAME
	&devDef_YM2413_MAME,
#endif
#ifdef EC_YM2413_NUKED
	&devDef_YM2413_Nuked,
#endif
	NULL
};
