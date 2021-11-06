// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont
// thanks-to: kingshriek
/*
    Sega/Yamaha YMF292-F (SCSP = Saturn Custom Sound Processor) emulation
    By ElSemi
    MAME/M1 conversion and cleanup by R. Belmont
    Additional code and bugfixes by kingshriek

    This chip has 32 voices.  Each voice can play a sample or be part of
    an FM construct.  Unlike traditional Yamaha FM chips, the base waveform
    for the FM still comes from the wavetable RAM.

    ChangeLog:
    * November 25, 2003  (ES) Fixed buggy timers and envelope overflows.
                         (RB) Improved sample rates other than 44100, multiple
                             chips now works properly.
    * December 02, 2003  (ES) Added DISDL register support, improves mix.
    * April 28, 2004     (ES) Corrected envelope rates, added key-rate scaling,
                             added ringbuffer support.
    * January 8, 2005    (RB) Added ability to specify region offset for RAM.
    * January 26, 2007   (ES) Added on-board DSP capability
    * September 24, 2007 (RB+ES) Removed fake reverb.  Rewrote timers and IRQ handling.
                             Fixed case where voice frequency is updated while looping.
                             Enabled DSP again.
    * December 16, 2007  (kingshriek) Many EG bug fixes, implemented effects mixer,
                             implemented FM.
    * January 5, 2008    (kingshriek+RB) Working, good-sounding FM, removed obsolete non-USEDSP code.
    * April 22, 2009     ("PluginNinja") Improved slot monitor, misc cleanups
    * June 6, 2011       (AS) Rewrote DMA from scratch, Darius 2 relies on it.
*/

#include <math.h>	// for pow() in scsplfo.c
#include <stdlib.h>
#include <string.h>	// for memset

#include "../../stdtype.h"
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "../logging.h"
#include "scsp.h"
#include "scspdsp.h"


static void SCSP_DoMasterSamples(void* info, UINT32 nsamples, DEV_SMPL **outputs);
static UINT8 device_start_scsp(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_scsp(void* info);
static void device_reset_scsp(void* info);

// SCSP register access
static void scsp_w8(void* info, UINT16 offset, UINT8 data);
static UINT8 scsp_r8(void* info, UINT16 offset);
static void SCSP_w16(void* info, UINT16 addr, UINT16 val);
static UINT16 SCSP_r16(void* info, UINT16 addr);

// MIDI I/O access (used for comms on Model 2/3)
static void scsp_midi_in(void* info, UINT8 offset, UINT8 data);
static UINT8 scsp_midi_out_r(void* info, UINT8 offset);

static void scsp_write_ram(void* info, UINT32 offset, UINT32 length, const UINT8* data);
static void scsp_set_mute_mask(void* info, UINT32 MuteMask);
static void scsp_set_options(void* info, UINT32 Flags);
static void scsp_set_log_cb(void* info, DEVCB_LOG func, void* param);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, scsp_w8},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D8, 0, scsp_r8},
	{RWF_REGISTER | RWF_WRITE, DEVRW_A16D16, 0, SCSP_w16},
	{RWF_REGISTER | RWF_READ, DEVRW_A16D16, 0, SCSP_r16},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, scsp_write_ram},
	{RWF_CHN_MUTE | RWF_WRITE, DEVRW_ALL, 0, scsp_set_mute_mask},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"SCSP", "MAME", FCC_MAME,
	
	device_start_scsp,
	device_stop_scsp,
	device_reset_scsp,
	SCSP_DoMasterSamples,
	
	scsp_set_options,	// SetOptionBits
	scsp_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	scsp_set_log_cb,	// SetLoggingCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_SCSP[] =
{
	&devDef,
	NULL
};


#define CLIP16(x)  (((x) > 32767) ? 32767 : (((x) < -32768) ? -32768 : (x)))
#define CLIP18(x)  (((x) > 131071) ? 131071 : (((x) < -131072) ? -131072 : (x)))


#define SHIFT   12
#define FIX(v)  ((UINT32) ((float) (1<<SHIFT)*(v)))


#define EG_SHIFT    16
#define FM_DELAY    0   // delay in number of slots processed before samples are written to the FM ring buffer
                        // driver code indicates should be 4, but sounds distorted then

// include the LFO handling code
#define INCLUDE_FROM_SCSP_C
#include "scsplfo.c"

/*
    SCSP features 32 programmable slots
    that can generate FM and PCM (from ROM/RAM) sound
*/

//SLOT PARAMETERS
#define KEYONEX(slot)   ((slot->udata.data[0x0]>>0x0)&0x1000)
#define KEYONB(slot)    ((slot->udata.data[0x0]>>0x0)&0x0800)
#define SBCTL(slot)     ((slot->udata.data[0x0]>>0x9)&0x0003)
#define SSCTL(slot)     ((slot->udata.data[0x0]>>0x7)&0x0003)
#define LPCTL(slot)     ((slot->udata.data[0x0]>>0x5)&0x0003)
#define PCM8B(slot)     ((slot->udata.data[0x0]>>0x0)&0x0010)

#define SA(slot)        (((slot->udata.data[0x0]&0xF)<<16)|(slot->udata.data[0x1]))

#define LSA(slot)       (slot->udata.data[0x2])

#define LEA(slot)       (slot->udata.data[0x3])

#define D2R(slot)       ((slot->udata.data[0x4]>>0xB)&0x001F)
#define D1R(slot)       ((slot->udata.data[0x4]>>0x6)&0x001F)
#define EGHOLD(slot)    ((slot->udata.data[0x4]>>0x0)&0x0020)
#define AR(slot)        ((slot->udata.data[0x4]>>0x0)&0x001F)

#define LPSLNK(slot)    ((slot->udata.data[0x5]>>0x0)&0x4000)
#define KRS(slot)       ((slot->udata.data[0x5]>>0xA)&0x000F)
#define DL(slot)        ((slot->udata.data[0x5]>>0x5)&0x001F)
#define RR(slot)        ((slot->udata.data[0x5]>>0x0)&0x001F)

#define STWINH(slot)    ((slot->udata.data[0x6]>>0x0)&0x0200)
#define SDIR(slot)      ((slot->udata.data[0x6]>>0x0)&0x0100)
#define TL(slot)        ((slot->udata.data[0x6]>>0x0)&0x00FF)

