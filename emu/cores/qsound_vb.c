// QSound sound emulation
// Valley Bell, 2018
// ported from a disassembly of the QSound DSP program ROM dl-1425.bin
#include <stdlib.h>
#include <string.h>	// for memset

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "qsound_vb.h"


typedef struct qsound_chip QSOUND_CHIP;

static void qsoundv_update(void* param, UINT32 samples, DEV_SMPL** outputs);
static UINT8 device_start_qsound_vb(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void device_stop_qsound_vb(void* info);
static void device_reset_qsound_vb(void* info);

static UINT8 qsoundv_r(void* info, UINT8 offset);
static void qsoundv_w(void* info, UINT8 offset, UINT8 data);
static void qsoundv_write_data(void* info, UINT8 address, UINT16 data);

static void qsoundv_alloc_rom(void* info, UINT32 memsize);
static void qsoundv_write_rom(void* info, UINT32 offset, UINT32 length, const UINT8* data);
static void qsoundv_set_mute_mask(void* info, UINT32 MuteMask);


static const INT16* get_filter_table_1(UINT16 offset);
static const INT16* get_filter_table_2(UINT16 offset);
static INT16 dsp_get_sample(QSOUND_CHIP* chip, UINT16 bank, UINT16 ofs);
INLINE void INC_MODULO(UINT16* reg, UINT16 rb, UINT16 re);
INLINE INT32 DSP_ROUND(INT32 value);
static void dsp_do_update_step(QSOUND_CHIP* chip);
static void dsp_update_delay(QSOUND_CHIP* chip);
static void dsp_update_test(QSOUND_CHIP* chip);	// ROM: 0018
static void dsp_copy_filter_data_1(QSOUND_CHIP* chip);	// ROM: 0039
static void dsp_copy_filter_data_2(QSOUND_CHIP* chip);	// ROM: 004F
static void dsp_init1(QSOUND_CHIP* chip);	// ROM: 0288
static void dsp_init2(QSOUND_CHIP* chip);	// ROM: 061A
static void dsp_filter_recalc(QSOUND_CHIP* chip, UINT16 refreshFlagAddr);	// ROM: 05DD / 099B
static void dsp_update_1(QSOUND_CHIP* chip);	// ROM: 0314
static void dsp_update_2(QSOUND_CHIP* chip);	// ROM: 06B2
static void dsp_filter_a(QSOUND_CHIP* chip, UINT16 i1);	// ROM: 0314/036F/03CA
static void dsp_filter_b(QSOUND_CHIP* chip, UINT16 i1);	// ROM: 0425/0470/04B4
static void dsp_sample_calc_1(QSOUND_CHIP* chip);	// ROM: 0504
static void dsp_sample_calc_2(QSOUND_CHIP* chip);	// ROM: 08A2


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, qsoundv_w},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, qsoundv_r},
	{RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D16, 0, qsoundv_write_data},
	{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, qsoundv_write_rom},
	{RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, qsoundv_alloc_rom},
	{0x00, 0x00, 0, NULL}
};
/*static*/ DEV_DEF devDef_QSound_VB =
{
	"QSound", "Valley Bell", FCC_VBEL,
	
	device_start_qsound_vb,
	device_stop_qsound_vb,
	device_reset_qsound_vb,
	qsoundv_update,
	
	NULL,	// SetOptionBits
	qsoundv_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};


typedef void (*UPDATE_FUNC)(QSOUND_CHIP* chip);
struct qsound_chip
{
	DEV_DATA _devData;
	
	UINT16 ram[0x800];	// DSP RAM, 2048 words, 16-bit each
	
	UINT8* romData;
	UINT32 romSize;
	UINT32 romMask;
	
	INT16 testInc;	// test mode increment (Y register)
	INT16 testOut;	// test mode output value (A0 register)
	UINT16 dataLatch;
	UINT8 busyState;
	INT16 out[2];
	
	UINT16 dspRoutine;	// offset of currently running update routine
	UINT8 dspRtStep;	// each routine outputs 6 samples before restarting, this indicates the current sample
	UPDATE_FUNC updateFunc;
	
	UINT16 muteMask;
};


// ---- routine offsets ----
// The routine to be executed can be selected externally by writing to OFS_ROUTINE.
//
// initialization routines
#define DSPRT_INIT1		0x0288	// default initialization routine
#define DSPRT_INIT2		0x061A	// alternate initialization routine
// refresh filter data, then start update
#define DSPRT_DO_TEST1	0x000C	// test routine - low-frequency sawtooth
#define DSPRT_DO_TEST2	0x000F	// test routine - high-frequency sawtooth
#define DSPRT_DO_UPD1	0x0039
#define DSPRT_DO_UPD2	0x004F
// update routines
#define DSPRT_TEST		0x0018	// test routine, outputs a sawtooth wave, doesn't do any additonal processing
#define DSPRT_UPDATE1	0x0314	// default update routine
#define DSPRT_UPDATE2	0x06B2	// alternate update routine


// ---- RAM offsets ----
#define OFS_CHANNEL_MEM		0x000	// 000-07F, 16 channels, 8 bytes per channel
#define OFS_CH_BANK			0x000
#define OFS_CH_CUR_ADDR		0x001
#define OFS_CH_RATE			0x002
#define OFS_CH_FRAC			0x003
#define OFS_CH_LOOP_LEN		0x004
#define OFS_CH_END_ADDR		0x005
#define OFS_CH_VOLUME		0x006
#define OFS_PAN_POS			0x080	// 080-092, 16+3 channels, offset into DSP ROM pan table for left speaker
#define OFS_REVERB_VOL		0x093
#define OFS_CH_REVERB		0x0BA	// 0BA-0C9, 16 channels
#define OFS_DELAYBUF_END	0x0D9
#define OFS_FILTER_REFRESH	0x0E2	// set to 1 to request recalculation of filter data
#define OFS_ROUTINE			0x0E3	// offset of DSP ROM sample processing routine
#define OFS_PAN_RPOS		0x0F7	// 0F7-109, 16+3 channels, offset cache for right speaker (== RAM[OFS_PAN_POS]+2*98)
#define OFS_CH_SMPLDATA		0x10A	// 10A-11C, 16+3 channels, caches (sampleData * channelVolume)
#define OFS_DELAYBUF_POS	0x11F

#define OFS_BUF_12D			0x12D
#define OFS_BUF_160			0x160
#define OFS_BUF_193			0x193
#define OFS_BUF_1C6			0x1C6

#define OFS_M1_BUF_1F9		0x1F9
#define OFS_M1_BUF_257		0x257
#define OFS_M1_FILTER1_TBL	0x2B5
#define OFS_M1_FILTER2_TBL	0x314
#define OFS_M1_BUF_373		0x373
#define OFS_M1_DELAY_BUFFER	0x554

#define OFS_M2_BUF_1FB		0x1FB
#define OFS_M2_BUF_227		0x227
#define OFS_M2_BUF_253		0x253
#define OFS_M2_BUF_27E		0x27E
#define OFS_M2_FILTER1A_TBL	0x2A9
#define OFS_M2_FILTER1B_TBL	0x2D6
#define OFS_M2_FILTER2A_TBL	0x302
#define OFS_M2_FILTER2B_TBL	0x32F
#define OFS_M2_BUF_35B		0x35B
#define OFS_M2_DELAY_BUFFER	0x53C


