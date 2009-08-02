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

ULONG _TNPlug_Obtain(struct TNPlugIFace *Self)
{
    return (ULONG)0;
}

