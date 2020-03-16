#include <stddef.h>	// for NULL
#include "../EmuStructs.h"

#include "saaintf.h"
#ifdef EC_SAA1099_MAME
#include "saa1099_mame.h"
#endif
#ifdef EC_SAA1099_NRS
#undef EC_SAA1099_NRS	// not yet added
//#include "saa1099_nrs.h"
#endif
#ifdef EC_SAA1099_VB
#include "saa1099_vb.h"
#endif


const DEV_DEF* devDefList_SAA1099[] =
{
#ifdef EC_SAA1099_MAME
	&devDef_SAA1099_MAME,
#endif
#ifdef EC_SAA1099_NRS
	&devDef_SAA1099_NRS,
#endif
#ifdef EC_SAA1099_VB
	&devDef_SAA1099_VB,
#endif
	NULL
};