#define MDL(slot)       ((slot->udata.data[0x7]>>0xC)&0x000F)
#define MDXSL(slot)     ((slot->udata.data[0x7]>>0x6)&0x003F)
#define MDYSL(slot)     ((slot->udata.data[0x7]>>0x0)&0x003F)

#define OCT(slot)       ((slot->udata.data[0x8]>>0xB)&0x000F)
#define FNS(slot)       ((slot->udata.data[0x8]>>0x0)&0x03FF)

#define LFORE(slot)     ((slot->udata.data[0x9]>>0x0)&0x8000)
#define LFOF(slot)      ((slot->udata.data[0x9]>>0xA)&0x001F)
#define PLFOWS(slot)    ((slot->udata.data[0x9]>>0x8)&0x0003)
#define PLFOS(slot)     ((slot->udata.data[0x9]>>0x5)&0x0007)
#define ALFOWS(slot)    ((slot->udata.data[0x9]>>0x3)&0x0003)
#define ALFOS(slot)     ((slot->udata.data[0x9]>>0x0)&0x0007)

#define ISEL(slot)      ((slot->udata.data[0xA]>>0x3)&0x000F)
#define IMXL(slot)      ((slot->udata.data[0xA]>>0x0)&0x0007)

#define DISDL(slot)     ((slot->udata.data[0xB]>>0xD)&0x0007)
#define DIPAN(slot)     ((slot->udata.data[0xB]>>0x8)&0x001F)
#define EFSDL(slot)     ((slot->udata.data[0xB]>>0x5)&0x0007)
#define EFPAN(slot)     ((slot->udata.data[0xB]>>0x0)&0x001F)

//Envelope times in ms
static const double ARTimes[64]={100000/*infinity*/,100000/*infinity*/,8100.0,6900.0,6000.0,4800.0,4000.0,3400.0,3000.0,2400.0,2000.0,1700.0,1500.0,
					1200.0,1000.0,860.0,760.0,600.0,500.0,430.0,380.0,300.0,250.0,220.0,190.0,150.0,130.0,110.0,95.0,
					76.0,63.0,55.0,47.0,38.0,31.0,27.0,24.0,19.0,15.0,13.0,12.0,9.4,7.9,6.8,6.0,4.7,3.8,3.4,3.0,2.4,
					2.0,1.8,1.6,1.3,1.1,0.93,0.85,0.65,0.53,0.44,0.40,0.35,0.0,0.0};
static const double DRTimes[64]={100000/*infinity*/,100000/*infinity*/,118200.0,101300.0,88600.0,70900.0,59100.0,50700.0,44300.0,35500.0,29600.0,25300.0,22200.0,17700.0,
					14800.0,12700.0,11100.0,8900.0,7400.0,6300.0,5500.0,4400.0,3700.0,3200.0,2800.0,2200.0,1800.0,1600.0,1400.0,1100.0,
					920.0,790.0,690.0,550.0,460.0,390.0,340.0,270.0,230.0,200.0,170.0,140.0,110.0,98.0,85.0,68.0,57.0,49.0,43.0,34.0,
					28.0,25.0,22.0,18.0,14.0,12.0,11.0,8.5,7.1,6.1,5.4,4.3,3.6,3.1};

typedef enum {SCSP_ATTACK,SCSP_DECAY1,SCSP_DECAY2,SCSP_RELEASE} SCSP_STATE;

typedef struct _EG
{
	int volume; //
	SCSP_STATE state;
	int step;
	//step vals
	int AR;     //Attack
	int D1R;    //Decay1
	int D2R;    //Decay2
	int RR;     //Release

	int DL;     //Decay level
	UINT8 EGHOLD;
	UINT8 LPLINK;
} SCSP_EG_t;

typedef struct _SLOT
{
	union
	{
		UINT16 data[0x10];  //only 0x1a bytes used
		UINT8 datab[0x20];
	} udata;

	UINT8 Backwards;    //the wave is playing backwards
	UINT8 active;   //this slot is currently playing
	UINT8 Muted;
	UINT8 *base;        //samples base address
	UINT32 cur_addr;    //current play address (24.8)
	UINT32 nxt_addr;    //next play address
	UINT32 step;        //pitch step (24.8)
	SCSP_EG_t EG;       //Envelope
	SCSP_LFO_t PLFO;    //Phase LFO
	SCSP_LFO_t ALFO;    //Amplitude LFO
	int slot;
	INT16 Prev; //Previous sample (for interpolation)
} SCSP_SLOT;


#define MEM4B(scsp)     ((scsp->udata.data[0]>>0x0)&0x0200)
#define DAC18B(scsp)    ((scsp->udata.data[0]>>0x0)&0x0100)
#define MVOL(scsp)      ((scsp->udata.data[0]>>0x0)&0x000F)
#define RBL(scsp)       ((scsp->udata.data[1]>>0x7)&0x0003)
#define RBP(scsp)       ((scsp->udata.data[1]>>0x0)&0x003F)
#define MOFULL(scsp)    ((scsp->udata.data[2]>>0x0)&0x1000)
#define MOEMPTY(scsp)   ((scsp->udata.data[2]>>0x0)&0x0800)
#define MIOVF(scsp)     ((scsp->udata.data[2]>>0x0)&0x0400)
#define MIFULL(scsp)    ((scsp->udata.data[2]>>0x0)&0x0200)
#define MIEMPTY(scsp)   ((scsp->udata.data[2]>>0x0)&0x0100)

#define SCILV0(scsp)    ((scsp->udata.data[0x24/2]>>0x0)&0xff)
#define SCILV1(scsp)    ((scsp->udata.data[0x26/2]>>0x0)&0xff)
#define SCILV2(scsp)    ((scsp->udata.data[0x28/2]>>0x0)&0xff)

#define SCIEX0  0
#define SCIEX1  1
#define SCIEX2  2
#define SCIMID  3
#define SCIDMA  4
#define SCIIRQ  5
#define SCITMA  6
#define SCITMB  7

typedef struct _scsp_state scsp_state;
struct _scsp_state
{
	DEV_DATA _devData;
	DEV_LOGGER logger;

	//int m_roffset;                /* offset in the region */
	//devcb_write8       m_irq_cb;  /* irq callback */
	//devcb_write_line   m_main_irq_cb;

