#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>
#include "include/LibBase.h"

LONG _TNPlug_SeekPlayer(struct TNPlugIFace *Self, struct TuneNet * inTuneNet, ULONG seconds)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;

	xmp_seek_time((xmp_context)inTuneNet->handle,seconds);
	return seconds;
}
