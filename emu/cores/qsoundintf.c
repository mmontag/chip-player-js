#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "qsoundintf.h"
#ifdef EC_QSOUND_MAME
#include "qsound_mame.h"
#endif
#ifdef EC_QSOUND_CTR
#include "qsound_ctr.h"
#endif


const DEV_DEF* devDefList_QSound[] =
{
#ifdef EC_QSOUND_CTR
	&devDef_QSound_ctr,
#endif
#ifdef EC_QSOUND_MAME
	&devDef_QSound_MAME,
#endif
	NULL
};
