/* This is part of the TuneNet XMP plugin
 * written by Chris Young <chris@unsatisfactorysoftware.co.uk>
 * based on an example plugin by Paul Heams
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/exec.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <utility/tagitem.h>
#include "include/proto/TNPlug.h"
#include <stdarg.h>

#include "include/LibBase.h"

BOOL _TNPlug_OpenPlayer(struct TNPlugIFace *Self, UBYTE * openme, struct TuneNet * inTuneNet)
{
	struct TNPlugLibBase * MyBase = (struct TNPlugLibBase *)Self->Data.LibBase;
	struct ExecIFace * IExec = MyBase->IExec;
	BOOL retres = FALSE;
	struct xmp_module_info modinfo;
	struct xmp_options *opt;
	
	inTuneNet->handle = (xmp_context)xmp_create_context();
	xmp_init((xmp_context)inTuneNet->handle,0,NULL);
	xmp_verbosity_level((xmp_context)inTuneNet->handle, 0);
	opt = xmp_get_options((xmp_context)inTuneNet->handle);
	opt->drv_id = "smix";
	opt->freq = 44100;
	opt->resol = 16;

	xmp_open_audio((xmp_context)inTuneNet->handle);

	if (inTuneNet->playmode == PLM_File)
	{
		if(xmp_load_module((xmp_context)inTuneNet->handle,(char *)openme))
		{
			xmp_get_module_info((xmp_context)inTuneNet->handle, &modinfo);

	 		/* Use ARender_InternalBuffer rendering - NEVER use Direct */
 			inTuneNet->DirectRender		= ARender_InternalBuffer;
			 		
			inTuneNet->max_subsongs 		= 0;					/* We should query the AHX player here for this value */
			inTuneNet->current_subsong		= 0;	/* Set to Tune 1 */
						
			/* Defaults */
			inTuneNet->dec_frequency 	= 44100;
			inTuneNet->dec_channels 	= 2;				// ** Always stereo **
			inTuneNet->bitrate 			= 0;				// ** N/A **
			inTuneNet->mix_lr = TN_STEREO;

			/* Set Tune Name etc.., if first char nulled then it TuneNet will use its own defaults */
			inTuneNet->Tune[0]		= (char) NULL;		// ** Null all first chars */

			inTuneNet->ms_duration		= modinfo.time;				// ** No reported song Time **

			inTuneNet->ST_Name[0]	= (char) NULL;
			inTuneNet->ST_Url[0]		= (char) NULL;
			inTuneNet->ST_Genre[0]	= (char) NULL;
			inTuneNet->ST_Note[0]	= (char) NULL;		

			if(modinfo.name)
				strcpy(inTuneNet->Tune,modinfo.name);
			if(modinfo.type)
				strcpy(inTuneNet->ST_Note,modinfo.type);

			retres = TRUE;										// ** Return OK/

			xmp_player_start((xmp_context)inTuneNet->handle);
		}
	}
	return retres;

}

