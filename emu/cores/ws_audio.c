// TODO: int/long -> INT32/INT16
#include <stdlib.h>
#include <string.h>	// for memset
#include <stddef.h>	// for NULL

#include <stdtype.h>
#include "../EmuStructs.h"
#include "../EmuCores.h"
#include "../snddef.h"
#include "../EmuHelper.h"
#include "ws_audio.h"


typedef struct _wsa_state wsa_state;

static UINT8 ws_audio_init(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf);
static void ws_audio_reset(void* info);
static void ws_audio_done(void* info);
static void ws_audio_update(void* info, UINT32 length, DEV_SMPL** buffer);
static void ws_audio_port_write(void* info, UINT8 port, UINT8 value);
static UINT8 ws_audio_port_read(void* info, UINT8 port);
static void ws_audio_process(wsa_state* chip);
static void ws_audio_sounddma(wsa_state* chip);
static void ws_write_ram_byte(void* info, UINT16 offset, UINT8 value);
static UINT8 ws_read_ram_byte(void* info, UINT16 offset);
static void ws_set_mute_mask(void* info, UINT32 MuteMask);


static DEVDEF_RWFUNC devFunc[] =
{
	{RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, ws_audio_port_write},
	{RWF_REGISTER | RWF_READ, DEVRW_A8D8, 0, ws_audio_port_read},
	{RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, ws_write_ram_byte},
	{RWF_MEMORY | RWF_READ, DEVRW_A16D8, 0, ws_read_ram_byte},
	//{RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, ws_write_ram_block},
	{0x00, 0x00, 0, NULL}
};
static DEV_DEF devDef =
{
	"WonderSwan", "in_wsr", 0x00000000,
	
	ws_audio_init,
	ws_audio_done,
	ws_audio_reset,
	ws_audio_update,
	
	NULL,	// SetOptionBits
	ws_set_mute_mask,
	NULL,	// SetPanning
	NULL,	// SetSampleRateChangeCallback
	NULL,	// LinkDevice
	
	devFunc,	// rwFuncs
};

const DEV_DEF* devDefList_WSwan[] =
{
	&devDef,
	NULL
};


typedef UINT8	byte;
#include "ws_initialIo.h"

#define SNDP	chip->ws_ioRam[0x80]
#define SNDV	chip->ws_ioRam[0x88]
#define SNDSWP	chip->ws_ioRam[0x8C]
#define SWPSTP	chip->ws_ioRam[0x8D]
#define NSCTL	chip->ws_ioRam[0x8E]
#define WAVDTP	chip->ws_ioRam[0x8F]
#define SNDMOD	chip->ws_ioRam[0x90]
#define SNDOUT	chip->ws_ioRam[0x91]
#define PCSRL	chip->ws_ioRam[0x92]
#define PCSRH	chip->ws_ioRam[0x93]
#define DMASL	chip->ws_ioRam[0x40]
#define DMASH	chip->ws_ioRam[0x41]
#define DMASB	chip->ws_ioRam[0x42]
#define DMADB	chip->ws_ioRam[0x43]
#define DMADL	chip->ws_ioRam[0x44]
#define DMADH	chip->ws_ioRam[0x45]
#define DMACL	chip->ws_ioRam[0x46]
#define DMACH	chip->ws_ioRam[0x47]
#define DMACTL	chip->ws_ioRam[0x48]
#define SDMASL	chip->ws_ioRam[0x4A]
#define SDMASH	chip->ws_ioRam[0x4B]
#define SDMASB	chip->ws_ioRam[0x4C]
#define SDMACL	chip->ws_ioRam[0x4E]
#define SDMACH	chip->ws_ioRam[0x4F]
#define SDMACTL	chip->ws_ioRam[0x52]

//SoundDMA の転送間隔
// 実際の数値が分からないので、予想です
// サンプリング周期から考えてみて以下のようにした
// 12KHz = 1.00HBlank = 256cycles間隔
// 16KHz = 0.75HBlank = 192cycles間隔
// 20KHz = 0.60HBlank = 154cycles間隔
// 24KHz = 0.50HBlank = 128cycles間隔
static const int DMACycles[4] = { 256, 192, 154, 128 };

typedef struct
{
	int wave;
	int lvol;
	int rvol;
	long offset;
	long delta;
	long pos;
	UINT8 Muted;
} WS_AUDIO;

struct _wsa_state
{
	DEV_DATA _devData;
	
