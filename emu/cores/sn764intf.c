#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "sn764intf.h"
#ifdef EC_SN76496_MAME
#include "sn76496.h"
#endif
#ifdef EC_SN76496_MAXIM
#include "sn76489.h"
#endif


const DEV_DEF* devDefList_SN76496[] =
{
#ifdef EC_SN76496_MAME
	&devDef_SN76496_MAME,
#endif
#ifdef EC_SN76496_MAXIM
	&devDef_SN76489_Maxim,
#endif
	NULL
};
