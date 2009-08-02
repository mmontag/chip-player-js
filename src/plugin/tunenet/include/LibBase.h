/* This is part of the TuneNet XMP plugin
 * written by Chris Young <chris@unsatisfactorysoftware.co.uk>
 * based on an example plugin by Paul Heams
 */

#define TARGET	"XMP.tnplug"

#include "xmp.h"

struct TNPlugLibBase
{
    struct Library libNode;
    BPTR segList;
    /* If you need more data fields, add them here */
    struct ExecIFace      	*IExec;
};
