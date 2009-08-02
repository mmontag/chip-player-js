/* This is part of the TuneNet XMP plugin
 * written by Chris Young <chris@unsatisfactorysoftware.co.uk>
 * based on an example plugin by Paul Heams
 */

/*
 *  $VER: init.c $Revision$ (26-Aug-2005)
 *
 *  This file is part of TNPlug.
 *
 * Auto generated and then modified by Paul heams & Per Johansson.
 */


#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <proto/dos.h>
#include <stdarg.h>
#include <libraries/dos.h>

/* Version Tag */
#include "include/TN_XMP.tnplug_rev.h"
STATIC CONST UBYTE
#ifdef __GNUC__
__attribute__((used))
#endif
verstag[] = VERSTAG;

struct Interface *INewlib;
BPTR g_seglist;

#include "include/LibBase.h"

/*
 * The system (and compiler) rely on a symbol named _start which marks
 * the beginning of execution of an ELF file. To prevent others from 
 * executing this library, and to keep the compiler/linker happy, we
 * define an empty _start symbol here.
 *
 * On the classic system (pre-AmigaOS4) this was usually done by
 * moveq #0,d0
 * rts
 *
 */

int32 _start(void);

int32 _start(void)
{
    return 100;
}

int main(void)
{
// dummy
}


/* Open the library */
STATIC struct Library *libOpen(struct LibraryManagerInterface *Self, ULONG version)
{
    struct TNPlugLibBase *libBase = (struct TNPlugLibBase *)Self->Data.LibBase; 

    if (version > VER)
    {
        return NULL;
    }

    /* Add any specific open code here 
       Return 0 before incrementing OpenCnt to fail opening */


    /* Add up the open count */
    libBase->libNode.lib_OpenCnt++;
    return (struct Library *)libBase;

}


/* Close the library */
STATIC APTR libClose(struct LibraryManagerInterface *Self)
{
    struct TNPlugLibBase *libBase = (struct TNPlugLibBase *)Self->Data.LibBase;
    /* Make sure to undo what open did */


    /* Make the close count */
    ((struct Library *)libBase)->lib_OpenCnt--;

    return 0;
}


/* Expunge the library */
STATIC APTR libExpunge(struct LibraryManagerInterface *Self)
{
    /* If your library cannot be expunged, return 0 */
    struct ExecIFace *IExec
        = (struct ExecIFace *)(*(struct ExecBase **)4)->MainInterface;
    APTR result = (APTR)0;
    struct TNPlugLibBase *libBase = (struct TNPlugLibBase *)Self->Data.LibBase;
    if (libBase->libNode.lib_OpenCnt == 0)
    {
       struct Library *newlibbase = INewlib->Data.LibBase;

       result = g_seglist;

       /* Undo what the init code did */
       IExec->DropInterface(INewlib);
       IExec->CloseLibrary(newlibbase);

//	     result = (APTR)libBase->segList;
        /* Undo what the init code did */ 

/*
	IExec->DropInterface((struct Interface *)libBase->IDataTypes);
	    IExec->CloseLibrary (libBase->DataTypesBase);
*/

//	__lib_exit();

        IExec->Remove((struct Node *)libBase);
        IExec->DeleteLibrary((struct Library *)libBase);
    }
    else
    {
        result = (APTR)0;
        libBase->libNode.lib_Flags |= LIBF_DELEXP;
    }
    return result;
}