// ---- lookup tables ----
#define PANTBL_LEFT_OUTPUT	0
#define PANTBL_LEFT_FILTER	1
#define PANTBL_RIGHT_OUTPUT	2
#define PANTBL_RIGHT_FILTER	3
static const INT16 PAN_TABLES[4][0x62] =
{
	{	// left channel output (ROM: 0110)
		0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
		0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
		0xC000, 0xC666, 0xCCCD, 0xD28F, 0xD70A, 0xDC29, 0xDEB8, 0xE3D7,
		0xE7AE, 0xEB96, 0xEE14, 0xF148, 0xF333, 0xF571, 0xF7AE, 0xF8F6,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		
		0xC005, 0xC02E, 0xC07F, 0xC0F9, 0xC19B, 0xC264, 0xC355, 0xC46D,
		0xC5AA, 0xC70C, 0xC893, 0xCA3D, 0xCC09, 0xCDF6, 0xD004, 0xD22F,
		0xD22F, 0xD478, 0xD6DD, 0xD95B, 0xDBF3, 0xDEA1, 0xE164, 0xE43B,
		0xE724, 0xEA1C, 0xED23, 0xF035, 0xF352, 0xF676, 0xF9A1, 0xFCCF,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000
	},
	{	// left channel filter (ROM: 0172)
		0x0000, 0xF99A, 0xF852, 0xF666, 0xF47B, 0xF28F, 0xF000, 0xEDC3,
		0xECCD, 0xEC00, 0xEA8F, 0xE800, 0xE28F, 0xDD81, 0xDB85, 0xD99A,
		0xD800, 0xD7AE, 0xD70A, 0xD6B8, 0xD666, 0xD1EC, 0xD000, 0xD000,
		0xCF0A, 0xCE98, 0xCE14, 0xCDE3, 0xCD71, 0xCCCD, 0xCB96, 0xC8F6,
		0xC000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000
	},
	{	// right channel output (ROM: 01D4)
		0x0000, 0xF8F6, 0xF7AE, 0xF571, 0xF333, 0xF148, 0xEE14, 0xEB96,
		0xE7AE, 0xE3D7, 0xDEB8, 0xDC29, 0xD70A, 0xD28F, 0xCCCD, 0xC666,
		0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
		0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000, 0xC000,
		0xC000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		
		0x0000, 0xFCCF, 0xF9A1, 0xF676, 0xF352, 0xF035, 0xED23, 0xEA1C,
		0xE724, 0xE43B, 0xE164, 0xDEA1, 0xDBF3, 0xD95B, 0xD6DD, 0xD478,
		0xD22F, 0xD22F, 0xD004, 0xCDF6, 0xCC09, 0xCA3D, 0xC893, 0xC70C,
		0xC5AA, 0xC46D, 0xC355, 0xC264, 0xC19B, 0xC0F9, 0xC07F, 0xC02E,
		0xC005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000
	},
	{	// right channel filter (ROM: 0236)
		0xC000, 0xC8F6, 0xCB96, 0xCCCD, 0xCD71, 0xCDE3, 0xCE14, 0xCE98,
		0xCF0A, 0xD000, 0xD000, 0xD1EC, 0xD666, 0xD6B8, 0xD70A, 0xD7AE,
		0xD800, 0xD99A, 0xDB85, 0xDD81, 0xE28F, 0xE800, 0xEA8F, 0xEC00,
		0xECCD, 0xEDC3, 0xF000, 0xF28F, 0xF47B, 0xF666, 0xF852, 0xF99A,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000
	}
};

static const INT16 LUT_09D2[] =
{	// ROM: 09D2
	0x0000, 0x3305, 0xFDC3, 0x023D, 0xC6B7, 0x778A, 0xFB86, 0x047A, 0x04F0, 0x0000
};

