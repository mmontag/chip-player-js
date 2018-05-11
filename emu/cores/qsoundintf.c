#include "../EmuStructs.h"

#include "qsoundintf.h"
#ifdef EC_QSOUND_MAME
#include "qsound_mame.h"
#endif
#ifdef EC_QSOUND_VB
#include "qsound_vb.h"
#endif


const DEV_DEF* devDefList_QSound[] =
{
#ifdef EC_QSOUND_MAME
	&devDef_QSound_MAME,
#endif
#ifdef EC_QSOUND_VB
	&devDef_QSound_VB,
#endif
	NULL
};
