#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "2151intf.h"
#ifdef EC_YM2151_MAME
#include "ym2151.h"
#endif
#ifdef EC_YM2151_NUKED
#include "nukedopm.h"
#endif


const DEV_DEF* devDefList_YM2151[] =
{
#ifdef EC_YM2151_MAME
	&devDef_YM2151_MAME,
#endif
#ifdef EC_YM2151_NUKED
	&devDef_YM2151_Nuked,
#endif
	NULL
};