static const INT16 LUT_09DC[] =
{	// ROM: 09DC
	0x009A, 0x009A, 0x0080, 0x0066, 0x004D, 0x003A, 0x003A, 0x003A,
	// ROM: 09E4
	0x003A, 0x003A, 0x003A, 0x003A, 0x004D, 0x0066, 0x0080, 0x009A,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const INT16 FILTER_LUT_MODE1[5][95] =
{
	{	// ROM: 0D53
		0x0000, 0x0000, 0x0000, 0x0006, 0x002C, 0xFFE8, 0xFFCB, 0xFFF6,
		0x003B, 0xFFD8, 0xFFE5, 0x0001, 0x0027, 0xFFE5, 0x0038, 0x007F,
		0x00AE, 0x0024, 0xFFF3, 0x0031, 0x00D4, 0x008E, 0x008F, 0xFFB7,
		0xFFEC, 0x0042, 0xFF94, 0xFF8B, 0xFE71, 0xFEF7, 0xFE78, 0xFDC7,
		0xFE27, 0xFFB9, 0x005F, 0xFEC1, 0xFF26, 0xFF1A, 0x014B, 0x027E,
		0x01C1, 0x01DD, 0xFF4C, 0x0214, 0x0453, 0x02EE, 0x26AB, 0x0EF4,
		0xF68E, 0x042F, 0xFF50, 0x00BF, 0xFE51, 0x0040, 0x0075, 0xFF6A,
		0xFEEE, 0xFF9F, 0xFF12, 0x00A5, 0x00A6, 0x00FA, 0xFFED, 0x0004,
		0x0025, 0x00CC, 0x00BA, 0xFFFA, 0x008C, 0xFFB3, 0xFFFF, 0x0001,
		0x0012, 0xFFF6, 0xFF69, 0xFF6B, 0xFF99, 0xFFF7, 0x0037, 0x0017,
		0xFF9A, 0xFF9F, 0xFFF5, 0x000D, 0xFFD0, 0xFFE5, 0x0005, 0x0012,
		0xFFC3, 0xFFE2, 0x0040, 0x0048, 0x0000, 0x0000, 0x0000
	},
	{	// ROM: 0DB2
		0x0000, 0x0000, 0x0000, 0x0055, 0x0018, 0xFFB4, 0xFF85, 0xFFAA,
		0xFFE3, 0xFFF2, 0xFFEC, 0xFFF9, 0x0006, 0xFFE4, 0xFFA9, 0xFFA7,
		0xFFFB, 0x0064, 0x009A, 0x00A0, 0x0096, 0x0076, 0x0029, 0xFFD0,
		0xFFB2, 0xFFE9, 0x003B, 0x0053, 0xFFFE, 0xFF50, 0xFEB3, 0xFEA8,
		0xFF35, 0xFFBE, 0xFFD9, 0x0002, 0x00E0, 0x01EF, 0x01EF, 0x0118,
		0x01B0, 0x053C, 0x09B3, 0x1501, 0x0771, 0x0292, 0x0000, 0x0061,
		0x015B, 0x011D, 0x0023, 0xFFA1, 0xFFB2, 0xFFAE, 0xFF69, 0xFF40,
		0xFF55, 0xFF6B, 0xFF6D, 0xFF8F, 0xFFEA, 0x0047, 0x0076, 0x0081,
		0x007F, 0x006E, 0x0047, 0x001F, 0x0014, 0x0024, 0x002E, 0x0017,
		0xFFE5, 0xFFC1, 0xFFCB, 0xFFEB, 0xFFED, 0xFFC4, 0xFFA4, 0xFFBB,
		0xFFF4, 0x0019, 0x001D, 0x001E, 0x0028, 0x0029, 0x001D, 0x001E,
		0x002E, 0x0027, 0xFFF1, 0xFFB6, 0x0000, 0x0000, 0x0000
	},
	{	// ROM: 0E11
		0x0000, 0x0000, 0x0000, 0x0017, 0x002A, 0x002F, 0x001D, 0x000A,
		0x0002, 0xFFF2, 0xFFCA, 0xFFA4, 0xFFA3, 0xFFBA, 0xFFC0, 0xFFB3,
		0xFFC7, 0x0012, 0x005E, 0x0071, 0x0057, 0x0045, 0x0043, 0x0032,
		0x0019, 0x001D, 0x003A, 0x003E, 0x0018, 0xFFD9, 0xFF7D, 0xFF00,
		0xFEBB, 0xFF16, 0xFFD3, 0x003A, 0x004E, 0x00DF, 0x01E5, 0x01F0,
		0x007F, 0x0006, 0x0359, 0x08EB, 0x0A7B, 0x1340, 0x0530, 0x0084,
		0x004F, 0x013A, 0x00BD, 0xFFB0, 0xFFA6, 0x0023, 0xFFEB, 0xFF46,
		0xFF3D, 0xFF9D, 0xFF78, 0xFEFE, 0xFF43, 0x0052, 0x0101, 0x00B9,
		0x0035, 0x0029, 0x0054, 0x0044, 0x0026, 0x003F, 0x004D, 0x000E,
		0xFFC4, 0xFFB9, 0xFFB9, 0xFF88, 0xFF69, 0xFFAC, 0x000E, 0x001D,
		0xFFF8, 0x0007, 0x0042, 0x0045, 0x000C, 0xFFFD, 0x0036, 0x005C,
		0x0034, 0xFFFA, 0xFFF1, 0xFFFE, 0x0000, 0x0000, 0x0000
	},
	{	// ROM: 0E70
		0x0000, 0x0000, 0x0000, 0x0002, 0xFFE4, 0xFFDB, 0xFFEF, 0x0000,
		0xFFF7, 0xFFEA, 0xFFFD, 0x0023, 0x0034, 0x0027, 0x0014, 0x0007,
		0xFFFA, 0x0002, 0x0037, 0x0079, 0x0081, 0x0043, 0x0008, 0x0001,
		0x0009, 0xFFFA, 0xFFF0, 0x0010, 0x0042, 0x0060, 0x0076, 0x0082,
		0x004B, 0xFFD1, 0xFFA4, 0x002B, 0x00DF, 0x00EF, 0x0097, 0x00DB,
		0x01B8, 0x01DB, 0x00E2, 0x00CE, 0x03AC, 0x0834, 0x0A67, 0x1374,
		0x0361, 0x0031, 0xFFDF, 0x00BA, 0x00E7, 0x0067, 0x002A, 0x0072,
		0x00BF, 0x00B8, 0x0074, 0x001D, 0xFFD1, 0xFFB8, 0xFFEB, 0x003C,
		0x0060, 0x0044, 0x001F, 0x0020, 0x003F, 0x0057, 0x004C, 0x0027,
		0x0007, 0x000E, 0x0037, 0x0055, 0x0043, 0x0012, 0xFFF4, 0xFFFD,
		0x0015, 0x0022, 0x001D, 0x0006, 0xFFE5, 0xFFCF, 0xFFDB, 0xFFFE,
		0x0010, 0x0000, 0xFFEB, 0xFFF0, 0x0000, 0x0000, 0x0000
	},
	{	// ROM: 0ECF
		0x0000, 0x0000, 0x0000, 0x0030, 0x0007, 0xFFEA, 0xFFE3, 0xFFF6,
		0x0018, 0x0036, 0x003B, 0x001D, 0xFFDC, 0xFF8B, 0xFF47, 0xFF2B,
		0xFF47, 0xFF9D, 0x000D, 0x005A, 0x0053, 0x0018, 0xFFFB, 0x0017,
		0x0035, 0x002F, 0x0026, 0x0038, 0x0043, 0x0039, 0x004B, 0x006B,
		0x0010, 0xFF0E, 0xFE48, 0xFE9D, 0xFF88, 0xFFDF, 0xFFD1, 0x0098,
		0x01F5, 0x01D8, 0xFFC7, 0xFEDC, 0x0220, 0x0791, 0x08E5, 0x1801,
		0x04D8, 0x0099, 0x002F, 0x00C8, 0x0098, 0x0024, 0x0040, 0x0086,
		0x004A, 0xFFAE, 0xFF30, 0xFEF6, 0xFEF4, 0xFF44, 0xFFD6, 0x0041,
		0x004A, 0x0038, 0x0059, 0x0085, 0x0072, 0x002C, 0xFFFD, 0xFFFF,
		0x0011, 0x001D, 0x001D, 0xFFFE, 0xFFB4, 0xFF64, 0xFF45, 0xFF69,
		0xFFAB, 0xFFE1, 0xFFFB, 0x0007, 0x0014, 0x0020, 0x0018, 0xFFFB,
		0xFFEC, 0x0006, 0x0030, 0x003E, 0x0000, 0x0000, 0x0000
	}
};

static const INT16 FILTER_LUT_MODE2[95] =
{
	// ROM: 0F73
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0xFE8D, 0xFF3C, 0xFEF4, 0xFE00, 0xFED1, 0xFEC5,
	0xFF48, 0xFFB4, 0x0114, 0xFF00, 0x012A, 0x00C4, 0x03DE, 0x00EC,
	0x045A, 0xFF82, 0x1119, 0x1995, 0x0317,
	0x0000, 0x0000, 0x0000, 0x0000,
	// ROM: 0FA4
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000
};


static const INT16* get_filter_table_1(UINT16 offset)
{
	size_t tblIdx;
	
	if (offset < 0x0D53)
		return NULL;
	offset -= 0x0D53;
	if ((offset % 95) > 0)
		return NULL;
	tblIdx = offset / 95;
	if (tblIdx >= 5)
		return NULL;
	return FILTER_LUT_MODE1[tblIdx];	// return beginning of one of the tables
}

static const INT16* get_filter_table_2(UINT16 offset)
{
	if (offset < 0x0F73)
		return NULL;
	offset -= 0x0F73;
	if (offset >= 95)
		return NULL;
	return &FILTER_LUT_MODE2[offset];	// return pointer to table data
}

static INT16 dsp_get_sample(QSOUND_CHIP* chip, UINT16 bank, UINT16 ofs)
{
	UINT32 romOfs;
	UINT8 smplData;
	
	if (! chip->romMask)
		return 0;	// no ROM loaded
	if (! (bank & 0x8000))
		return 0;	// ignore attempts to read from DSP program ROM
	bank &= 0x7FFF;
	romOfs = (bank << 16) | (ofs << 0);
	
	smplData = chip->romData[romOfs & chip->romMask];
	return (INT16)((smplData << 8) | (smplData << 0));	// MAME currently expands the 8 bit ROM data to 16 bits this way.
}

INLINE void INC_MODULO(UINT16* reg, UINT16 rb, UINT16 re)
{
	if (*reg == re)
		*reg = rb;
	else
		(*reg) ++;
	return;
}

INLINE INT32 DSP_ROUND(INT32 value)
{
	return (value + 0x8000) & ~0xFFFF;
}

static void dsp_do_update_step(QSOUND_CHIP* chip)
{
	do
	{
		switch(chip->ram[OFS_ROUTINE])
		{
		case DSPRT_INIT1:
			dsp_init1(chip);
			break;
		case DSPRT_INIT2:
			dsp_init2(chip);
			break;
		case DSPRT_DO_TEST1:	// ROM: 000C
			chip->testInc = 0x0001;
			chip->ram[OFS_ROUTINE] = DSPRT_TEST;
			break;
		case DSPRT_DO_TEST2:	// ROM: 000F
			chip->testInc = 0x0400;
			chip->ram[OFS_ROUTINE] = DSPRT_TEST;
			break;
		case DSPRT_DO_UPD1:
			dsp_copy_filter_data_1(chip);
			break;
		case DSPRT_DO_UPD2:
			dsp_copy_filter_data_2(chip);
			break;
		case DSPRT_TEST:
			chip->updateFunc = dsp_update_test;
			break;
		case DSPRT_UPDATE1:
			chip->updateFunc = dsp_update_1;
			break;
		case DSPRT_UPDATE2:
			chip->updateFunc = dsp_update_2;
			break;
		default:	// handle invalid routines
			chip->ram[OFS_ROUTINE] = DSPRT_INIT1;
			break;
		}
	} while(chip->updateFunc == dsp_do_update_step);
	
	chip->dspRtStep = 0;
	chip->updateFunc(chip);
	return;
}

static void dsp_update_delay(QSOUND_CHIP* chip)
{
	if (chip->dspRtStep > 0)
		chip->dspRtStep --;
	else
		chip->updateFunc = dsp_do_update_step;
	
	return;
}

static void dsp_update_test(QSOUND_CHIP* chip)	// ROM: 0018
{
	if (chip->dspRtStep == 0)
	{
		chip->testOut += chip->testInc;	// saturation is disabled here
		
		// ---- Note: The DSP program processes external commands here. ---
		chip->busyState = 0;
	}
	
	chip->out[0] = chip->testOut;
	chip->out[1] = chip->testOut;
	
	chip->dspRtStep ++;
	if (chip->dspRtStep >= 2)
	{
		chip->dspRtStep -= 2;
		chip->updateFunc = dsp_do_update_step;
	}
	return;
}

static void dsp_copy_filter_data_1(QSOUND_CHIP* chip)	// ROM: 0039
{
	const INT16* filterTbl;
	
	filterTbl = get_filter_table_1(chip->ram[0x00DA]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M1_FILTER1_TBL], filterTbl, 95 * sizeof(INT16));
	
	filterTbl = get_filter_table_1(chip->ram[0x00DC]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M1_FILTER2_TBL], filterTbl, 95 * sizeof(INT16));
	
	chip->ram[OFS_ROUTINE] = DSPRT_UPDATE1;
	return;
}