/* The ROMTAG Init Function */
STATIC struct Library *libInit(struct Library *LibraryBase, APTR seglist, struct Interface *exec)
{
    struct TNPlugLibBase *libBase = (struct TNPlugLibBase *)LibraryBase;
    struct ExecIFace *IExec
#ifdef __GNUC__
        __attribute__((unused))
#endif
        = (struct ExecIFace *)exec;

    libBase->libNode.lib_Node.ln_Type = NT_LIBRARY;
    libBase->libNode.lib_Node.ln_Pri  = 0;
    libBase->libNode.lib_Node.ln_Name =     TARGET;
    libBase->libNode.lib_Flags        = LIBF_SUMUSED|LIBF_CHANGED;
    libBase->libNode.lib_Version      = VER;
    libBase->libNode.lib_Revision     = AMIREV;
    libBase->libNode.lib_IdString     = (void *)VSTRING;

    libBase->segList = (BPTR)seglist;

    /* Add additional init code here if you need it. For example, to open additional
       Libraries: */
	 libBase->IExec = IExec; 
   struct Library *newlibbase;
   newlibbase = IExec->OpenLibrary("newlib.library", 52);
   if (newlibbase)
   {
       INewlib = IExec->GetInterface(newlibbase, "main", 1, NULL);
   }

   if (INewlib)
   {
       g_seglist = seglist;
//       return library;
   } else {
       IExec->DebugPrintF("Couldn't open newlib.library V52\n");
//       IExec->Alert(AT_Recovery|AG_OpenLib|AO_NewlibLib);
       return NULL;
   }

//	__lib_init( (struct Library *) (*(struct ExecBase **)4) );


	return (struct Library *)libBase;
}

/* ------------------- Manager Interface ------------------------ */
/* These are generic. Replace if you need more fancy stuff */
STATIC LONG _manager_Obtain(struct LibraryManagerInterface *Self)
{
    return ++Self->Data.RefCount;
}

STATIC ULONG _manager_Release(struct LibraryManagerInterface *Self)
{
    return --Self->Data.RefCount;
}

/* Manager interface vectors */
STATIC CONST APTR lib_manager_vectors[] =
{
    (void *)_manager_Obtain,
    (void *)_manager_Release,
    (void *)NULL,
    (void *)NULL,
    (void *)libOpen,
    (void *)libClose,
    (void *)libExpunge,
    (void *)NULL,
    (APTR)-1
};

/* "__library" interface tag list */
STATIC CONST struct TagItem lib_managerTags[] =
{
    { MIT_Name,        (Tag)"__library"       },
    { MIT_VectorTable, (Tag)lib_manager_vectors },
    { MIT_Version,     1                        },
    { TAG_DONE,        0                        }
};

/* ------------------- Library Interface(s) ------------------------ */

#include "Vectors.cpp"

/* Uncomment this line (and see below) if your library has a 68k jump table */
/* extern APTR VecTable68K[]; */

STATIC CONST struct TagItem mainTags[] =
{
    { MIT_Name,        (Tag)"main" },
    { MIT_VectorTable, (Tag)main_vectors },
    { MIT_Version,     1 },
    { TAG_DONE,        0 }
};

STATIC CONST CONST_APTR libInterfaces[] =
{
    lib_managerTags,
    mainTags,
    NULL
};

STATIC CONST struct TagItem libCreateTags[] =
{
    { CLT_DataSize,    sizeof(struct TNPlugLibBase) },
    { CLT_InitFunc,    (Tag)libInit },
    { CLT_Interfaces,  (Tag)libInterfaces},
    /* Uncomment the following line if you have a 68k jump table */
    /* { CLT_Vector68K, (Tag)VecTable68K }, */
    {TAG_DONE,         0 }
};


/* ------------------- ROM Tag ------------------------ */
STATIC CONST struct Resident lib_res
#ifdef __GNUC__
__attribute__((used))
#endif
=
{
    RTC_MATCHWORD,
    (struct Resident *)&lib_res,
    (APTR)(&lib_res + 1),
    RTF_NATIVE|RTF_AUTOINIT, /* Add RTF_COLDSTART if you want to be resident */
    VER,
    NT_LIBRARY, /* Make this NT_DEVICE if needed */
    0, /* PRI, usually not needed unless you're resident */
    TARGET,
    VSTRING,
    (APTR)libCreateTags
};
