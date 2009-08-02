#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

extern struct xmp_drv_info drv_smix;

BOOL _TNPlug_InitPlayer(struct TNPlugIFace *Self, struct TuneNet * inTuneNet, ULONG playmode)
{
   struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace * IExec = MyBase->IExec;

	xmp_drv_register(&drv_smix);

  	return TRUE;
}