static void dsp_copy_filter_data_2(QSOUND_CHIP* chip)	// ROM: 004F
{
	const INT16* filterTbl;
	
	filterTbl = get_filter_table_2(chip->ram[0x00DA]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M2_FILTER1A_TBL], filterTbl, 45 * sizeof(INT16));
	filterTbl = get_filter_table_2(chip->ram[0x00DB]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M2_FILTER1B_TBL], filterTbl, 44 * sizeof(INT16));
	
	filterTbl = get_filter_table_2(chip->ram[0x00DC]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M2_FILTER2A_TBL], filterTbl, 45 * sizeof(INT16));
	filterTbl = get_filter_table_2(chip->ram[0x00DD]);
	if (filterTbl != NULL)
		memcpy(&chip->ram[OFS_M2_FILTER2B_TBL], filterTbl, 44 * sizeof(INT16));
	
	chip->ram[OFS_ROUTINE] = DSPRT_UPDATE2;
	return;
}

static void dsp_init1(QSOUND_CHIP* chip)	// ROM: 0288
{
	UINT8 curChn;
	
	memset(chip->ram, 0x00, 0x800);
	chip->ram[OFS_ROUTINE] = DSPRT_UPDATE1;
	
	for (curChn = 0; curChn < 19; curChn ++)
		chip->ram[OFS_PAN_POS + curChn] = 0x0120;	// pan = centre
	chip->ram[0x0123] = OFS_M1_BUF_1F9;
	chip->ram[0x0124] = OFS_M1_BUF_257;
	chip->ram[0x0120] = OFS_M1_BUF_373;
	chip->ram[OFS_DELAYBUF_POS] = OFS_M1_DELAY_BUFFER;
	chip->ram[OFS_DELAYBUF_END] = OFS_M1_DELAY_BUFFER + 6;
	chip->ram[0x0125] = OFS_BUF_12D;
	chip->ram[0x0127] = OFS_BUF_160;
	chip->ram[0x0129] = OFS_BUF_193;
	chip->ram[0x012B] = OFS_BUF_1C6;
	
	chip->ram[0x00DE] = 0x0000;
	chip->ram[0x00DF] = 0x002E;
	chip->ram[0x00E0] = 0x0000;
	chip->ram[0x00E1] = 0x0030;
	dsp_filter_recalc(chip, OFS_FILTER_REFRESH);
	chip->ram[0x00E4] = 0x3FFF;
	chip->ram[0x00E5] = 0x3FFF;
	chip->ram[0x00E6] = 0x3FFF;
	chip->ram[0x00E7] = 0x3FFF;
	for (curChn = 0; curChn < 16; curChn ++)
		chip->ram[OFS_CH_BANK + curChn * 0x08] = 0x8000;
	
	chip->ram[0x00CC] = 0x8000;
	chip->ram[0x00D0] = 0x8000;
	chip->ram[0x00D4] = 0x8000;
	chip->ram[0x00DA] = 0x0DB2;
	chip->ram[0x00DC] = 0x0E11;
	
	dsp_copy_filter_data_1(chip);
	chip->dspRtStep = 4;
	chip->updateFunc = dsp_update_delay;	// simulate the DSP being busy for 4 samples
	return;
}

static void dsp_init2(QSOUND_CHIP* chip)	// ROM: 061A
{
	UINT8 curChn;
	
	memset(chip->ram, 0x00, 0x800);
	chip->ram[OFS_ROUTINE] = DSPRT_UPDATE2;
	
	for (curChn = 0; curChn < 19; curChn ++)
		chip->ram[OFS_PAN_POS + curChn] = 0x0120;	// pan = centre
	chip->ram[0x0123] = OFS_M2_BUF_1FB;
	chip->ram[0x0124] = OFS_M2_BUF_227;
	chip->ram[0x01F9] = OFS_M2_BUF_253;
	chip->ram[0x01FA] = OFS_M2_BUF_27E;
	chip->ram[0x0120] = OFS_M2_BUF_35B;
	chip->ram[OFS_DELAYBUF_POS] = OFS_M2_DELAY_BUFFER;
	chip->ram[OFS_DELAYBUF_END] = OFS_M2_DELAY_BUFFER + 6;
	chip->ram[0x0125] = OFS_BUF_12D;
	chip->ram[0x0127] = OFS_BUF_160;
	chip->ram[0x0129] = OFS_BUF_193;
	chip->ram[0x012B] = OFS_BUF_1C6;
	
	chip->ram[0x00DE] = 0x0001;
	chip->ram[0x00DF] = 0x0000;
	chip->ram[0x00E0] = 0x0000;
	chip->ram[0x00E1] = 0x0000;
	dsp_filter_recalc(chip, OFS_FILTER_REFRESH);
	chip->ram[0x00E4] = 0x3FFF;
	chip->ram[0x00E5] = 0x3FFF;
	chip->ram[0x00E6] = 0x3FFF;
	chip->ram[0x00E7] = 0x3FFF;
	for (curChn = 0; curChn < 16; curChn ++)
		chip->ram[OFS_CH_BANK + curChn * 0x08] = 0x8000;
	
	chip->ram[0x00CC] = 0x8000;
	chip->ram[0x00D0] = 0x8000;
	chip->ram[0x00D4] = 0x8000;
	chip->ram[0x00DA] = 0x0F73;
	chip->ram[0x00DB] = 0x0FA4;
	chip->ram[0x00DC] = 0x0F73;
	chip->ram[0x00DD] = 0x0FA4;
	
	dsp_copy_filter_data_2(chip);
	chip->dspRtStep = 4;
	chip->updateFunc = dsp_update_delay;	// simulate the DSP being busy for 4 samples
	return;
}

