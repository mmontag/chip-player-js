#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "c6280intf.h"
#ifdef EC_C6280_MAME
#include "c6280_mame.h"
#endif
#ifdef EC_C6280_OOTAKE
#include "Ootake_PSG.h"
#endif


const DEV_DEF* devDefList_C6280[] =
{
#ifdef EC_C6280_OOTAKE
	&devDef_C6280_Ootake,
#endif
#ifdef EC_C6280_MAME
	&devDef_C6280_MAME,
#endif
	NULL
};
