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
#include <stdlib.h>
#include <string.h>

#include "include/LibBase.h"

/* Testme is the name of the Tune, TestBuffer is a small segment of the file - the size of totalsize */
LONG _TNPlug_TestPlayer(struct TNPlugIFace *Self, UBYTE * testme, UBYTE * testbuffer, ULONG totalsize, ULONG testsize)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace 		* IExec = MyBase->IExec;
   	LONG j=0;
	xmp_context ctx;

	ctx = xmp_create_context();
	xmp_init(ctx,NULL,NULL);
	xmp_verbosity_level(ctx, 0);

	/* Check buffer comming in */

	j = xmp_test_module(ctx, (char *)testme, NULL);

	if(j==0) return PLM_File;
		else return 0;
}
