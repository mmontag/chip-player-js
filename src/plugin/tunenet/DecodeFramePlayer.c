/* This is part of the TuneNet XMP plugin
 * written by Chris Young <chris@unsatisfactorysoftware.co.uk>
 * based on an example plugin by Paul Heams
 */

#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

LONG _TNPlug_DecodeFramePlayer(struct TNPlugIFace *Self, struct TuneNet * inTuneNet, WORD * outbuffer)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace * IExec = MyBase->IExec;

	ULONG size = 0;
	void *data;

	if (inTuneNet && inTuneNet->handle) 
	{
		if(xmp_player_frame((xmp_context)inTuneNet->handle) == 0)
		{
			xmp_get_buffer((xmp_context)inTuneNet->handle,&data,&size);
			inTuneNet->pcm[0]= data;
			inTuneNet->pcm[1]= (inTuneNet->pcm[0]+sizeof(WORD));
		}
   	}
	return (LONG)size/inTuneNet->dec_channels/sizeof(WORD);	// ** Returning Zero assumes failure, otherwise return No of samples **
}