	union
	{
		UINT16 data[0x30/2];
		UINT8 datab[0x30];
	} udata;

	SCSP_SLOT Slots[32];
	INT16 RINGBUF[128];
	UINT8 BUFPTR;
#if FM_DELAY
	INT16 DELAYBUF[FM_DELAY];
	UINT8 DELAYPTR;
#endif
	UINT8 *SCSPRAM;
	UINT32 SCSPRAM_LENGTH;
	UINT32 clock;
	UINT32 rate;

	//UINT32 IrqTimA;
	//UINT32 IrqTimBC;
	//UINT32 IrqMidi;

	UINT8 MidiOutW, MidiOutR;
	UINT8 MidiStack[32];
	UINT8 MidiW, MidiR;

	INT32 EG_TABLE[0x400];

	int LPANTABLE[0x10000];
	int RPANTABLE[0x10000];

	//int TimPris[3];
	//int TimCnt[3];

	// timers
	//emu_timer *timerA, *timerB, *timerC;

	// DMA stuff
	struct
	{
		UINT32 dmea;
		UINT16 drga;
		UINT16 dtlg;
		UINT8 dgate;
		UINT8 ddir;
	} dma;

	UINT16 mcieb;
	UINT16 mcipd;

	int ARTABLE[64], DRTABLE[64];

	SCSPDSP DSP;

	INT16 *RBUFDST;   //this points to where the sample will be stored in the RingBuf

	//LFO
	//int PLFO_TRI[256], PLFO_SQR[256], PLFO_SAW[256], PLFO_NOI[256];
	//int ALFO_TRI[256], ALFO_SQR[256], ALFO_SAW[256], ALFO_NOI[256];
	//int PSCALES[8][256];
	//int ASCALES[8][256];
};

/* TODO */
//#define dma_transfer_end  ((scsp_regs[0x24/2] & 0x10)>>4)|(((scsp_regs[0x26/2] & 0x10)>>4)<<1)|(((scsp_regs[0x28/2] & 0x10)>>4)<<2)

static const float SDLT[8]={-1000000.0f,-36.0f,-30.0f,-24.0f,-18.0f,-12.0f,-6.0f,0.0f};


static UINT8 BypassDSP = 0x01;

static int Get_AR(scsp_state *scsp,int base,int R)
{
	int Rate=base+(R<<1);
	if(Rate>63) Rate=63;
	if(Rate<0) Rate=0;
	return scsp->ARTABLE[Rate];
}

static int Get_DR(scsp_state *scsp,int base,int R)
{
	int Rate=base+(R<<1);
	if(Rate>63) Rate=63;
	if(Rate<0) Rate=0;
	return scsp->DRTABLE[Rate];
}

static void Compute_EG(scsp_state *scsp,SCSP_SLOT *slot)
{
	int octave=(OCT(slot)^8)-8;
	int rate;
	if(KRS(slot)!=0xf)
		rate=octave+2*KRS(slot)+((FNS(slot)>>9)&1);
	else
		rate=0; //rate=((FNS(slot)>>9)&1);

	slot->EG.volume=0x17F<<EG_SHIFT;
	slot->EG.AR=Get_AR(scsp,rate,AR(slot));
	slot->EG.D1R=Get_DR(scsp,rate,D1R(slot));
	slot->EG.D2R=Get_DR(scsp,rate,D2R(slot));
	slot->EG.RR=Get_DR(scsp,rate,RR(slot));
	slot->EG.DL=0x1f-DL(slot);
	slot->EG.EGHOLD=EGHOLD(slot);
}

static void SCSP_StopSlot(SCSP_SLOT *slot,int keyoff);

static int EG_Update(SCSP_SLOT *slot)
{
	switch(slot->EG.state)
	{
		case SCSP_ATTACK:
			slot->EG.volume+=slot->EG.AR;
			if(slot->EG.volume>=(0x3ff<<EG_SHIFT))
			{
				if (!LPSLNK(slot))
				{
					slot->EG.state=SCSP_DECAY1;
					if(slot->EG.D1R>=(1024<<EG_SHIFT)) //Skip SCSP_DECAY1, go directly to SCSP_DECAY2
						slot->EG.state=SCSP_DECAY2;
				}
				slot->EG.volume=0x3ff<<EG_SHIFT;
			}
			if(slot->EG.EGHOLD)
				return 0x3ff<<(SHIFT-10);
			break;
		case SCSP_DECAY1:
			slot->EG.volume-=slot->EG.D1R;
			if(slot->EG.volume<=0)
				slot->EG.volume=0;
			if(slot->EG.volume>>(EG_SHIFT+5)<=slot->EG.DL)
				slot->EG.state=SCSP_DECAY2;
			break;
		case SCSP_DECAY2:
			if(D2R(slot)==0)
				return (slot->EG.volume>>EG_SHIFT)<<(SHIFT-10);
			slot->EG.volume-=slot->EG.D2R;
			if(slot->EG.volume<=0)
				slot->EG.volume=0;

			break;
		case SCSP_RELEASE:
			slot->EG.volume-=slot->EG.RR;
			if(slot->EG.volume<=0)
			{
				slot->EG.volume=0;
				SCSP_StopSlot(slot,0);
				//slot->EG.volume=0x17F<<EG_SHIFT;
				//slot->EG.state=SCSP_ATTACK;
			}
			break;
		default:
			return 1<<SHIFT;
	}
	return (slot->EG.volume>>EG_SHIFT)<<(SHIFT-10);
}

static UINT32 SCSP_Step(SCSP_SLOT *slot)
{
	int octave=(OCT(slot)^8)-8+SHIFT-10;
	UINT32 Fn=FNS(slot)+(1 << 10);
	if (octave >= 0)
	{
		Fn<<=octave;
	}
	else
	{
		Fn>>=-octave;
	}

	return Fn;
}


static void Compute_LFO(SCSP_SLOT *slot)
{
	if(PLFOS(slot)!=0)
		LFO_ComputeStep(&(slot->PLFO),LFOF(slot),PLFOWS(slot),PLFOS(slot),0);
	if(ALFOS(slot)!=0)
		LFO_ComputeStep(&(slot->ALFO),LFOF(slot),ALFOWS(slot),ALFOS(slot),1);
}

