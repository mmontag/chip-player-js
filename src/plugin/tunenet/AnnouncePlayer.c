#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include "include/libraries/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"
#include "include/TN_XMP.tnplug_rev.h"

struct audio_player * _TNPlug_AnnouncePlayer(struct TNPlugIFace *Self, ULONG version)
{
	static struct audio_player my_player =
   {
		"XMP",															// ** As reported in TuneNet (keep this short).
		"Chris Young and the XMP team\nhttp://xmp.sourceforge.net\nhttp://www.unsatisfactorysoftware.co.uk",		// ** 127 chars max!
		"Extended Module Player",
		"",																			// ** Extentions (.mp3) comma seperated (mp3,mp2) ** (16 chars max)
		(char *) NULL,																	// ** Pointer to pattern match string - IGNORE for now **
		(struct Library *) NULL,													// ** TN PRIVATE :- These Must be left NULL **
		(struct Interface *) NULL,													// ** TN PRIVARE :- These Must be left NULL **
		0,																					// ** BOOL -- IGNORE **
		PLM_File,																	// ** PlayModes accepted (OR together)
		aPlayer_SEEK,
		PLUGIN_API_VERSION,																				// Version MMrrmm 10000 = Version 1
		VER*10000 + REV*100 + SUB,  //020701 (2.7.1)
		NULL,
		TN_PRI_Normal,
		0
	};
	struct  TNPlugLibBase * MyBase;
	MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
 
	/* Returns information about the player if better than version 0.70, otherwise fail. */
	if (version >= 7600) return &my_player;
	else return (struct audio_player *) 0;
}
