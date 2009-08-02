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

VOID _TNPlug_ExitPlayer(struct TNPlugIFace *Self, struct audio_player * aplayer)
{
   struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;

	return;
}