static void SCSP_StartSlot(scsp_state *scsp, SCSP_SLOT *slot)
{
	UINT32 start_offset;

	slot->active=1;
	start_offset = PCM8B(slot) ? SA(slot) : SA(slot) & 0x7FFFE;
	slot->base=scsp->SCSPRAM + start_offset;
	slot->cur_addr=0;
	slot->nxt_addr=1<<SHIFT;
	slot->step=SCSP_Step(slot);
	Compute_EG(scsp,slot);
	slot->EG.state=SCSP_ATTACK;
	slot->EG.volume=0x17F<<EG_SHIFT;
	slot->Prev=0;
	slot->Backwards=0;

	Compute_LFO(slot);

//	printf("StartSlot[%p]: SA %x PCM8B %x LPCTL %x ALFOS %x STWINH %x TL %x EFSDL %x\n", slot, SA(slot), PCM8B(slot), LPCTL(slot), ALFOS(slot), STWINH(slot), TL(slot), EFSDL(slot));
}

static void SCSP_StopSlot(SCSP_SLOT *slot,int keyoff)
{
	if(keyoff /*&& slot->EG.state!=SCSP_RELEASE*/)
	{
		slot->EG.state=SCSP_RELEASE;
	}
	else
	{
		slot->active=0;
	}
	slot->udata.data[0]&=~0x800;
}

#define log_base_2(n) (log((double)(n))/log(2.0))

static void SCSP_Init(scsp_state *scsp, UINT32 clock)
{
	int i;

	SCSPDSP_Init(&scsp->DSP);

	scsp->clock = clock;
	scsp->rate = clock / 512;

	//scsp->IrqTimA = scsp->IrqTimBC = scsp->IrqMidi = 0;
	scsp->MidiR = scsp->MidiW = 0;
	scsp->MidiOutR = scsp->MidiOutW = 0;

	// get SCSP RAM
	scsp->SCSPRAM_LENGTH = 0x80000;	// 512 KB
	scsp->SCSPRAM = (unsigned char*)malloc(scsp->SCSPRAM_LENGTH);
	scsp->DSP.SCSPRAM_LENGTH = scsp->SCSPRAM_LENGTH / 2;
	scsp->DSP.SCSPRAM = (UINT16*)scsp->SCSPRAM;
	//scsp->SCSPRAM += scsp->roffset;

	//scsp->timerA = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(scsp_device::timerA_cb), this));
	//scsp->timerB = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(scsp_device::timerB_cb), this));
	//scsp->timerC = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(scsp_device::timerC_cb), this));

	for(i=0;i<0x400;++i)
	{
		float envDB=((float)(3*(i-0x3ff)))/32.0f;
		float scale=(float)(1<<SHIFT);
		scsp->EG_TABLE[i]=(INT32)(pow(10.0,envDB/20.0)*scale);
	}

	for(i=0;i<0x10000;++i)
	{
		int iTL =(i>>0x0)&0xff;
		int iPAN=(i>>0x8)&0x1f;
		int iSDL=(i>>0xD)&0x07;
		float TL;
		float SegaDB=0.0f;
		float fSDL;
		float PAN;
		float LPAN,RPAN;

		if(iTL&0x01) SegaDB-=0.4f;
		if(iTL&0x02) SegaDB-=0.8f;
		if(iTL&0x04) SegaDB-=1.5f;
		if(iTL&0x08) SegaDB-=3.0f;
		if(iTL&0x10) SegaDB-=6.0f;
		if(iTL&0x20) SegaDB-=12.0f;
		if(iTL&0x40) SegaDB-=24.0f;
		if(iTL&0x80) SegaDB-=48.0f;

		TL=powf(10.0f,SegaDB/20.0f);

		SegaDB=0;
		if(iPAN&0x1) SegaDB-=3.0f;
		if(iPAN&0x2) SegaDB-=6.0f;
		if(iPAN&0x4) SegaDB-=12.0f;
		if(iPAN&0x8) SegaDB-=24.0f;

		if((iPAN&0xf)==0xf) PAN=0.0f;
		else PAN=powf(10.0f,SegaDB/20.0f);

		if(iPAN<0x10)
		{
			LPAN=PAN;
			RPAN=1.0;
		}
		else
		{
			RPAN=PAN;
			LPAN=1.0;
		}

		if(iSDL)
			fSDL=powf(10.0f,(SDLT[iSDL])/20.0f);
		else
			fSDL=0.0;

		scsp->LPANTABLE[i]=FIX((4.0f*LPAN*TL*fSDL));
		scsp->RPANTABLE[i]=FIX((4.0f*RPAN*TL*fSDL));
	}

	scsp->ARTABLE[0]=scsp->DRTABLE[0]=0;    //Infinite time
	scsp->ARTABLE[1]=scsp->DRTABLE[1]=0;    //Infinite time
	for(i=2;i<64;++i)
	{
		double t,step,scale;
		t=ARTimes[i];   //In ms
		if(t!=0.0)
		{
			step=(1023*1000.0) / (44100.0*t);
			scale=(double) (1<<EG_SHIFT);
			scsp->ARTABLE[i]=(int) (step*scale);
		}
		else
			scsp->ARTABLE[i]=1024<<EG_SHIFT;

		t=DRTimes[i];   //In ms
		step=(1023*1000.0) / (44100.0*t);
		scale=(double) (1<<EG_SHIFT);
		scsp->DRTABLE[i]=(int) (step*scale);
	}

	// make sure all the slots are off
	for(i=0;i<32;++i)
	{
		scsp->Slots[i].slot=i;
		scsp->Slots[i].active=0;
		scsp->Slots[i].base=NULL;
		scsp->Slots[i].EG.state=SCSP_RELEASE;
	}

	LFO_Init();
	// no "pend"
	scsp->udata.data[0x20/2] = 0;
	//scsp->TimCnt[0] = 0xffff;
	//scsp->TimCnt[1] = 0xffff;
	//scsp->TimCnt[2] = 0xffff;
}

