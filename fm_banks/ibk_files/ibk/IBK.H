/* IBK.H - Definitions for SBTimbre Bank File 
** Jamie O'Connell - 91.01.13 
** Modified 92.06.28 JWO - It seems CL is now using the 12th byte as a 
**                         percussion voice indicator                  
** Modified 93.07.02 JWO - Added transpos parameter to IBK timbre      
** Modified 93.10.11 JWO - Added Percussion Pitch                      
*/

#ifndef IBK_H
#define IBK_H

#define IBKEXT   "IBK"
#define SBIEXT   "SBI"

#define SBISIG   "SBI\x1A"     /* 4 chars long */
#define IBKSIG   "IBK\x1A"     /* 4 chars long */

/* Packed Timbre Parameters */

typedef struct {        /* 16 Bytes each */
	BYTE modchar;   /* AM, VIB, SUS, KSR, MUL */
	BYTE carchar;
	BYTE modscal;   /* KSL, TL */
	BYTE carscal;
	BYTE modad;     /* Attack/Decay */
	BYTE carad;
	BYTE modsr;     /* Sustain/Release */
	BYTE carsr;
	BYTE modwave;   /* Wave Select */
	BYTE carwave;
        BYTE feedback;  /* FB, Connection *(inverse of Adlib)* <- not true? */
                        /* The following was originally reserved...CL uses  */
                        /* the next byte the same way we do: BD=6,SD=7,TT=8 */
                        /* CY=9,HH=10                                       */
        BYTE percvoc;   /* Percussion voice number                    : JWO */
        char transpos;  /* Number of notes to transpose timbre, signed: JWO */
        BYTE dpitch;    /* percussion pitch: MIDI Note 0 - 127        : JWO */
        BYTE rsv[2];    /* unsused - so far */
        } SBTIMBRE;     

/* SBI Instrument file */

typedef struct {
        char     sig[4];             /* signature: "SBI\x1A"  */
        char     name[32];           /* instrument name: NUL Terminated */
        SBTIMBRE snd;                /* instrument parameters */
        } SBIFMT;

/* IBK Instrument Bank File */

typedef struct {                     /* 3204 Bytes (0x0C83) */
        char     sig[4];             /* signature: "IBK\x1A"  */
        SBTIMBRE snd[128];           /* Instrument block */
        char     name[128][9];       /* name block: NUL terminated strings */
        } IBKFMT;

#endif
