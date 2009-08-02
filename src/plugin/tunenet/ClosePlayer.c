#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

/* Called when song is closed */

VOID _TNPlug_ClosePlayer(struct TNPlugIFace *Self, struct TuneNet * inTuneNet)
{
  	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace * IExec = MyBase->IExec;

	if (inTuneNet) 
	{
		if(inTuneNet->handle)
		{
			xmp_player_end((xmp_context)inTuneNet->handle);
			xmp_release_module((xmp_context)inTuneNet->handle);
			xmp_close_audio((xmp_context)inTuneNet->handle);
			xmp_free_context((xmp_context)inTuneNet->handle);
			inTuneNet->handle = NULL;
		}

	}
}
