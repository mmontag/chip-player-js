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
	ULONG thislen = 0, tlen = 0;
	BYTE *left;
	BYTE *right;
	BYTE *mono;
	WORD *tmp,*tmp2;
	void *data;
	ULONG max = decode_samples * decode_frames;
	ULONG acc = 0;
	int i;

	if (inTuneNet && inTuneNet->handle) 
	{

IExec->DebugPrintF("%lx\n",inTuneNet->handle);

		tmp = inTuneNet->pcm[0];
		tmp2 = inTuneNet->pcm[1];

//		for(i = 0; i < decode_frames; i++)
	//	{
			if(xmp_player_frame((xmp_context)inTuneNet->handle) == 0)
			{
				xmp_get_buffer((xmp_context)inTuneNet->handle,&data,&size);
		//		IExec->CopyMem(data,inTuneNet->pcm[0],size); // + acc,size);
				inTuneNet->pcm[0]= data;
				inTuneNet->pcm[1]= (inTuneNet->pcm[0]+sizeof(WORD));

	//			acc += size;
			}
//		}
   	}
	return (LONG)size/inTuneNet->dec_channels/sizeof(WORD);	// ** Returning Zero assumes failure, otherwise return No of samples **
}