INLINE void SCSP_UpdateSlotReg(scsp_state *scsp,int s,int r)
{
	SCSP_SLOT *slot=scsp->Slots+s;
	int sl;
	switch(r&0x3f)
	{
		case 0:
		case 1:
			if(KEYONEX(slot))
			{
				for(sl=0;sl<32;++sl)
				{
					SCSP_SLOT *s2=scsp->Slots+sl;
					{
						if(KEYONB(s2) && s2->EG.state==SCSP_RELEASE/*&& !s2->active*/)
						{
							SCSP_StartSlot(scsp, s2);
						}
						if(!KEYONB(s2) /*&& s2->active*/)
						{
							SCSP_StopSlot(s2,1);
						}
					}
				}
				slot->udata.data[0]&=~0x1000;
			}
			break;
		case 0x10:
		case 0x11:
			slot->step=SCSP_Step(slot);
			break;
		case 0xA:
		case 0xB:
			slot->EG.RR=Get_DR(scsp,0,RR(slot));
			slot->EG.DL=0x1f-DL(slot);
			break;
		case 0x12:
		case 0x13:
			Compute_LFO(slot);
			break;
	}
}

INLINE void SCSP_UpdateReg(scsp_state *scsp, /*address_space &space,*/ int reg)
{
	switch(reg&0x3f)
	{
		case 0x0:
			// TODO: Make this work in VGMPlay
			//scsp->stream->set_output_gain(0,MVOL(scsp) / 15.0);
			//scsp->stream->set_output_gain(1,MVOL(scsp) / 15.0);
			break;
		case 0x2:
		case 0x3:
			{
				scsp->DSP.RBL = (8 * 1024) << RBL(scsp); // 8 / 16 / 32 / 64 kwords
				scsp->DSP.RBP = RBP(scsp);
			}
			break;
		case 0x6:
		case 0x7:
			scsp_midi_in(scsp, 0, scsp->udata.data[0x6/2]&0xff);
			break;
		case 8:
		case 9:
			/* Only MSLC could be written. */
			scsp->udata.data[0x8/2] &= 0xf800; /**< @todo Docs claims MSLC to be 0x7800, but Jikkyou Parodius doesn't agree. */
			break;
		case 0x12:
		case 0x13:
			//scsp->dma.dmea = (scsp->udata.data[0x12/2] & 0xfffe) | (scsp->dma.dmea & 0xf0000);
			break;
		case 0x14:
		case 0x15:
			//scsp->dma.dmea = ((scsp->udata.data[0x14/2] & 0xf000) << 4) | (scsp->dma.dmea & 0xfffe);
			//scsp->dma.drga = (scsp->udata.data[0x14/2] & 0x0ffe);
			break;
		case 0x16:
		case 0x17:
			//scsp->dma.dtlg = (scsp->udata.data[0x16/2] & 0x0ffe);
			//scsp->dma.ddir = (scsp->udata.data[0x16/2] & 0x2000) >> 13;
			//scsp->dma.dgate = (scsp->udata.data[0x16/2] & 0x4000) >> 14;
			//if(scsp->udata.data[0x16/2] & 0x1000) // dexe
			//	SCSP_exec_dma(space, scsp);
			break;
		case 0x18:
		case 0x19:
			break;
		case 0x1a:
		case 0x1b:
			break;
		case 0x1C:
		case 0x1D:
			break;
		case 0x1e: // SCIEB
		case 0x1f:
			break;
		case 0x20: // SCIPD
		case 0x21:
			break;
		case 0x22:  //SCIRE
		case 0x23:
			break;
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
			break;
		case 0x2a:
		case 0x2b:
			scsp->mcieb = scsp->udata.data[0x2a/2];

			//MainCheckPendingIRQ(scsp, 0);
			//if(scsp->mcieb & ~0x60)
			//	popmessage("SCSP MCIEB enabled %04x, contact MAMEdev",scsp->mcieb);
			break;
		case 0x2c:
		case 0x2d:
			//if(scsp->udata.data[0x2c/2] & 0x20)
			//	MainCheckPendingIRQ(scsp, 0x20);
			break;
		case 0x2e:
		case 0x2f:
			scsp->mcipd &= ~scsp->udata.data[0x2e/2];
			//MainCheckPendingIRQ(scsp, 0);
			break;

	}
}

static void SCSP_UpdateSlotRegR(scsp_state *scsp, int slot,int reg)
{
}

static void SCSP_UpdateRegR(scsp_state *scsp, int reg)
{
	switch(reg&0x3f)
	{
		case 4:
		case 5:
			{
				unsigned short v=scsp->udata.data[0x5/2];
				v&=0xff00;
				v|=scsp->MidiStack[scsp->MidiR];
				//scsp->irq_cb(scsp->IrqMidi, CLEAR_LINE);   // cancel the IRQ
				//logerror("Read %x from SCSP MIDI\n", v);
				if(scsp->MidiR!=scsp->MidiW)
				{
					++scsp->MidiR;
					scsp->MidiR&=31;
				}
				scsp->udata.data[0x4/2]=v;
			}
			break;
		case 8:
		case 9:
			{
				// MSLC     |  CA   |SGC|EG
				// f e d c b a 9 8 7 6 5 4 3 2 1 0
				unsigned char MSLC=(scsp->udata.data[0x8/2]>>11)&0x1f;
				SCSP_SLOT *slot=scsp->Slots + MSLC;
				unsigned int SGC = (slot->EG.state) & 3;
				unsigned int CA = (slot->cur_addr>>(SHIFT+12)) & 0xf;
				unsigned int EG = (0x1f - (slot->EG.volume>>(EG_SHIFT+5))) & 0x1f;
				/* note: according to the manual MSLC is write only, CA, SGC and EG read only.  */
				scsp->udata.data[0x8/2] =  /*(MSLC << 11) |*/ (CA << 7) | (SGC << 5) | EG;
			}
			break;

		case 0x18:
		case 0x19:
			break;

		case 0x1a:
		case 0x1b:
			break;

		case 0x1c:
		case 0x1d:
			break;

		case 0x2a:
		case 0x2b:
			scsp->udata.data[0x2a/2] = scsp->mcieb;
			break;

		case 0x2c:
		case 0x2d:
			scsp->udata.data[0x2c/2] = scsp->mcipd;
			break;
	}
}