static void dsp_filter_recalc(QSOUND_CHIP* chip, UINT16 refreshFlagAddr)	// ROM: 05DD / 099B
{
	// Note: Subroutines 05DD and 099B are identical, except for a varying amount of NOPs at the end of the routine.
	UINT8 curFilter;
	UINT16 temp;
	
	for (curFilter = 0; curFilter < 4; curFilter ++)
	{
		temp = chip->ram[0x0125 + curFilter * 2] - chip->ram[0x00DE + curFilter];
		if (temp < 301 + curFilter * 51)
			temp += 51;
		chip->ram[0x0126 + curFilter * 2] = temp;
	}
	
	chip->ram[refreshFlagAddr] = 0;
	return;
}

static void dsp_update_1(QSOUND_CHIP* chip)	// ROM: 0314
{
	if (chip->dspRtStep < 3)
		dsp_filter_a(chip, chip->dspRtStep - 0);
	else
		dsp_filter_b(chip, chip->dspRtStep - 3);
	dsp_sample_calc_1(chip);
	
	chip->dspRtStep ++;
	if (chip->dspRtStep >= 6)
	{
		chip->dspRtStep -= 6;
		chip->updateFunc = dsp_do_update_step;
	}
	return;
}

static void dsp_update_2(QSOUND_CHIP* chip)	// ROM: 06B2
{
	// filter processing is identical in both functions
	if (chip->dspRtStep < 3)
		dsp_filter_a(chip, chip->dspRtStep - 0);
	else
		dsp_filter_b(chip, chip->dspRtStep - 3);
	dsp_sample_calc_2(chip);
	
	chip->dspRtStep ++;
	if (chip->dspRtStep >= 6)
	{
		chip->dspRtStep -= 6;
		chip->updateFunc = dsp_do_update_step;
	}
	return;
}

static void dsp_filter_a(QSOUND_CHIP* chip, UINT16 i1)	// ROM: 0314/036F/03CA
{
	UINT16 i4 = i1 * 4;
	INT16 x, y, z;
	INT32 a1;
	INT32 p;
	
	if (chip->ram[0x00F1 + i1] == chip->ram[0x00CB + i4])
		chip->ram[0x00E8 + i1] = 0;
	// ROM 0322/037D/03D8
	if (chip->ram[0x00D6 + i1] != 0)
	{
		chip->ram[0x011A + i1] = 0;
		chip->ram[0x00D6 + i1] = 0;
		chip->ram[0x00EB + i1] = 0x000A;
		chip->ram[0x00E8 + i1] = chip->ram[0x00CD + i4];
		chip->ram[0x00F1 + i1] = chip->ram[0x00CA + i4];
	}
	// ROM: 0333/038E/03E9
	//chip->ram[0x00F1 + i1] = chip->ram[0x00F1 + i1];
	x = dsp_get_sample(chip, chip->ram[0x00CC + i4], chip->ram[0x00F1 + i1]);
	chip->ram[0x00F4 + i1] = x >> 12;	// top nibble of x, sign-extended
	p = (INT16)chip->ram[0x00F4 + i1] * (INT16)chip->ram[0x00EB + i1];
	z = (INT16)chip->ram[0x00EB + i1] >> 1;
	// ROM 034A/03A5/0400
	if (p <= 0)
		z = -z;
	a1 = chip->ram[0x011A + i1] + p;
	x = chip->ram[0x00E8 + i1];
	y = a1 + z;
	p = x * y;
	chip->ram[0x011A + i1] = p >> 16;
	// ROM: 0354/03AF/040A
	x = LUT_09DC[0x08 + (INT16)chip->ram[0x00F4 + i1]];
	p = x * (INT16)chip->ram[0x00EB + i1];
	y = (p << 10) >> 16;
	if (y <= 1)
		y = 1;
	else if (y >= 2000)
		y = 2000;
	chip->ram[0x00EB + i1] = y;
	
	return;
}

static void dsp_filter_b(QSOUND_CHIP* chip, UINT16 i1)	// ROM: 0425/0470/04B4
{
	UINT16 i4 = i1 * 4;
	INT16 x, y, z;
	INT32 a1;
	INT32 p;
	
	x = dsp_get_sample(chip, chip->ram[0x00CC + i4], chip->ram[0x00F1 + i1]);
	chip->ram[0x00F1 + i1] ++;
	x = (x >> 8) & 0x0F;
	chip->ram[0x00F4 + i1] = x;
	p = x * (INT16)chip->ram[0x00EB + i1];
	z = (INT16)chip->ram[0x00EB + i1] >> 1;
	// ROM 0447/0492/04DD
	if (p <= 0)
		z = -z;
	a1 = chip->ram[0x011A + i1] + p;
	x = chip->ram[0x00E8 + i1];
	y = a1 + z;
	p = x * y;
	chip->ram[0x011A + i1] = p >> 16;
	// ROM: 0451/049C/04E7
	x = LUT_09DC[0x08 + (INT16)chip->ram[0x00F4 + i1]];
	p = x * (INT16)chip->ram[0x00EB + i1];
	y = (p << 10) >> 16;
	if (y <= 1)
		y = 1;
	else if (y >= 2000)
		y = 2000;
	chip->ram[0x00EB + i1] = y;
	
	return;
}

