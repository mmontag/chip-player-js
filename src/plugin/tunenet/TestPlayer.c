#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "include/LibBase.h"

/* Testme is the name of the Tune, TestBuffer is a small segment of the file - the size of totalsize */
LONG _TNPlug_TestPlayer(struct TNPlugIFace *Self, UBYTE * testme, UBYTE * testbuffer, ULONG totalsize, ULONG testsize)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace 		* IExec = MyBase->IExec;
   	LONG retn = 0,j=0;

	if(!MyBase->ctx)
	{
		MyBase->ctx = xmp_create_context();
		xmp_init(MyBase->ctx,NULL,NULL);
		xmp_verbosity_level(MyBase->ctx, 0);
	}

	/* Check buffer comming in */

	j = xmp_test_module(MyBase->ctx, (char *)testme, NULL);

	if(j==0)
	{
		retn = PLM_File;
	}

	return retn;
}