static void SCSP_w16(void* info, UINT16 addr, UINT16 val)
{
	scsp_state *scsp = (scsp_state *)info;
	addr&=0xfffe;
	if(addr<0x400)
	{
		int slot=addr/0x20;
		addr&=0x1f;
		*((UINT16 *) (scsp->Slots[slot].udata.datab+(addr))) = val;
		SCSP_UpdateSlotReg(scsp,slot,addr&0x1f);
	}
	else if(addr<0x600)
	{
		if (addr < 0x430)
		{
			*((UINT16 *) (scsp->udata.datab+((addr&0x3f)))) = val;
			SCSP_UpdateReg(scsp, addr&0x3f);
		}
	}
	else if(addr<0x700)
		scsp->RINGBUF[(addr-0x600)/2]=val;
	else
	{
		//DSP
		if(addr<0x780)  //COEF
			*((UINT16 *) (scsp->DSP.COEF+(addr-0x700)/2))=val;
		else if(addr<0x7c0)
			*((UINT16 *) (scsp->DSP.MADRS+(addr-0x780)/2))=val;
		else if(addr<0x800) // MADRS is mirrored twice
			*((UINT16 *) (scsp->DSP.MADRS+(addr-0x7c0)/2))=val;
		else if(addr<0xC00)
		{
			*((UINT16 *) (scsp->DSP.MPRO+(addr-0x800)/2))=val;

			if(addr==0xBF0)
			{
				SCSPDSP_Start(&scsp->DSP);
			}
		}
	}
}

static UINT16 SCSP_r16(void* info, UINT16 addr)
{
	scsp_state *scsp = (scsp_state *)info;
	UINT16 v=0;
	addr&=0xffff;
	if(addr<0x400)
	{
		int slot=addr/0x20;
		addr&=0x1f;
		SCSP_UpdateSlotRegR(scsp, slot,addr&0x1f);
		v=*((UINT16 *) (scsp->Slots[slot].udata.datab+(addr)));
	}
	else if(addr<0x600)
	{
		if (addr < 0x430)
		{
			SCSP_UpdateRegR(scsp, addr&0x3f);
			v= *((UINT16 *) (scsp->udata.datab+((addr&0x3f))));
		}
	}
	else if(addr<0x700)
		v=scsp->RINGBUF[(addr-0x600)/2];
#if 1	// disabled by default until I get the DSP to work correctly
		// can be enabled using separate option
	else
	{
		//DSP
		if(addr<0x780)	//COEF
			v= *((UINT16 *) (scsp->DSP.COEF+(addr-0x700)/2));
		else if(addr<0x7c0)
			v= *((UINT16 *) (scsp->DSP.MADRS+(addr-0x780)/2));
		else if(addr<0x800)
			v= *((UINT16 *) (scsp->DSP.MADRS+(addr-0x7c0)/2));
		else if(addr<0xC00)
			v= *((UINT16 *) (scsp->DSP.MPRO+(addr-0x800)/2));
		else if(addr<0xE00)
		{
			if(addr & 2)
				v= scsp->DSP.TEMP[(addr >> 2) & 0x7f] & 0xffff;
			else
				v= scsp->DSP.TEMP[(addr >> 2) & 0x7f] >> 16;
		}
		else if(addr<0xE80)
		{
			if(addr & 2)
				v= scsp->DSP.MEMS[(addr >> 2) & 0x1f] & 0xffff;
			else
				v= scsp->DSP.MEMS[(addr >> 2) & 0x1f] >> 16;
		}
		else if(addr<0xEC0)
		{
			if(addr & 2)
				v= scsp->DSP.MIXS[(addr >> 2) & 0xf] & 0xffff;
			else
				v= scsp->DSP.MIXS[(addr >> 2) & 0xf] >> 16;
		}
		else if(addr<0xEE0)
			v= *((UINT16 *) (scsp->DSP.EFREG+(addr-0xec0)/2));
		else
		{
			/**!
			@todo Kyuutenkai reads from 0xee0/0xee2, it's tied with EXTS register(s) also used for CD-Rom Player equalizer.
			This port is actually an external parallel port, directly connected from the CD Block device, hence code is a bit of an hack.
			*/
			emu_logf(&scsp->logger, DEVLOG_DEBUG, "Reading from EXTS register %08x\n",addr);
			if(addr<0xEE4)
				v = *((UINT16 *) (scsp->DSP.EXTS+(addr-0xee0)/2));
		}
	}
#endif
	return v;
}


#define REVSIGN(v) ((~v)+1)