static void dsp_sample_calc_1(QSOUND_CHIP* chip)	// ROM: 0504
{
	INT16 x, y;
	INT32 a0, a1;
	INT32 p;
	UINT8 curChn;
	UINT16 curBank;
	UINT16 chnBase;
	UINT16 tmpOfs, tmpOf2;
	
	// ROM: 050A
	curBank = chip->ram[0x0078 + OFS_CH_BANK];
	for (curChn = 0, chnBase = OFS_CHANNEL_MEM; curChn < 16; curChn ++, chnBase += 0x08)
	{
		INT64 addr;
		
		p = (INT16)chip->ram[chnBase + OFS_CH_VOLUME] * dsp_get_sample(chip, curBank, chip->ram[chnBase + OFS_CH_CUR_ADDR]);
		chip->ram[OFS_CH_SMPLDATA + curChn] = p >> 14;	// p*4 >> 16
		
		curBank = chip->ram[chnBase + OFS_CH_BANK];
		addr = chip->ram[chnBase + OFS_CH_RATE] << 4;
		addr += ((INT16)chip->ram[chnBase + OFS_CH_CUR_ADDR] << 16) | (chip->ram[chnBase + OFS_CH_FRAC] << 0);
		if ((UINT16)(addr >> 16) >= chip->ram[chnBase + OFS_CH_END_ADDR])
			addr -= (chip->ram[chnBase + OFS_CH_LOOP_LEN] << 16);
		// The DSP's a0/a1 registers are 36 bits. The result is clamped when writing it back to RAM.
		if (addr > 0x7FFFFFFFLL)
			addr = 0x7FFFFFFFLL;
		else if (addr < -0x80000000LL)
			addr = -0x80000000LL;
		chip->ram[chnBase + OFS_CH_FRAC] = (addr >> 0) & 0xFFFF;
		chip->ram[chnBase + OFS_CH_CUR_ADDR] = (addr >> 16) & 0xFFFF;
	}
	
	// ROM: 051E
	a1 = 0;
	for (curChn = 0; curChn < 16; curChn ++)
	{
		p = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn] * (INT16)chip->ram[OFS_CH_REVERB + curChn];
		a1 += (p * 4);
	}
	tmpOfs = chip->ram[OFS_DELAYBUF_POS] & 0x7FF;
	a0 = (INT16)chip->ram[tmpOfs];
	chip->ram[0x0122] = a0;
	a0 += (INT16)chip->ram[0x0122];	// need 17 bits here
	a0 >>= 1;
	y = a0;
	x = (INT16)chip->ram[OFS_REVERB_VOL];
	chip->ram[0x0121] = y;
	p = x * y;
	a1 += (p * 4);
	chip->ram[tmpOfs] = a1 >> 16;
	INC_MODULO(&tmpOfs, OFS_M1_DELAY_BUFFER, chip->ram[OFS_DELAYBUF_END]);
	chip->ram[OFS_DELAYBUF_POS] = tmpOfs;
	
	// ROM: 0538
	tmpOfs = chip->ram[0x0120] & 0x7FF;
	chip->ram[tmpOfs] = a0;
	INC_MODULO(&tmpOfs, OFS_M1_BUF_373, OFS_M1_DELAY_BUFFER - 1);
	chip->ram[0x0120] = tmpOfs;
	
	// ---- Note: The DSP program processes external commands here. ---
	chip->busyState = 0;
	
	// ---- left channel ----
	// ROM: 055E
	y = 0; p = 0; a0 = 0; a1 = 0;
	for (curChn = 0; curChn < 19; curChn ++)
	{
		UINT16 panPos;
		
		panPos = chip->ram[OFS_PAN_POS + curChn];
		a0 -= p*4;
		p = x * y;
		y = chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_LEFT_OUTPUT][panPos - 0x110];
		
		a1 -= p*4;
		p = x * y;
		y = chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_LEFT_FILTER][panPos - 0x110];
		
		chip->ram[OFS_PAN_RPOS + curChn] = panPos + 98 * 2;
	}
	a0 -= p*4;
	p = x * y;
	a1 -= p*4;
	a0 += (INT16)chip->ram[0x0121] << 16;
	chip->ram[0x011E] = a0 >> 16;
	
	// ROM: 0572
	tmpOf2 = OFS_M1_FILTER1_TBL;
	tmpOfs = chip->ram[0x0123] & 0x7FF;
	a0 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M1_BUF_1F9, OFS_M1_BUF_257 - 1);
	for (curChn = 0; curChn < 93; curChn ++)
	{
		a0 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M1_BUF_1F9, OFS_M1_BUF_257 - 1);
	}
	a0 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = a1 >> 16;
	a0 -= p*4;
	p = x * y;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M1_BUF_1F9, OFS_M1_BUF_257 - 1);
	chip->ram[0x0123] = tmpOfs;
	
	// ROM: 0582
	a0 -= p*4;
	x = (INT16)chip->ram[0x00E4];
	tmpOfs = chip->ram[0x0125] & 0x7FF;
	chip->ram[tmpOfs] = a0 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_12D, OFS_BUF_160 - 1);
	chip->ram[0x0125] = tmpOfs;
	
	tmpOfs = chip->ram[0x0126] & 0x7FF;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_12D, OFS_BUF_160 - 1);
	p = x * y;
	x = (INT16)chip->ram[0x00E5];
	a0 = p*4;
	y = (INT16)chip->ram[0x011E];
	chip->ram[0x0126] = tmpOfs;
	
	tmpOfs = chip->ram[0x0127] & 0x7FF;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_BUF_160, OFS_BUF_193 - 1);
	chip->ram[0x0127] = tmpOfs;
	
	tmpOfs = chip->ram[0x0128] & 0x7FF;
	y = chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_160, OFS_BUF_193 - 1);
	chip->ram[0x0128] = tmpOfs;
	p = x * y;
	y = (INT16)chip->ram[tmpOfs];
	x = LUT_09D2[0];
	a0 += p*4;
	p = x * y;
	a0 = DSP_ROUND(a0);
	chip->out[0] = a0 >> 16;
	
	// ---- right channel ----
	// ROM: 059D
	y = 0; p = 0; a0 = 0; a1 = 0;
	for (curChn = 0; curChn < 19; curChn ++)
	{
		UINT16 panPos;
		
		//panPos = chip->ram[OFS_PAN_POS + curChn] + 98 * 2;
		panPos = chip->ram[OFS_PAN_RPOS + curChn];
		a0 -= p*4;
		p = x * y;
		y = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_RIGHT_OUTPUT][panPos - 0x110 - 98 * 2];
		
		a1 -= p*4;
		p = x * y;
		y = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_RIGHT_FILTER][panPos - 0x110 - 98 * 2];
	}
	a0 -= p*4;
	p = x * y;
	a1 -= p*4;
	a1 += (INT16)chip->ram[0x0121] << 16;
	chip->ram[0x011E] = a0 >> 16;
	
	// ROM: 05A9
	tmpOf2 = OFS_M1_FILTER2_TBL;
	tmpOfs = chip->ram[0x0124] & 0x7FF;
	a0 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M1_BUF_257, OFS_M1_FILTER1_TBL - 1);
	for (curChn = 0; curChn < 93; curChn ++)
	{
		a0 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M1_BUF_257, OFS_M1_FILTER1_TBL - 1);
	}
	a0 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = a1 >> 16;
	a0 -= p*4;
	p = x * y;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M1_BUF_257, OFS_M1_FILTER1_TBL - 1);
	chip->ram[0x0124] = tmpOfs;
	
	// ROM: 05BA
	a0 -= p*4;
	x = (INT16)chip->ram[0x00E6];
	tmpOfs = chip->ram[0x0129] & 0x7FF;
	chip->ram[tmpOfs] = a0 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_193, OFS_BUF_1C6 - 1);
	chip->ram[0x0129] = tmpOfs;
	
	tmpOfs = chip->ram[0x012A] & 0x7FF;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_193, OFS_BUF_1C6 - 1);
	p = x * y;
	x = (INT16)chip->ram[0x00E7];
	a0 = p*4;
	y = (INT16)chip->ram[0x011E];
	chip->ram[0x012A] = tmpOfs;
	
	tmpOfs = chip->ram[0x012B] & 0x7FF;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_BUF_1C6, OFS_M1_BUF_1F9 - 1);
	chip->ram[0x012B] = tmpOfs;
	
	tmpOfs = chip->ram[0x012C] & 0x7FF;
	y = chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_1C6, OFS_M1_BUF_1F9 - 1);
	chip->ram[0x012C] = tmpOfs;
	p = x * y;
	y = (INT16)chip->ram[tmpOfs];
	x = LUT_09D2[0];
	a0 += p*4;
	p = x * y;
	a0 = DSP_ROUND(a0);
	chip->out[1] = a0 >> 16;
	
	if (chip->ram[OFS_FILTER_REFRESH])
		dsp_filter_recalc(chip, OFS_FILTER_REFRESH);
	
	return;
}