	WS_AUDIO ws_audio[4];
	int sweepDelta;
	int sweepOffset;
	int SweepTime;
	int SweepStep;
	int SweepCount;
	int SweepFreq;
	int NoiseType;
	int NoiseRng;
	int MainVolume;
	int PCMVolumeLeft;
	int PCMVolumeRight;

	UINT8	 ws_ioRam[0x100];
	UINT8	*ws_internalRam;
	
	UINT32 clock;
	UINT32 smplrate;
};

#define DEFAULT_CLOCK	3072000


static UINT8 ws_audio_init(const DEV_GEN_CFG* cfg, DEV_INFO* retDevInf)
{
	wsa_state* chip;
	
	chip = (wsa_state*)calloc(1, sizeof(wsa_state));
	if (chip == NULL)
		return 0xFF;
	
	// actual size is 64 KB, but the audio chip can only access 16 KB
	chip->ws_internalRam = (UINT8*)malloc(0x4000);
	
	chip->clock = cfg->clock;
	chip->smplrate = cfg->smplRate;
	//SRATE_CUSTOM_HIGHEST(cfg->srMode, chip->smplrate, cfg->smplRate);
	
	ws_set_mute_mask(chip, 0x00);
	
	chip->_devData.chipInf = chip;
	INIT_DEVINF(retDevInf, &chip->_devData, chip->smplrate, &devDef);
	
	return 0x00;
}

static void ws_audio_reset(void* info)
{
	wsa_state* chip = (wsa_state*)info;
	int i;
	
	// TODO: save/restore muting flags
	memset(&chip->ws_audio, 0, sizeof(WS_AUDIO));
	chip->SweepTime = 0;
	chip->SweepStep = 0;
	chip->NoiseType = 0;
	chip->NoiseRng = 1;
	chip->MainVolume = 0x02;	// 0x04
	chip->PCMVolumeLeft = 0;
	chip->PCMVolumeRight = 0;
	
	chip->sweepDelta = chip->clock * 256 / chip->smplrate;
	chip->sweepOffset = 0;
	
	for (i=0x80;i<0xc9;i++)
		ws_audio_port_write(chip, i, initialIoValue[i]);
}

static void ws_audio_done(void* info)
{
	wsa_state* chip = (wsa_state*)info;
	
	free(chip->ws_internalRam);
	free(chip);
	
	return;
}

static void ws_audio_update(void* info, UINT32 length, DEV_SMPL** buffer)
{
	wsa_state* chip = (wsa_state*)info;
	DEV_SMPL* bufL;
	DEV_SMPL* bufR;
	UINT32 i;
	int ch, cnt;
	INT16 w;	// could fit into INT8
	DEV_SMPL l, r;

	bufL = buffer[0];
	bufR = buffer[1];
	for (i=0; i<length; i++)
	{
		chip->sweepOffset += chip->sweepDelta;
		while(chip->sweepOffset >= 0x10000)
		{
			chip->sweepOffset -= 0x10000;
			ws_audio_process(chip);
		}
		l = r = 0;

		for (ch=0; ch<4; ch++)
		{
			if (chip->ws_audio[ch].Muted)
				continue;
			
			if ((ch==1) && (SNDMOD&0x20))
			{
				// Voice出力
				w = chip->ws_ioRam[0x89];
				w -= 0x80;
				l += chip->PCMVolumeLeft  * w;
				r += chip->PCMVolumeRight * w;
			}
			else if (SNDMOD&(1<<ch))
			{
				if ((ch==3) && (SNDMOD&0x80))
				{
					//Noise

					//OSWANの擬似乱数の処理と同等のつもり
#define BIT(n) (1<<n)
					const unsigned long noise_mask[8] =
					{
						BIT(0)|BIT(1),
						BIT(0)|BIT(1)|BIT(4)|BIT(5),
						BIT(0)|BIT(1)|BIT(3)|BIT(4),
						BIT(0)|BIT(1)|BIT(4)|BIT(6),
						BIT(0)|BIT(2),
						BIT(0)|BIT(3),
						BIT(0)|BIT(4),
						BIT(0)|BIT(2)|BIT(3)|BIT(4)
					};

					const unsigned long noise_bit[8] =
					{
						BIT(15),
						BIT(14),
						BIT(13),
						BIT(12),
						BIT(11),
						BIT(10),
						BIT(9),
						BIT(8)
					};

					int Masked, XorReg;

					chip->ws_audio[ch].offset += chip->ws_audio[ch].delta;
					cnt = chip->ws_audio[ch].offset>>16;
					chip->ws_audio[ch].offset &= 0xffff;
					while (cnt > 0)
					{
						cnt--;

						chip->NoiseRng &= noise_bit[chip->NoiseType]-1;
						if (!chip->NoiseRng) chip->NoiseRng = noise_bit[chip->NoiseType]-1;

						Masked = chip->NoiseRng & noise_mask[chip->NoiseType];
						XorReg = 0;
						while (Masked)
						{
							XorReg ^= Masked&1;
							Masked >>= 1;
						}
						if (XorReg)
							chip->NoiseRng |= noise_bit[chip->NoiseType];
						chip->NoiseRng >>= 1;
					}

					PCSRL = (byte)(chip->NoiseRng&0xff);
					PCSRH = (byte)((chip->NoiseRng>>8)&0x7f);

					w = (chip->NoiseRng&1)? 0x7f:-0x80;
					l += chip->ws_audio[ch].lvol * w;
					r += chip->ws_audio[ch].rvol * w;
				}
				else
				{
					chip->ws_audio[ch].offset += chip->ws_audio[ch].delta;
					cnt = chip->ws_audio[ch].offset>>16;
					chip->ws_audio[ch].offset &= 0xffff;
					chip->ws_audio[ch].pos += cnt;
					chip->ws_audio[ch].pos &= 0x1f;
					w = chip->ws_internalRam[(chip->ws_audio[ch].wave&0xFFF0) + (chip->ws_audio[ch].pos>>1)];
					if ((chip->ws_audio[ch].pos&1) == 0)
						w = (w<<4)&0xf0;	//下位ニブル
					else
						w = w&0xf0;			//上位ニブル
					w -= 0x80;
					l += chip->ws_audio[ch].lvol * w;
					r += chip->ws_audio[ch].rvol * w;
				}
			}
		}

		bufL[i] = l * chip->MainVolume;
		bufR[i] = r * chip->MainVolume;
	}
}

