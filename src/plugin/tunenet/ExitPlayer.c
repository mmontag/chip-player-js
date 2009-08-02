#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

VOID _TNPlug_ExitPlayer(struct TNPlugIFace *Self, struct audio_player * aplayer)
{
   struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;

	xmp_free_context(MyBase->ctx);

	return;
}