INLINE INT32 SCSP_UpdateSlot(scsp_state *scsp, SCSP_SLOT *slot)
{
	INT32 sample;
	int step=slot->step;
	UINT32 addr1,addr2,addr_select;                                   // current and next sample addresses
	UINT32 *addr[2]      = {&addr1, &addr2};                          // used for linear interpolation
	UINT32 *slot_addr[2] = {&(slot->cur_addr), &(slot->nxt_addr)};    //

	if (SSCTL(slot) == 3) // manual says cannot be used
	{
		emu_logf(&scsp->logger, DEVLOG_WARN, "Invaild SSCTL setting at slot %02x\n", slot->slot);
		return 0;
	}

	if(PLFOS(slot)!=0)
	{
		step=step*PLFO_Step(&(slot->PLFO));
		step>>=SHIFT;
	}

	if(PCM8B(slot))
	{
		addr1=slot->cur_addr>>SHIFT;
		addr2=slot->nxt_addr>>SHIFT;
	}
	else
	{
		addr1=(slot->cur_addr>>(SHIFT-1))&~1;
		addr2=(slot->nxt_addr>>(SHIFT-1))&~1;
	}

	if(MDL(slot)!=0 || MDXSL(slot)!=0 || MDYSL(slot)!=0)
	{
		INT32 smp=(scsp->RINGBUF[(scsp->BUFPTR+MDXSL(slot))&63]+scsp->RINGBUF[(scsp->BUFPTR+MDYSL(slot))&63])/2;

		smp<<=0xA; // associate cycle with 1024
		smp>>=0x1A-MDL(slot); // ex. for MDL=0xF, sample range corresponds to +/- 64 pi (32=2^5 cycles) so shift by 11 (16-5 == 0x1A-0xF)
		if(!PCM8B(slot)) smp<<=1;

		addr1+=smp; addr2+=smp;
	}

#ifdef VGM_BIG_ENDIAN
#define READ_BE16(ptr)	(*(INT16*)ptr)
#else
#define READ_BE16(ptr)	(INT16)(((ptr)[0] << 8) | (ptr)[1])
#endif
	if (SSCTL(slot) == 0) // External DRAM data
	{
		if (PCM8B(slot)) //8 bit signed
		{
			INT16 p1=(INT8)scsp->SCSPRAM[SA(slot)+addr1]<<8;
			INT16 p2=(INT8)scsp->SCSPRAM[SA(slot)+addr2]<<8;
			INT32 fpart=slot->cur_addr&((1<<SHIFT)-1);
			INT32 s=(int)p1*((1<<SHIFT)-fpart)+(int)p2*fpart;
			sample=(s>>SHIFT);
		}
		else    //16 bit signed
		{
			const UINT8 *pp1 = &scsp->SCSPRAM[SA(slot)+addr1];
			const UINT8 *pp2 = &scsp->SCSPRAM[SA(slot)+addr2];
			INT16 p1 = READ_BE16(pp1);
			INT16 p2 = READ_BE16(pp2);
			INT32 fpart=slot->cur_addr&((1<<SHIFT)-1);
			INT32 s=(int)p1*((1<<SHIFT)-fpart)+(int)p2*fpart;
			sample=(s>>SHIFT);
		}
	}
	else if (SSCTL(slot) == 1)  // Internally generated data (Noise)
		sample = (INT16)(rand() & 0xffff); // Unknown algorithm
	else //if (SSCTL(slot) >= 2)  // Internally generated data (All 0)
		sample = 0;

	if(SBCTL(slot)&0x1)
		sample ^= 0x7FFF;
	if(SBCTL(slot)&0x2)
		sample = (INT16)(sample^0x8000);

	if(slot->Backwards)
		slot->cur_addr-=step;
	else
		slot->cur_addr+=step;
	slot->nxt_addr=slot->cur_addr+(1<<SHIFT);

	addr1=slot->cur_addr>>SHIFT;
	addr2=slot->nxt_addr>>SHIFT;

	if(addr1>=LSA(slot) && !(slot->Backwards))
	{
		if(LPSLNK(slot) && slot->EG.state==SCSP_ATTACK)
			slot->EG.state = SCSP_DECAY1;
	}

	for (addr_select=0;addr_select<2;addr_select++)
	{
		INT32 rem_addr;
		switch(LPCTL(slot))
		{
		case 0: //no loop
			if(*addr[addr_select]>=LSA(slot) && *addr[addr_select]>=LEA(slot))
			{
				//slot->active=0;
				SCSP_StopSlot(slot,0);
			}
			break;
		case 1: //normal loop
			if(*addr[addr_select]>=LEA(slot))
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot)<<SHIFT);
				*slot_addr[addr_select]=(LSA(slot)<<SHIFT) + rem_addr;
			}
			break;
		case 2: //reverse loop
			if((*addr[addr_select]>=LSA(slot)) && !(slot->Backwards))
			{
				rem_addr = *slot_addr[addr_select] - (LSA(slot)<<SHIFT);
				*slot_addr[addr_select]=(LEA(slot)<<SHIFT) - rem_addr;
				slot->Backwards=1;
			}
			else if((*addr[addr_select]<LSA(slot) || (*slot_addr[addr_select]&0x80000000)) && slot->Backwards)
			{
				rem_addr = (LSA(slot)<<SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select]=(LEA(slot)<<SHIFT) - rem_addr;
			}
			break;
		case 3: //ping-pong
			if(*addr[addr_select]>=LEA(slot)) //reached end, reverse till start
			{
				rem_addr = *slot_addr[addr_select] - (LEA(slot)<<SHIFT);
				*slot_addr[addr_select]=(LEA(slot)<<SHIFT) - rem_addr;
				slot->Backwards=1;
			}
			else if((*addr[addr_select]<LSA(slot) || (*slot_addr[addr_select]&0x80000000)) && slot->Backwards)//reached start or negative
			{
				rem_addr = (LSA(slot)<<SHIFT) - *slot_addr[addr_select];
				*slot_addr[addr_select]=(LSA(slot)<<SHIFT) + rem_addr;
				slot->Backwards=0;
			}
			break;
		}
	}

	if(!SDIR(slot))
	{
		if(ALFOS(slot)!=0)
		{
			sample=sample*ALFO_Step(&(slot->ALFO));
			sample>>=SHIFT;
		}

		if(slot->EG.state==SCSP_ATTACK)
			sample=(sample*EG_Update(slot))>>SHIFT;
		else
			sample=(sample*scsp->EG_TABLE[EG_Update(slot)>>(SHIFT-10)])>>SHIFT;
	}

	if(!STWINH(slot))
	{
		if(!SDIR(slot))
		{
			unsigned short Enc=((TL(slot))<<0x0)|(0x7<<0xd);
			*scsp->RBUFDST=(sample*scsp->LPANTABLE[Enc])>>(SHIFT+1);
		}
		else
		{
			unsigned short Enc=(0<<0x0)|(0x7<<0xd);
			*scsp->RBUFDST=(sample*scsp->LPANTABLE[Enc])>>(SHIFT+1);
		}
	}

	return sample;
}