static void ws_audio_port_write(void* info, UINT8 port, UINT8 value)
{
	wsa_state* chip = (wsa_state*)info;
	int i;
	long freq;

	chip->ws_ioRam[port]=value;

	switch (port)
	{
	// 0x80-0x87の周波数レジスタについて
	// - ロックマン&フォルテの0x0fの曲では、周波数=0xFFFF の音が不要
	// - デジモンディープロジェクトの0x0dの曲のノイズは 周波数=0x07FF で音を出す
	// →つまり、0xFFFF の時だけ音を出さないってことだろうか。
	//   でも、0x07FF の時も音を出さないけど、ノイズだけ音を出すのかも。
	case 0x80:
	case 0x81:	i=(((unsigned int)chip->ws_ioRam[0x81])<<8)+((unsigned int)chip->ws_ioRam[0x80]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = chip->clock/(2048-(i&0x7ff));
				chip->ws_audio[0].delta = (long)((float)freq*(float)65536/(float)chip->smplrate);
				break;
	case 0x82:
	case 0x83:	i=(((unsigned int)chip->ws_ioRam[0x83])<<8)+((unsigned int)chip->ws_ioRam[0x82]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = chip->clock/(2048-(i&0x7ff));
				chip->ws_audio[1].delta = (long)((float)freq*(float)65536/(float)chip->smplrate);
				break;
	case 0x84:
	case 0x85:	i=(((unsigned int)chip->ws_ioRam[0x85])<<8)+((unsigned int)chip->ws_ioRam[0x84]);
				chip->SweepFreq = i;
				if (i == 0xffff)
					freq = 0;
				else
					freq = chip->clock/(2048-(i&0x7ff));
				chip->ws_audio[2].delta = (long)((float)freq*(float)65536/(float)chip->smplrate);
				break;
	case 0x86:
	case 0x87:	i=(((unsigned int)chip->ws_ioRam[0x87])<<8)+((unsigned int)chip->ws_ioRam[0x86]);
				if (i == 0xffff)
					freq = 0;
				else
					freq = chip->clock/(2048-(i&0x7ff));
				chip->ws_audio[3].delta = (long)((float)freq*(float)65536/(float)chip->smplrate);
				break;
	case 0x88:
				chip->ws_audio[0].lvol = (value>>4)&0xf;
				chip->ws_audio[0].rvol = value&0xf;
				break;
	case 0x89:
				chip->ws_audio[1].lvol = (value>>4)&0xf;
				chip->ws_audio[1].rvol = value&0xf;
				break;
	case 0x8A:
				chip->ws_audio[2].lvol = (value>>4)&0xf;
				chip->ws_audio[2].rvol = value&0xf;
				break;
	case 0x8B:
				chip->ws_audio[3].lvol = (value>>4)&0xf;
				chip->ws_audio[3].rvol = value&0xf;
				break;
	case 0x8C:
				chip->SweepStep = (signed char)value;
				break;
	case 0x8D:
				//Sweepの間隔は 1/375[s] = 2.666..[ms]
				//CPU Clockで言うと 3072000/375 = 8192[cycles]
				//ここの設定値をnとすると、8192[cycles]*(n+1) 間隔でSweepすることになる
				//
				//これを HBlank (256cycles) の間隔で言うと、
				//　8192/256 = 32
				//なので、32[HBlank]*(n+1) 間隔となる
				chip->SweepTime = (((unsigned int)value)+1)<<5;
				chip->SweepCount = chip->SweepTime;
				break;
	case 0x8E:
				chip->NoiseType = value&7;
				if (value&8) chip->NoiseRng = 1;	//ノイズカウンターリセット
				break;
	case 0x8F:
				chip->ws_audio[0].wave = (unsigned int)value<<6;
				chip->ws_audio[1].wave = chip->ws_audio[0].wave + 0x10;
				chip->ws_audio[2].wave = chip->ws_audio[1].wave + 0x10;
				chip->ws_audio[3].wave = chip->ws_audio[2].wave + 0x10;
				break;
	case 0x90:
				break;
	case 0x91:
				//ここでのボリューム調整は、内蔵Speakerに対しての調整だけらしいので、
				//ヘッドフォン接続されていると認識させれば問題無いらしい。
				chip->ws_ioRam[port] |= 0x80;
				break;
	case 0x92:
	case 0x93:
				break;
	case 0x94:
				chip->PCMVolumeLeft  = (value&0xc)*2;
				chip->PCMVolumeRight = ((value<<2)&0xc)*2;
				break;
	case 0x52:
				//if (value&0x80)
				//	ws_timer_set(2, DMACycles[value&3]);
				break;
	}
}

static UINT8 ws_audio_port_read(void* info, UINT8 port)
{
	wsa_state* chip = (wsa_state*)info;
	return (chip->ws_ioRam[port]);
}

// HBlank間隔で呼ばれる
// Note: Must be called every 256 cycles (3072000 Hz clock), i.e. at 12000 Hz
static void ws_audio_process(wsa_state* chip)
{
	long freq;

	if (chip->SweepStep && (SNDMOD&0x40))
	{
		if (chip->SweepCount < 0)
		{
			chip->SweepCount = chip->SweepTime;
			chip->SweepFreq += chip->SweepStep;
			chip->SweepFreq &= 0x7FF;

			freq = chip->clock/(2048-chip->SweepFreq);
			chip->ws_audio[2].delta = (long)((float)freq*(float)65536/(float)chip->smplrate);
		}
		chip->SweepCount--;
	}
}

static void ws_audio_sounddma(wsa_state* chip)
{
	UINT16 i;
	UINT32 j;
	byte b;

	if ((SDMACTL&0x88)==0x80)
	{
		i=(SDMACH<<8)|SDMACL;
		j=(SDMASB<<16)|(SDMASH<<8)|SDMASL;
		//b=cpu_readmem20(j);
		b=chip->ws_internalRam[j&0x3FFF];

		chip->ws_ioRam[0x89]=b;
		i--;
		j++;
		if(i<32)
		{
			i=0;
			SDMACTL&=0x7F;
		}
		else
		{
			// set DMA timer
			//ws_timer_set(2, DMACycles[SDMACTL&3]);
		}
		SDMASB=(byte)((j>>16)&0xFF);
		SDMASH=(byte)((j>>8)&0xFF);
		SDMASL=(byte)(j&0xFF);
		SDMACH=(byte)((i>>8)&0xFF);
		SDMACL=(byte)(i&0xFF);
	}
}

static void ws_write_ram_byte(void* info, UINT16 offset, UINT8 value)
{
	wsa_state* chip = (wsa_state*)info;
	
	// RAM - 16 KB (WS) / 64 KB (WSC) internal RAM
	chip->ws_internalRam[offset & 0x3FFF] = value;
	return;
}

static UINT8 ws_read_ram_byte(void* info, UINT16 offset)
{
	wsa_state* chip = (wsa_state*)info;
	
	return chip->ws_internalRam[offset & 0x3FFF];
}

static void ws_set_mute_mask(void* info, UINT32 MuteMask)
{
	wsa_state* chip = (wsa_state*)info;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 4; CurChn ++)
		chip->ws_audio[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
