#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

/* Misc events which are handled after a tune is opened */
ULONG _TNPlug_DoNotify(struct TNPlugIFace *Self, struct TuneNet * inTuneNet, ULONG item, ULONG value)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace * IExec = MyBase->IExec;							// ** Exec not used in the code below, so commented out **
	ULONG TestBit = 0;
	ULONG retres = 0; 

	/* Do Notitify - Add as many notifications as the player supports */
	if ((inTuneNet) && (inTuneNet->handle))
	{
		switch (item)
		{
			/* Change Sub Song */
			case TNDN_SubSong:
			{
				/* If the channels / frequency or whatever changes here you must store them in the TuneNet object so that tunenet
							can decide what to do - for this example we assue it stays the same.
				*/
				if (inTuneNet->handle)
				{
				}
			}
			break;

			case TNDN_PlaySpeed:
			{
			
			}
			break;
		}
	}
	return retres;		// ** Couldn't Open **
}