static void dsp_sample_calc_2(QSOUND_CHIP* chip)	// ROM: 08A2
{
	INT16 x, y;
	INT32 a0, a1;
	INT32 p;
	UINT8 curChn;
	UINT16 curBank;
	UINT16 chnBase;
	UINT16 tmpOfs, tmpOf2;
	
	// ROM: 08A8
	curBank = chip->ram[0x0078 + OFS_CH_BANK];
	for (curChn = 0, chnBase = OFS_CHANNEL_MEM; curChn < 16; curChn ++, chnBase += 0x08)
	{
		INT64 addr;
		
		p = (INT16)chip->ram[chnBase + OFS_CH_VOLUME] * dsp_get_sample(chip, curBank, chip->ram[chnBase + OFS_CH_CUR_ADDR]);
		chip->ram[OFS_CH_SMPLDATA + curChn] = p >> 14;	// p*4 >> 16
		
		curBank = chip->ram[chnBase + OFS_CH_BANK];
		addr = chip->ram[chnBase + OFS_CH_RATE] << 4;
		addr += ((INT16)chip->ram[chnBase + OFS_CH_CUR_ADDR] << 16) | (chip->ram[chnBase + OFS_CH_FRAC] << 0);
		if ((UINT16)(addr >> 16) >= chip->ram[chnBase + OFS_CH_END_ADDR])
			addr -= (chip->ram[chnBase + OFS_CH_LOOP_LEN] << 16);
		// The DSP's a0/a1 registers are 36 bits. The result is clamped when writing it back to RAM.
		if (addr > 0x7FFFFFFFLL)
			addr = 0x7FFFFFFFLL;
		else if (addr < -0x80000000LL)
			addr = -0x80000000LL;
		chip->ram[chnBase + OFS_CH_FRAC] = (addr >> 0) & 0xFFFF;
		chip->ram[chnBase + OFS_CH_CUR_ADDR] = (addr >> 16) & 0xFFFF;
	}
	
	// ROM: 08BC
	a1 = 0;
	for (curChn = 0; curChn < 16; curChn ++)
	{
		p = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn] * (INT16)chip->ram[OFS_CH_REVERB + curChn];
		a1 += (p * 4);
	}
	tmpOfs = chip->ram[OFS_DELAYBUF_POS] & 0x7FF;
	a0 = (INT16)chip->ram[tmpOfs];
	chip->ram[0x0122] = a0;
	a0 += (INT16)chip->ram[0x0122];	// need 17 bits here
	a0 >>= 1;
	y = a0;
	x = (INT16)chip->ram[OFS_REVERB_VOL];
	chip->ram[0x0121] = y;
	p = x * y;
	a1 += (p * 4);
	chip->ram[tmpOfs] = a1 >> 16;
	INC_MODULO(&tmpOfs, OFS_M2_DELAY_BUFFER, chip->ram[OFS_DELAYBUF_END]);
	chip->ram[OFS_DELAYBUF_POS] = tmpOfs;
	
	// ROM: 08D6
	tmpOfs = chip->ram[0x0120] & 0x7FF;
	chip->ram[tmpOfs] = a0;
	INC_MODULO(&tmpOfs, OFS_M2_BUF_35B, OFS_M2_DELAY_BUFFER - 1);
	chip->ram[0x0120] = tmpOfs;
	
	// ---- Note: The DSP program processes external commands here. ---
	chip->busyState = 0;
	
	// ---- left channel ----
	// ROM: 08FC
	y = 0; p = 0; a0 = 0; a1 = 0;
	for (curChn = 0; curChn < 19; curChn ++)
	{
		UINT16 panPos;
		
		panPos = chip->ram[OFS_PAN_POS + curChn];
		a0 -= p*4;
		p = x * y;
		y = chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_LEFT_OUTPUT][panPos - 0x110];
		
		a1 -= p*4;
		p = x * y;
		y = chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_LEFT_FILTER][panPos - 0x110];
		
		chip->ram[OFS_PAN_RPOS + curChn] = panPos + 98 * 2;
	}
	a0 -= p*4;
	p = x * y;
	a1 -= p*4;
	a0 += (INT16)chip->ram[0x0121] << 16;
	chip->ram[0x011E] = a0 >> 16;
	
	// ROM: 0910
	tmpOf2 = OFS_M2_FILTER1A_TBL;
	tmpOfs = chip->ram[0x0123] & 0x7FF;
	a0 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_1FB, OFS_M2_BUF_227 - 1);
	for (curChn = 0; curChn < 43; curChn ++)
	{
		a0 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_1FB, OFS_M2_BUF_227 - 1);
	}
	a0 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = a1 >> 16;
	a0 -= p*4;
	p = x * y;
	a0 -= p*4;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M2_BUF_1FB, OFS_M2_BUF_227 - 1);
	chip->ram[0x0123] = tmpOfs;
	
	// ROM: 0921
	tmpOfs = chip->ram[0x01F9] & 0x7FF;
	a1 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_253, OFS_M2_BUF_27E - 1);
	for (curChn = 0; curChn < 42; curChn ++)
	{
		a1 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_253, OFS_M2_BUF_27E - 1);
	}
	a1 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[0x011E];
	a1 -= p*4;
	p = x * y;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M2_BUF_253, OFS_M2_BUF_27E - 1);
	chip->ram[0x01F9] = tmpOfs;
	
	// ROM: 0930
	a1 -= p*4;
	x = (INT16)chip->ram[0x00E4];
	tmpOfs = chip->ram[0x0125] & 0x7FF;
	chip->ram[tmpOfs] = a0 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_12D, OFS_BUF_160 - 1);
	chip->ram[0x0125] = tmpOfs;
	
	tmpOfs = chip->ram[0x0126] & 0x7FF;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_12D, OFS_BUF_160 - 1);
	p = x * y;
	x = (INT16)chip->ram[0x00E5];
	a0 = p*4;
	y = (INT16)chip->ram[0x011E];
	chip->ram[0x0126] = tmpOfs;
	
	tmpOfs = chip->ram[0x0127] & 0x7FF;
	chip->ram[tmpOfs] = a1 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_160, OFS_BUF_193 - 1);
	chip->ram[0x0127] = tmpOfs;
	
	tmpOfs = chip->ram[0x0128] & 0x7FF;
	y = chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_160, OFS_BUF_193 - 1);
	chip->ram[0x0128] = tmpOfs;
	p = x * y;
	y = (INT16)chip->ram[tmpOfs];
	x = LUT_09D2[0];
	a0 += p*4;
	p = x * y;
	a0 = DSP_ROUND(a0);
	chip->out[0] = a0 >> 16;
	
	// ---- right channel ----
	// ROM: 094B
	y = 0; p = 0; a0 = 0; a1 = 0;
	for (curChn = 0; curChn < 19; curChn ++)
	{
		UINT16 panPos;
		
		//panPos = chip->ram[OFS_PAN_POS + curChn] + 98 * 2;
		panPos = chip->ram[OFS_PAN_RPOS + curChn];
		a0 -= p*4;
		p = x * y;
		y = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_RIGHT_OUTPUT][panPos - 0x110 - 98 * 2];
		
		a1 -= p*4;
		p = x * y;
		y = (INT16)chip->ram[OFS_CH_SMPLDATA + curChn];
		x = PAN_TABLES[PANTBL_RIGHT_FILTER][panPos - 0x110 - 98 * 2];
	}
	a0 -= p*4;
	p = x * y;
	a1 -= p*4;
	a1 += (INT16)chip->ram[0x0121] << 16;
	chip->ram[0x011E] = a0 >> 16;
	
	// ROM: 0957
	tmpOf2 = OFS_M2_FILTER2A_TBL;
	tmpOfs = chip->ram[0x0124] & 0x7FF;
	a0 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_227, OFS_M2_BUF_253 - 1);
	for (curChn = 0; curChn < 93; curChn ++)
	{
		a0 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_227, OFS_M2_BUF_253 - 1);
	}
	a0 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = a1 >> 16;
	a0 -= p*4;
	p = x * y;
	a0 -= p*4;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M2_BUF_227, OFS_M2_BUF_253 - 1);
	chip->ram[0x0124] = tmpOfs;
	
	// ROM: 0969
	tmpOfs = chip->ram[0x01FA] & 0x7FF;
	a1 = 0; p = 0;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_27E, 0x02A9 - 1);
	for (curChn = 0; curChn < 42; curChn ++)
	{
		a1 -= p*4;
		p = x * y;
		x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
		y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_M2_BUF_27E, 0x02A9 - 1);
	}
	a1 -= p*4;
	p = x * y;
	x = (INT16)chip->ram[tmpOf2];	tmpOf2 ++;
	y = (INT16)chip->ram[0x011E];
	a1 -= p*4;
	p = x * y;
	chip->ram[tmpOfs] = y;	INC_MODULO(&tmpOfs, OFS_M2_BUF_27E, 0x02A9 - 1);
	chip->ram[0x01FA] = tmpOfs;
	
	// ROM: 0978
	a1 -= p*4;
	x = (INT16)chip->ram[0x00E6];
	tmpOfs = chip->ram[0x0129] & 0x7FF;
	chip->ram[tmpOfs] = a0 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_193, OFS_BUF_1C6 - 1);
	chip->ram[0x0129] = tmpOfs;
	
	tmpOfs = chip->ram[0x012A] & 0x7FF;
	y = (INT16)chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_193, OFS_BUF_1C6 - 1);
	p = x * y;
	x = (INT16)chip->ram[0x00E7];
	a0 = p*4;
	y = (INT16)chip->ram[0x011E];
	chip->ram[0x012A] = tmpOfs;
	
	tmpOfs = chip->ram[0x012B] & 0x7FF;
	chip->ram[tmpOfs] = a1 >> 16;	INC_MODULO(&tmpOfs, OFS_BUF_1C6, OFS_M1_BUF_1F9 - 1);
	chip->ram[0x012B] = tmpOfs;
	
	tmpOfs = chip->ram[0x012C] & 0x7FF;
	y = chip->ram[tmpOfs];	INC_MODULO(&tmpOfs, OFS_BUF_1C6, OFS_M1_BUF_1F9 - 1);
	chip->ram[0x012C] = tmpOfs;
	p = x * y;
	y = (INT16)chip->ram[tmpOfs];
	x = LUT_09D2[0];
	a0 += p*4;
	p = x * y;
	a0 = DSP_ROUND(a0);
	chip->out[1] = a0 >> 16;
	
	if (chip->ram[OFS_FILTER_REFRESH])
		dsp_filter_recalc(chip, OFS_FILTER_REFRESH);
	
	return;
}