static void SCSP_DoMasterSamples(void* info, UINT32 nsamples, DEV_SMPL **outputs)
{
	scsp_state *scsp = (scsp_state *)info;
	DEV_SMPL *bufr,*bufl;
	UINT32 sl, s, i;

	bufl = outputs[0];
	bufr = outputs[1];

	if (scsp->SCSPRAM == NULL)
	{
		memset(bufl, 0, nsamples * sizeof(DEV_SMPL));
		memset(bufr, 0, nsamples * sizeof(DEV_SMPL));
		return;
	}

	for(s=0;s<nsamples;++s)
	{
		DEV_SMPL smpl, smpr;

		smpl = smpr = 0;

		for(sl=0;sl<32;++sl)
		{
#if FM_DELAY
			scsp->RBUFDST=scsp->DELAYBUF+scsp->DELAYPTR;
#else
			scsp->RBUFDST=scsp->RINGBUF+scsp->BUFPTR;
#endif
			if(scsp->Slots[sl].active && ! scsp->Slots[sl].Muted)
			{
				SCSP_SLOT *slot=scsp->Slots+sl;
				unsigned short Enc;
				signed int sample;

				sample=SCSP_UpdateSlot(scsp, slot);

				if (! BypassDSP)
				{
					Enc=((TL(slot))<<0x0)|((IMXL(slot))<<0xd);
					SCSPDSP_SetSample(&scsp->DSP,(sample*scsp->LPANTABLE[Enc])>>(SHIFT-2),ISEL(slot),IMXL(slot));
				}
				Enc=((TL(slot))<<0x0)|((DIPAN(slot))<<0x8)|((DISDL(slot))<<0xd);
				{
					smpl+=(sample*scsp->LPANTABLE[Enc])>>SHIFT;
					smpr+=(sample*scsp->RPANTABLE[Enc])>>SHIFT;
				}
			}

#if FM_DELAY
			scsp->RINGBUF[(scsp->BUFPTR+64-(FM_DELAY-1))&63] = scsp->DELAYBUF[(scsp->DELAYPTR+FM_DELAY-(FM_DELAY-1))%FM_DELAY];
#endif
			++scsp->BUFPTR;
			scsp->BUFPTR&=63;
#if FM_DELAY
			++scsp->DELAYPTR;
			if(scsp->DELAYPTR>FM_DELAY-1) scsp->DELAYPTR=0;
#endif
		}

		if (! BypassDSP)
		{
			SCSPDSP_Step(&scsp->DSP);

			for(i=0;i<16;++i)
			{
				SCSP_SLOT *slot=scsp->Slots+i;
				if(EFSDL(slot))
				{
					unsigned short Enc=((EFPAN(slot))<<0x8)|((EFSDL(slot))<<0xd);
					smpl+=(scsp->DSP.EFREG[i]*scsp->LPANTABLE[Enc])>>SHIFT;
					smpr+=(scsp->DSP.EFREG[i]*scsp->RPANTABLE[Enc])>>SHIFT;
				}
			}

			for(i=0;i<2;++i)
			{
				SCSP_SLOT *slot=scsp->Slots+i+16; // 100217, 100237 EFSDL, EFPAN for EXTS0/1
				if(EFSDL(slot))
				{
					UINT16 Enc;
					scsp->DSP.EXTS[i] = 0; //scsp->exts[i][s];
					Enc=((EFPAN(slot))<<0x8)|((EFSDL(slot))<<0xd);
					smpl+=(scsp->DSP.EXTS[i]*scsp->LPANTABLE[Enc])>>SHIFT;
					smpr+=(scsp->DSP.EXTS[i]*scsp->RPANTABLE[Enc])>>SHIFT;
				}
			}
		}

		if (DAC18B(scsp))
		{
			smpl = CLIP18(smpl);
			smpr = CLIP18(smpr);
		}
		else
		{
			smpl = CLIP16(smpl>>2);
			smpr = CLIP16(smpr>>2);
		}
		bufl[s] = smpl;
		bufr[s] = smpr;
	}
}

static UINT8 device_start_scsp(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	scsp_state *scsp;

	scsp = (scsp_state *)calloc(1, sizeof(scsp_state));
	if (scsp == NULL)
		return 0xFF;

	// init the emulation
	SCSP_Init(scsp, cfg->clock);

	scsp_set_mute_mask(scsp, 0x00000000);

	scsp->_devData.chipInf = scsp;
	INIT_DEVINF(retDevInf, &scsp->_devData, scsp->rate, &devDef);

	return 0x00;
}

static void device_stop_scsp(void* info)
{
	scsp_state *scsp = (scsp_state *)info;
	
	free(scsp->SCSPRAM);
	free(scsp);
	
	return;
}

static void device_reset_scsp(void* info)
{
	scsp_state *scsp = (scsp_state *)info;
	int i;
	
	// make sure all the slots are off
	for(i=0;i<32;++i)
	{
		scsp->Slots[i].slot=i;
		scsp->Slots[i].active=0;
		scsp->Slots[i].base=NULL;
		scsp->Slots[i].EG.state=SCSP_RELEASE;
	}
	
	SCSPDSP_Init(&scsp->DSP);
	scsp->DSP.SCSPRAM_LENGTH = scsp->SCSPRAM_LENGTH / 2;
	scsp->DSP.SCSPRAM = (UINT16*)scsp->SCSPRAM;
	
	return;
}


static UINT8 scsp_r8(void* info, UINT16 offset)
{
	UINT16 val = SCSP_r16(info, offset & 0xFFFE);

	if (offset & 1)
		return (UINT8)(val >> 0);
	else
		return (UINT8)(val >> 8);
}

static void scsp_w8(void* info, UINT16 offset, UINT8 data)
{
	scsp_state *scsp = (scsp_state *)info;
	UINT16 tmp;

	tmp = SCSP_r16(scsp, offset & 0xFFFE);
	if (offset & 1)
		tmp = (tmp & 0xFF00) | (data << 0);
	else
		tmp = (tmp & 0x00FF) | (data << 8);
	SCSP_w16(scsp, offset & 0xFFFE, tmp);
}

static void scsp_midi_in(void* info, UINT8 offset, UINT8 data)
{
	scsp_state *scsp = (scsp_state *)info;

//	printf("scsp_midi_in: %02x\n", data);

	scsp->MidiStack[scsp->MidiW++]=data;
	scsp->MidiW &= 31;

	//CheckPendingIRQ(scsp);
}

static UINT8 scsp_midi_out_r(void* info, UINT8 offset)
{
	scsp_state *scsp = (scsp_state *)info;
	UINT8 val;

	val=scsp->MidiStack[scsp->MidiR++];
	scsp->MidiR&=31;
	return val;
}


static void scsp_write_ram(void* info, UINT32 offset, UINT32 length, const UINT8* data)
{
	scsp_state *scsp = (scsp_state *)info;
	
	if (offset >= scsp->SCSPRAM_LENGTH)
		return;
	if (offset + length > scsp->SCSPRAM_LENGTH)
		length = scsp->SCSPRAM_LENGTH - offset;
	
	memcpy(scsp->SCSPRAM + offset, data, length);
	
	return;
}


static void scsp_set_mute_mask(void* info, UINT32 MuteMask)
{
	scsp_state *scsp = (scsp_state *)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 32; CurChn ++)
		scsp->Slots[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}

static void scsp_set_options(void* info, UINT32 Flags)
{
	BypassDSP = (Flags & 0x01) >> 0;
	
	return;
}

static void scsp_set_log_cb(void* info, DEVCB_LOG func, void* param)
{
	scsp_state *scsp = (scsp_state *)info;
	dev_logger_set(&scsp->logger, scsp, func, param);
	return;
}
