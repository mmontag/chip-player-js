#define TARGET	"XMP.tnplug"

#include "xmp.h"

/* Number of frames to decode in one call to the decode player 
   Must be at least 10 frames in order to keep the calls down.
   5 / 10 / 25 or 50
   50 frames = 1 second */
#define	decode_frames			25	
#define decode_samples	16384

/* For 1 frame (Frequency / 50) */
#define	decode_frame_samples	44100/50

void tnxmpcallback(void *b, int i);

struct TNPlugLibBase
{
    struct Library libNode;
    BPTR segList;
    /* If you need more data fields, add them here */
    struct ExecIFace      	*IExec;
	xmp_context ctx;
};