static UINT8 device_start_qsound_vb(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	QSOUND_CHIP* chip;
	
	chip = (QSOUND_CHIP*)calloc(1, sizeof(QSOUND_CHIP));
	if (chip == NULL)
		return 0xFF;
	
	chip->romData = NULL;
	chip->romSize = 0x00;
	chip->romMask = 0x00;
	
	qsoundv_set_mute_mask(chip, 0x0000);
	
	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, /*cfg->clock*/30000000 / 1248, &devDef_QSound_VB);
	
	return 0x00;
}

static void device_stop_qsound_vb(void* info)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	free(chip->romData);
	free(chip);
	
	return;
}

static void device_reset_qsound_vb(void* info)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	memset(chip->ram, 0x00, 0x800);
	chip->testInc = 0;
	chip->testOut = 0;
	chip->dataLatch = 0x0000;
	chip->busyState = 1;
	chip->out[0] = chip->out[1] = 0;
	chip->dspRoutine = DSPRT_INIT1;
	chip->dspRtStep = 0;
	chip->updateFunc = dsp_do_update_step;
	
	dsp_init1(chip);
	
	return;
}

static UINT8 qsoundv_r(void* info, UINT8 offset)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	// ready bit (0x00 = busy, 0x80 == ready)
	return chip->busyState ? 0x00 : 0x80;
}

static void qsoundv_w(void* info, UINT8 offset, UINT8 data)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	switch (offset)
	{
	case 0:
		chip->dataLatch = (chip->dataLatch & 0x00FF) | (data << 8);
		break;
	case 1:
		chip->dataLatch = (chip->dataLatch & 0xFF00) | (data << 0);
		break;
	case 2:
		qsoundv_write_data(chip, data, chip->dataLatch);
		break;
	}
	
	return;
}

static void qsoundv_write_data(void* info, UINT8 address, UINT16 data)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	chip->ram[address] = data;
	chip->busyState = 1;
	
	return;
}

static void qsoundv_update(void* param, UINT32 samples, DEV_SMPL** outputs)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)param;
	UINT32 curSmpl;
	
	memset(outputs[0], 0, samples * sizeof(*outputs[0]));
	memset(outputs[1], 0, samples * sizeof(*outputs[1]));
	
	for (curSmpl = 0; curSmpl < samples; curSmpl ++)
	{
		chip->updateFunc(chip);
		outputs[0][curSmpl] = chip->out[0];
		outputs[1][curSmpl] = chip->out[1];
	}
	
	return;
}

static void qsoundv_alloc_rom(void* info, UINT32 memsize)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	if (chip->romSize == memsize)
		return;
	
	chip->romData = (UINT8*)realloc(chip->romData, memsize);
	chip->romSize = memsize;
	chip->romMask = pow2_mask(memsize);
	memset(chip->romData, /*0xFF*/0x00, memsize);
	
	return;
}

static void qsoundv_write_rom(void* info, UINT32 offset, UINT32 length, const UINT8* data)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	if (offset > chip->romSize)
		return;
	if (offset + length > chip->romSize)
		length = chip->romSize - offset;
	
	memcpy(chip->romData + offset, data, length);
	
	return;
}

static void qsoundv_set_mute_mask(void* info, UINT32 MuteMask)
{
	QSOUND_CHIP* chip = (QSOUND_CHIP*)info;
	
	chip->muteMask = MuteMask;
	
	return;
}
