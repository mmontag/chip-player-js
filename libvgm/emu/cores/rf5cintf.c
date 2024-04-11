#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "rf5cintf.h"
#ifdef EC_RF5C68_MAME
#include "rf5c68.h"
#endif
#ifdef EC_RF5C68_GENS
#include "scd_pcm.h"
#endif


const DEV_DEF* devDefList_RF5C68[] =
{
#ifdef EC_RF5C68_MAME
	&devDef_RF5C68_MAME,
#endif
#ifdef EC_RF5C68_GENS
	&devDef_RF5C68_Gens,
#endif
	NULL
};
