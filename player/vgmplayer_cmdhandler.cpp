// VGM player command handling functions
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INLINE	static inline

#include "../common_def.h"
#include "vgmplayer.hpp"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"	// for SndEmu_GetDeviceFunc()
#include "../emu/dac_control.h"
#include "../emu/cores/sn764intf.h"	// for SN76496_W constants

#include "dblk_compr.h"
#include "helper.h"

#define fData	(&_fileData[_filePos])	// used by command handlers for better readability

/*static*/ const VGMPlayer::COMMAND_INFO VGMPlayer::_CMD_INFO[0x100] =
{
	// {chip type, function},                         VGM command
	{0xFF, 0x01, &VGMPlayer::Cmd_unknown},              // 00
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 01
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 02
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 03
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 04
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 05
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 06
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 07
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 08
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 09
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0A
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0B
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0C
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0D
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0E
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 0F
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 10
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 11
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 12
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 13
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 14
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 15
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 16
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 17
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 18
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 19
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1A
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1B
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1C
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1D
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1E
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 1F
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 20
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 21
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 22
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 23
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 24
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 25
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 26
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 27
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 28
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 29
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2A
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2B
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2C
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2D
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2E
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 2F
	{0x00, 0x02, &VGMPlayer::Cmd_SN76489},              // 30 SN76489 register write (2nd chip)
	{0xFF, 0x02, &VGMPlayer::Cmd_AY_Stereo},            // 31 AY8910 stereo mask [chip type depends on data]
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 32
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 33
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 34
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 35
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 36
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 37
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 38
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 39
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 3A
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 3B
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 3C
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 3D
	{0xFF, 0x02, &VGMPlayer::Cmd_unknown},              // 3E
	{0x00, 0x02, &VGMPlayer::Cmd_GGStereo},             // 3F GameGear stereo mask (2nd chip)
	{0x29, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // 40 Mikey register write
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 41
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 42
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 43
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 44
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 45
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 46
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 47
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 48
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 49
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 4A
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 4B
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 4C
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 4D
	{0xFF, 0x03, &VGMPlayer::Cmd_unknown},              // 4E
	{0x00, 0x02, &VGMPlayer::Cmd_GGStereo},             // 4F GameGear stereo mask
	{0x00, 0x02, &VGMPlayer::Cmd_SN76489},              // 50 SN76489 register write
	{0x01, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 51 YM2413 register write
	{0x02, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 52 YM2612 register write, port 0
	{0x02, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 53 YM2612 register write, port 1
	{0x03, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 54 YM2151 register write
	{0x06, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 55 YM2203 register write
	{0x07, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 56 YM2608 register write, port 0
	{0x07, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 57 YM2608 register write, port 1
	{0x08, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 58 YM2610 register write, port 0
	{0x08, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 59 YM2610 register write, port 1
	{0x09, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 5A YM3812 register write
	{0x0A, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 5B YM3526 register write
	{0x0B, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 5C Y8950 register write
	{0x0F, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // 5D YMZ280 register write
	{0x0C, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 5E YMF262 register write, port 0
	{0x0C, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // 5F YMF262 register write, port 1
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 60
	{0xFF, 0x03, &VGMPlayer::Cmd_DelaySamples2B},       // 61 wait N samples (2 byte parameter)
	{0xFF, 0x01, &VGMPlayer::Cmd_Delay60Hz},            // 62 wait 735 samples (1/60 second)
	{0xFF, 0x01, &VGMPlayer::Cmd_Delay50Hz},            // 63 wait 882 samples (1/50 second)
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 64
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 65
	{0xFF, 0x00, &VGMPlayer::Cmd_EndOfData},            // 66 end of command data [length 0 due to special handling]
	{0xFF, 0x00, &VGMPlayer::Cmd_DataBlock},            // 67 data block [length 0 due to special handling]
	{0xFF, 0x0C, &VGMPlayer::Cmd_PcmRamWrite},          // 68 PCM RAM write
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 69
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6A
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6B
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6C
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6D
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6E
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 6F
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 70 wait 1 sample
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 71 wait 2 samples
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 72 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 73 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 74 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 75 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 76 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 77 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 78 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 79 .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7A .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7B .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7C .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7D .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7E .
	{0xFF, 0x01, &VGMPlayer::Cmd_DelaySamplesN1},       // 7F wait 16 samples
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 80 write YM2612 PCM data from data block 0
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 81 write YM2612 PCM data from data block 0, then wait 1 sample
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 82 write YM2612 PCM data from data block 0, then wait 2 samples
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 83 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 84 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 85 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 86 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 87 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 88 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 89 .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8A .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8B .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8C .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8D .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8E .
	{0x02, 0x01, &VGMPlayer::Cmd_YM2612PCM_Delay},      // 8F write YM2612 PCM data from data block 0, then wait 15 samples
	{0xFF, 0x05, &VGMPlayer::Cmd_DACCtrl_Setup},        // 90 DAC Stream Control: Setup Chip
	{0xFF, 0x05, &VGMPlayer::Cmd_DACCtrl_SetData},      // 91 DAC Stream Control: Set Data
	{0xFF, 0x06, &VGMPlayer::Cmd_DACCtrl_SetFrequency}, // 92 DAC Stream Control: Set Playback Frequency
	{0xFF, 0x0B, &VGMPlayer::Cmd_DACCtrl_PlayData_Loc}, // 93 DAC Stream Control: Play Data (using start positon/length)
	{0xFF, 0x02, &VGMPlayer::Cmd_DACCtrl_Stop},         // 94 DAC Stream Control: Stop
	{0xFF, 0x05, &VGMPlayer::Cmd_DACCtrl_PlayData_Blk}, // 95 DAC Stream Control: Play Data (single data block)
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 96
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 97
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 98
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 99
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9A
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9B
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9C
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9D
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9E
	{0xFF, 0x00, &VGMPlayer::Cmd_invalid},              // 9F
	{0x12, 0x03, &VGMPlayer::Cmd_DReg8_Data8},          // A0 AY8910 register write
	{0x01, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // A1 YM2413 register write (2nd chip)
	{0x02, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A2 YM2612 register write, port 0 (2nd chip)
	{0x02, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A3 YM2612 register write, port 1 (2nd chip)
	{0x03, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // A4 YM2151 register write (2nd chip)
	{0x06, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // A5 YM2203 register write (2nd chip)
	{0x07, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A6 YM2608 register write, port 0 (2nd chip)
	{0x07, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A7 YM2608 register write, port 1 (2nd chip)
	{0x08, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A8 YM2610 register write, port 0 (2nd chip)
	{0x08, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // A9 YM2610 register write, port 1 (2nd chip)
	{0x09, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // AA YM3812 register write (2nd chip)
	{0x0A, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // AB YM3526 register write (2nd chip)
	{0x0B, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // AC Y8950 register write (2nd chip)
	{0x0F, 0x03, &VGMPlayer::Cmd_Reg8_Data8},           // AD YMZ280 register write (2nd chip)
	{0x0C, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // AE YMF262 register write, port 0 (2nd chip)
	{0x0C, 0x03, &VGMPlayer::Cmd_CPort_Reg8_Data8},     // AF YMF262 register write, port 1 (2nd chip)
	{0x05, 0x03, &VGMPlayer::Cmd_RF5C_Reg},             // B0 RF5C68 register write
	{0x10, 0x03, &VGMPlayer::Cmd_RF5C_Reg},             // B1 RF5C164 register write
	{0x11, 0x03, &VGMPlayer::Cmd_PWM_Reg},              // B2 PWM register write
	{0x13, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // B3 GameBoy DMG register write
	{0x14, 0x03, &VGMPlayer::Cmd_NES_Reg},              // B4 NES APU register write
	{0x15, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // B5 YMW258 (MultiPCM) register write
	{0x16, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // B6 uPD7759 register write
	{0x17, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // B7 OKIM6258 register write
	{0x18, 0x03, &VGMPlayer::Cmd_OKIM6295_Reg},         // B8 OKIM6295 register write
	{0x1B, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // B9 HuC6280 register write
	{0x1D, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // BA K053260 register write
	{0x1E, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // BB Pokey register write
	{0x21, 0x03, &VGMPlayer::Cmd_WSwan_Reg},            // BC WonderSwan register write
	{0x23, 0x03, &VGMPlayer::Cmd_SAA_Reg},              // BD SAA1099 register write
	{0x25, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // BE ES5506 register write (8-bit data)
	{0x28, 0x03, &VGMPlayer::Cmd_Ofs8_Data8},           // BF GA20 register write
	{0x04, 0x04, &VGMPlayer::Cmd_SegaPCM_Mem},          // C0 Sega PCM memory write
	{0x05, 0x04, &VGMPlayer::Cmd_RF5C_Mem},             // C1 RF5C68 memory write
	{0x10, 0x04, &VGMPlayer::Cmd_RF5C_Mem},             // C2 RF5C164 memory write
	{0x15, 0x04, &VGMPlayer::Cmd_YMW_Bank},             // C3 set YMW258 (MultiPCM) bank
	{0x1F, 0x04, &VGMPlayer::Cmd_QSound_Reg},           // C4 QSound register write
	{0x20, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // C5 SCSP register write
	{0x21, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // C6 WonderSwan memory write
	{0x22, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // C7 VSU-VUE (Virtual Boy) register write
	{0x26, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // C8 X1-010 register write
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // C9
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CA
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CB
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CC
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CD
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CE
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // CF
	{0x0D, 0x04, &VGMPlayer::Cmd_Port_Reg8_Data8},      // D0 YMF278B register write
	{0x0E, 0x04, &VGMPlayer::Cmd_Port_Reg8_Data8},      // D1 YMF271 register write
	{0x19, 0x04, &VGMPlayer::Cmd_Port_Reg8_Data8},      // D2 K051649 (SCC1) register write
	{0x1A, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // D3 K054539 register write
	{0x1C, 0x04, &VGMPlayer::Cmd_Ofs16_Data8},          // D4 C140 register write
	{0x24, 0x04, &VGMPlayer::Cmd_Port_Ofs8_Data8},      // D5 ES5503 register write
	{0x25, 0x04, &VGMPlayer::Cmd_Ofs8_Data16},          // D6 ES5506 register write (16-bit data)
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // D7
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // D8
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // D9
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DA
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DB
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DC
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DD
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DE
	{0xFF, 0x04, &VGMPlayer::Cmd_unknown},              // DF
	{0xFF, 0x05, &VGMPlayer::Cmd_YM2612PCM_Seek},       // E0 set YM2612 PCM data offset
	{0x27, 0x05, &VGMPlayer::Cmd_Ofs16_Data16},         // E1 C352 register write
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E2
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E3
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E4
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E5
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E6
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E7
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E8
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // E9
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // EA
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // EB
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // EC
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // ED
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // EE
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // EF
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F0
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F1
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F2
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F3
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F4
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F5
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F6
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F7
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F8
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // F9
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FA
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FB
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FC
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FD
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FE
	{0xFF, 0x05, &VGMPlayer::Cmd_unknown},              // FF
};

/*static*/ const UINT8 VGMPlayer::_VGM_BANK_CHIPS[VGMPlayer::_PCM_BANK_COUNT] =
{
	0x02,	// 00 YM2612
	0x05,	// 01 RF5C68
	0x10,	// 02 RF5C164
	0x11,	// 03 PWM
	0x17,	// 04 OKIM6258
	0x1B,	// 05 HuC6280
	0x20,	// 06 SCSP
	0x14,	// 07 NES APU
	0xFF,	// 08
	0xFF,	// 09
	0xFF,	// 0A
	0xFF,	// 0B
	0xFF,	// 0C
	0xFF,	// 0D
	0xFF,	// 0E
	0xFF,	// 0F
	0xFF,	// 10
	0xFF,	// 11
	0xFF,	// 12
	0xFF,	// 13
	0xFF,	// 14
	0xFF,	// 15
	0xFF,	// 16
	0xFF,	// 17
	0xFF,	// 18
	0xFF,	// 19
	0xFF,	// 1A
	0xFF,	// 1B
	0xFF,	// 1C
	0xFF,	// 1D
	0xFF,	// 1E
	0xFF,	// 1F
	0xFF,	// 20
	0xFF,	// 21
	0xFF,	// 22
	0xFF,	// 23
	0xFF,	// 24
	0xFF,	// 25
	0xFF,	// 26
	0xFF,	// 27
	0xFF,	// 28
	0xFF,	// 29
	0xFF,	// 2A
	0xFF,	// 2B
	0xFF,	// 2C
	0xFF,	// 2D
	0xFF,	// 2E
	0xFF,	// 2F
	0xFF,	// 30
	0xFF,	// 31
	0xFF,	// 32
	0xFF,	// 33
	0xFF,	// 34
	0xFF,	// 35
	0xFF,	// 36
	0xFF,	// 37
	0xFF,	// 38
	0xFF,	// 39
	0xFF,	// 3A
	0xFF,	// 3B
	0xFF,	// 3C
	0xFF,	// 3D
	0xFF,	// 3E
	0xFF,	// 3F
};

/*static*/ const UINT8 VGMPlayer::_VGM_ROM_CHIPS[0x40][2] =
{
// {chipType, memIdx}
	{0x04, 0},	// 80 SegaPCM
	{0x07, 0},	// 81 YM2608 ADPCM-B (DeltaT)
	{0x08, 0},	// 82 YM2610 ADPCM-A
	{0x08, 1},	// 83 YM2610 ADPCM-B (DeltaT)
	{0x0D, 0},	// 84 YMF278B ROM
	{0x0E, 0},	// 85 YMF271
	{0x0F, 0},	// 86 YMZ280B
	{0x0D, 1},	// 87 YMF278B RAM
	{0x0B, 0},	// 88 Y8950 DeltaT
	{0x15, 0},	// 89 YMW258 (MultiPCM)
	{0x16, 0},	// 8A uPD7759
	{0x18, 0},	// 8B OKIM6295
	{0x1A, 0},	// 8C K054539
	{0x1C, 0},	// 8D C140
	{0x1D, 0},	// 8E K053260
	{0x1F, 0},	// 8F QSound
	{0x25, 0},	// 90 ES5506
	{0x26, 0},	// 91 X1-010
	{0x27, 0},	// 92 C352
	{0x28, 0},	// 93 GA20
	{0xFF, 0},	// 94
	{0xFF, 0},	// 95
	{0xFF, 0},	// 96
	{0xFF, 0},	// 97
	{0xFF, 0},	// 98
	{0xFF, 0},	// 99
	{0xFF, 0},	// 9A
	{0xFF, 0},	// 9B
	{0xFF, 0},	// 9C
	{0xFF, 0},	// 9D
	{0xFF, 0},	// 9E
	{0xFF, 0},	// 9F
	{0xFF, 0},	// A0
	{0xFF, 0},	// A1
	{0xFF, 0},	// A2
	{0xFF, 0},	// A3
	{0xFF, 0},	// A4
	{0xFF, 0},	// A5
	{0xFF, 0},	// A6
	{0xFF, 0},	// A7
	{0xFF, 0},	// A8
	{0xFF, 0},	// A9
	{0xFF, 0},	// AA
	{0xFF, 0},	// AB
	{0xFF, 0},	// AC
	{0xFF, 0},	// AD
	{0xFF, 0},	// AE
	{0xFF, 0},	// AF
	{0xFF, 0},	// B0
	{0xFF, 0},	// B1
	{0xFF, 0},	// B2
	{0xFF, 0},	// B3
	{0xFF, 0},	// B4
	{0xFF, 0},	// B5
	{0xFF, 0},	// B6
	{0xFF, 0},	// B7
	{0xFF, 0},	// B8
	{0xFF, 0},	// B9
	{0xFF, 0},	// BA
	{0xFF, 0},	// BB
	{0xFF, 0},	// BC
	{0xFF, 0},	// BD
	{0xFF, 0},	// BE
	{0xFF, 0},	// BF
};

/*static*/ const UINT8 VGMPlayer::_VGM_RAM_CHIPS[0x40] =
{
	// --- RAM --- (16-bit addressing)
	0x05,	// C0 RF5C68
	0x10,	// C1 RF5C164
	0x14,	// C2 NES APU
	0xFF,	// C3
	0xFF,	// C4
	0xFF,	// C5
	0xFF,	// C6
	0xFF,	// C7
	0xFF,	// C8
	0xFF,	// C9
	0xFF,	// CA
	0xFF,	// CB
	0xFF,	// CC
	0xFF,	// CD
	0xFF,	// CE
	0xFF,	// CF
	0xFF,	// D0
	0xFF,	// D1
	0xFF,	// D2
	0xFF,	// D3
	0xFF,	// D4
	0xFF,	// D5
	0xFF,	// D6
	0xFF,	// D7
	0xFF,	// D8
	0xFF,	// D9
	0xFF,	// DA
	0xFF,	// DB
	0xFF,	// DC
	0xFF,	// DD
	0xFF,	// DE
	0xFF,	// DF
	// --- RAM --- (32-bit addressing)
	0x20,	// E0 SCSP
	0x24,	// E1 ES5503
	0xFF,	// E2
	0xFF,	// E3
	0xFF,	// E4
	0xFF,	// E5
	0xFF,	// E6
	0xFF,	// E7
	0xFF,	// E8
	0xFF,	// E9
	0xFF,	// EA
	0xFF,	// EB
	0xFF,	// EC
	0xFF,	// ED
	0xFF,	// EE
	0xFF,	// EF
	0xFF,	// F0
	0xFF,	// F1
	0xFF,	// F2
	0xFF,	// F3
	0xFF,	// F4
	0xFF,	// F5
	0xFF,	// F6
	0xFF,	// F7
	0xFF,	// F8
	0xFF,	// F9
	0xFF,	// FA
	0xFF,	// FB
	0xFF,	// FC
	0xFF,	// FD
	0xFF,	// FE
	0xFF,	// FF reserved
};


INLINE UINT16 ReadLE16(const UINT8* data)
{
	// read 16-Bit Word (Little Endian/Intel Byte Order)
#ifdef VGM_LITTLE_ENDIAN
	return *(UINT16*)data;
#else
	return (data[0x01] << 8) | (data[0x00] << 0);
#endif
}

INLINE UINT16 ReadBE16(const UINT8* data)
{
	// read 16-Bit Word (Big Endian/Motorola Byte Order)
#ifdef VGM_BIG_ENDIAN
	return *(UINT16*)data;
#else
	return (data[0x00] << 8) | (data[0x01] << 0);
#endif
}

INLINE UINT32 ReadLE24(const UINT8* data)
{
	// read 24-Bit Word (Little Endian/Intel Byte Order)
#ifdef VGM_LITTLE_ENDIAN
	return	(*(UINT32*)data) & 0x00FFFFFF;
#else
	return	(data[0x02] << 16) | (data[0x01] <<  8) | (data[0x00] <<  0);
#endif
}

INLINE UINT32 ReadLE32(const UINT8* data)
{
	// read 32-Bit Word (Little Endian/Intel Byte Order)
#ifdef VGM_LITTLE_ENDIAN
	return	*(UINT32*)data;
#else
	return	(data[0x03] << 24) | (data[0x02] << 16) |
			(data[0x01] <<  8) | (data[0x00] <<  0);
#endif
}

void VGMPlayer::Cmd_invalid(void)
{
	_playState |= PLAYSTATE_END;
	_psTrigger |= PLAYSTATE_END;
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
	emu_logf(&_logger, PLRLOG_ERROR, "Invalid VGM command %02X found! (filePos 0x%06X)\n", fData[0x00], _filePos);
	return;
}

void VGMPlayer::Cmd_unknown(void)
{
	// The difference between "invalid" and "unknown" is, that there is a command length
	// defined for "unknown" and thus it is possible to just skip it.
	UINT8 cmdID = fData[0x00];
	if (_shownCmdWarnings[cmdID] < 10)
	{
		_shownCmdWarnings[cmdID] ++;
		emu_logf(&_logger, PLRLOG_WARN, "Unknown VGM command %02X found! (filePos 0x%06X)\n", cmdID, _filePos);
	}
	return;
}

void VGMPlayer::Cmd_EndOfData(void)
{
	UINT8 silenceStop = 0;
	UINT8 doLoop = (_fileHdr.loopOfs != 0);
	
	if (_playState & PLAYSTATE_SEEK)	// recalculate playSmpl to fix state when triggering callbacks
		_playSmpl = Tick2Sample(_fileTick);	// Note: fileTick results in more accurate position
	if (doLoop)
	{
		if (_lastLoopTick == _fileTick)
		{
			doLoop = 0;	// prevent freezing due to infinite loop
			emu_logf(&_logger, PLRLOG_WARN, "Ignored Zero-Sample-Loop!\n");
		}
		else
		{
			_lastLoopTick = _fileTick;
		}
	}
	if (doLoop)
	{
		_curLoop ++;
		if (_eventCbFunc != NULL)
		{
			UINT8 retVal = _eventCbFunc(this, _eventCbParam, PLREVT_LOOP, &_curLoop);
			if (retVal == 0x01)	// "stop" signal?
			{
				_playState |= PLAYSTATE_END;
				_psTrigger |= PLAYSTATE_END;
				if (_eventCbFunc != NULL)
					_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
				return;
			}
		}
		_filePos = _fileHdr.loopOfs;
		return;
	}
	
	_playState |= PLAYSTATE_END;
	_psTrigger |= PLAYSTATE_END;
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
	
	if (_playOpts.hardStopOld)
	{
		silenceStop |= (_fileHdr.fileVer < 0x150) << 0;
		silenceStop |= (_fileHdr.fileVer == 0x150 && _playOpts.hardStopOld == 2) << 1;
	}
	if (silenceStop)
	{
		size_t curDev;
		for (curDev = 0; curDev < _devices.size(); curDev ++)
		{
			DEV_INFO* devInf = &_devices[curDev].base.defInf;
			devInf->devDef->Reset(devInf->dataPtr);
		}
	}
	
	return;
}

void VGMPlayer::Cmd_DelaySamples2B(void)
{
	_fileTick += ReadLE16(&fData[0x01]);
	return;
}

void VGMPlayer::Cmd_Delay60Hz(void)
{
	_fileTick += 735;	// 44100/60
	return;
}

void VGMPlayer::Cmd_Delay50Hz(void)
{
	_fileTick += 882;	// 44100/50
	return;
}

void VGMPlayer::Cmd_DelaySamplesN1(void)
{
	_fileTick += 1 + (fData[0x00] & 0x0F);
	return;
}

INLINE void SendYMCommand(VGMPlayer::CHIP_DEVICE* cDev, UINT8 port, UINT8 reg, UINT8 data)
{
	cDev->write8(cDev->base.defInf.dataPtr, (port << 1) | 0, reg);
	cDev->write8(cDev->base.defInf.dataPtr, (port << 1) | 1, data);
	return;
}

static void WriteChipROM(VGMPlayer::CHIP_DEVICE* cDev, UINT8 memID,
						 UINT32 memSize, UINT32 dataOfs, UINT32 dataLen, const UINT8* data)
{
	if (memID == 0)
	{
		if (cDev->romSize != NULL)
			cDev->romSize(cDev->base.defInf.dataPtr, memSize);
		if (cDev->romWrite != NULL && dataLen)
			cDev->romWrite(cDev->base.defInf.dataPtr, dataOfs, dataLen, data);
	}
	else
	{
		if (cDev->romSizeB != NULL)
			cDev->romSizeB(cDev->base.defInf.dataPtr, memSize);
		if (cDev->romWriteB != NULL && dataLen)
			cDev->romWriteB(cDev->base.defInf.dataPtr, dataOfs, dataLen, data);
	}
	
	return;
}

void VGMPlayer::DoRAMOfsPatches(UINT8 chipType, UINT8 chipID, UINT32& dataOfs, UINT32& dataLen)
{
	switch(chipType)
	{
	case 0x05:	// RF5C68
		dataOfs |= (_rf5cBank[0][chipID] << 12);	// add current memory bank to offset
		break;
	case 0x10:	// RF5C164
		dataOfs |= (_rf5cBank[1][chipID] << 12);	// add current memory bank to offset
		break;
	}
	
	return;
}

void VGMPlayer::Cmd_DataBlock(void)
{
	UINT8 dblkType;
	UINT8 chipType;
	UINT8 chipID;
	UINT32 dblkLen;
	UINT32 memSize;
	UINT32 dataOfs;
	UINT32 dataLen;
	const UINT8* dataPtr;
	CHIP_DEVICE* cDev;
	
	dblkType = fData[0x02];
	dblkLen = ReadLE32(&fData[0x03]);
	chipID = (dblkLen & 0x80000000) >> 31;
	dblkLen &= 0x7FFFFFFF;
	_filePos += 0x07;
	
	switch(dblkType & 0xC0)
	{
	case 0x00:	// uncompressed data block
	case 0x40:	// compressed data block
		if (_curLoop > 0)
			return;	// skip during the 2nd/3rd/... loop, as these blocks were already loaded
		
		if (dblkType == 0x7F)
		{
			ReadPCMComprTable(dblkLen, &fData[0x00], &_pcmComprTbl);
		}
		else
		{
			PCM_BANK* pcmBnk = &_pcmBank[dblkType & 0x3F];
			PCM_CDB_INF dbCI;
			UINT32 oldLen = (UINT32)pcmBnk->data.size();
			dataLen = dblkLen;
			dataPtr = &fData[0x00];
			
			if (dblkType & 0x40)
			{
				ReadComprDataBlkHdr(dblkLen, dataPtr, &dbCI);
				dbCI.cmprInfo.comprTbl = &_pcmComprTbl;
				dataLen = dbCI.decmpLen;
			}
			
			pcmBnk->bankOfs.push_back(oldLen);
			pcmBnk->bankSize.push_back(dataLen);
			
			pcmBnk->data.resize(oldLen + dataLen);
			if (dblkType & 0x40)
			{
				UINT8 retVal = DecompressDataBlk(dataLen, &pcmBnk->data[oldLen],
					dblkLen - dbCI.hdrSize, &dataPtr[dbCI.hdrSize], &dbCI.cmprInfo);
				if (retVal == 0x10)
					emu_logf(&_logger, PLRLOG_ERROR, "Error loading table-compressed data block! No table loaded!\n");
				else if (retVal == 0x11)
					emu_logf(&_logger, PLRLOG_ERROR, "Data block and loaded value table incompatible!\n");
				else if (retVal == 0x80)
					emu_logf(&_logger, PLRLOG_ERROR, "Unknown data block compression!\n");
			}
			else
			{
				memcpy(&pcmBnk->data[oldLen], dataPtr, dataLen);
			}
			
			// TODO: refresh DAC Stream pointers (call daccontrol_refresh_data)
		}
		break;
	case 0x80:	// ROM/RAM write
		chipType = _VGM_ROM_CHIPS[dblkType & 0x3F][0];
		cDev = GetDevicePtr(chipType, chipID);
		if (cDev == NULL)
			break;
		
		memSize = ReadLE32(&fData[0x00]);
		dataOfs = ReadLE32(&fData[0x04]);
		dataPtr = &fData[0x08];
		dataLen = dblkLen - 0x08;
		if (chipType == 0x1C && dataLen && (cDev->flags & 0x01))
		{
			// chip == ASIC 219 (ID 0x1C + flags 0x01): byte-swap sample data
			dataLen &= ~0x01;
			std::vector<UINT8> swpData(dataLen);
			for (UINT32 curPos = 0x00; curPos < dataLen; curPos += 0x02)
			{
				swpData[curPos + 0x00] = dataPtr[curPos + 0x01];
				swpData[curPos + 0x01] = dataPtr[curPos + 0x00];
			}
			WriteChipROM(cDev, _VGM_ROM_CHIPS[dblkType & 0x3F][1], memSize, dataOfs, dataLen, &swpData[0x00]);
		}
		else
		{
			WriteChipROM(cDev, _VGM_ROM_CHIPS[dblkType & 0x3F][1], memSize, dataOfs, dataLen, dataPtr);
		}
		break;
	case 0xC0:	// RAM Write
		chipType = _VGM_RAM_CHIPS[dblkType & 0x3F];
		cDev = GetDevicePtr(chipType, chipID);
		if (cDev == NULL || cDev->romWrite == NULL)
			break;
		
		if (! (dblkType & 0x20))
		{
			// C0..DF: 16-bit addressing
			dataOfs = ReadLE16(&fData[0x00]);
			dataLen = dblkLen - 0x02;
			dataPtr = &fData[0x02];
		}
		else
		{
			// E0..FF: 32-bit addressing
			dataOfs = ReadLE32(&fData[0x00]);
			dataLen = dblkLen - 0x04;
			dataPtr = &fData[0x04];
		}
		DoRAMOfsPatches(chipType, chipID, dataOfs, dataLen);
		cDev->romWrite(cDev->base.defInf.dataPtr, dataOfs, dataLen, dataPtr);
		break;
	}
	
	_filePos += dblkLen;
	return;
}

void VGMPlayer::Cmd_PcmRamWrite(void)
{
	UINT8 dbType = fData[0x02] & 0x7F;
	if (dbType >= _PCM_BANK_COUNT)
		return;
	
	UINT8 chipType = _VGM_BANK_CHIPS[dbType];
	UINT8 chipID = (fData[0x02] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->romWrite == NULL)
		return;
	
	UINT32 dbPos = ReadLE24(&fData[0x03]);
	UINT32 wrtAddr = ReadLE24(&fData[0x06]);
	UINT32 dataLen = ReadLE24(&fData[0x09]);
	if (dbPos >= _pcmBank[dbType].data.size())
		return;
	const UINT8* ROMData = &_pcmBank[dbType].data[dbPos];
	if (! dataLen)
		dataLen += 0x01000000;
	
	if (chipType == 0x14)	// NES APU
	{
		//Last95Drum = dbPos / dataLen - 1;
		//Last95Max = _pcmBank[dbType].data.size() / dataLen;
	}
	
	DoRAMOfsPatches(chipType, chipID, wrtAddr, dataLen);
	cDev->romWrite(cDev->base.defInf.dataPtr, wrtAddr, dataLen, ROMData);
	
	return;
}

void VGMPlayer::Cmd_YM2612PCM_Delay(void)
{
	CHIP_DEVICE* cDev = GetDevicePtr(0x02, 0);
	_fileTick += (fData[0x00] & 0x0F);
	
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	if (_ym2612pcm_bnkPos >= _pcmBank[0].data.size())
		return;
	
	UINT8 data = _pcmBank[0].data[_ym2612pcm_bnkPos];
	SendYMCommand(cDev, 0x00, 0x2A, data);
	_ym2612pcm_bnkPos ++;
	// TODO: clip when exceeding pcmBank size
	
	return;
}

void VGMPlayer::Cmd_YM2612PCM_Seek(void)
{
	_ym2612pcm_bnkPos = ReadLE32(&fData[0x01]);
	return;
}

void VGMPlayer::Cmd_DACCtrl_Setup(void)	// DAC Stream Control: Setup Chip
{
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
	{
		if (fData[0x01] == 0xFF)
			return;
		
		DEV_GEN_CFG devCfg;
		DACSTRM_DEV dacStrm;
		UINT8 retVal;
		
		devCfg.emuCore = 0x00;
		devCfg.srMode = DEVRI_SRMODE_NATIVE;
		devCfg.flags = 0x00;
		devCfg.clock = 0;
		devCfg.smplRate = _outSmplRate;
		retVal = device_start_daccontrol(&devCfg, &dacStrm.defInf);
		if (retVal)
			return;
		dacStrm.defInf.devDef->Reset(dacStrm.defInf.dataPtr);
		dacStrm.streamID = fData[0x01];
		dacStrm.bankID = 0xFF;
		dacStrm.pbMode = 0x00;
		dacStrm.freq = 0;
		dacStrm.lastItem = (UINT32)-1;
		dacStrm.maxItems = 0;
		
		_dacStrmMap[dacStrm.streamID] = _dacStreams.size();
		_dacStreams.push_back(dacStrm);
		
		dsID = _dacStrmMap[dacStrm.streamID];
	}
	
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	UINT8 chipType = fData[0x02] & 0x7F;
	UINT8 chipID = (fData[0x02] & 0x80) >> 7;
	UINT16 chipCmd = ReadBE16(&fData[0x03]);
	CHIP_DEVICE* destChip = GetDevicePtr(chipType, chipID);
	if (destChip == NULL)
		return;
	
	daccontrol_setup_chip(dacStrm->defInf.dataPtr, &destChip->base.defInf, destChip->chipType, chipCmd);
	return;
}

void VGMPlayer::Cmd_DACCtrl_SetData(void)	// DAC Stream Control: Set Data Bank
{
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
		return;
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	
	dacStrm->bankID = fData[0x02];
	if (dacStrm->bankID >= _PCM_BANK_COUNT)
		return;
	PCM_BANK* pcmBnk = &_pcmBank[dacStrm->bankID];
	
	dacStrm->maxItems = (UINT32)pcmBnk->bankOfs.size();
	if (pcmBnk->data.empty())
		daccontrol_set_data(dacStrm->defInf.dataPtr, NULL, 0, fData[0x03], fData[0x04]);
	else
		daccontrol_set_data(dacStrm->defInf.dataPtr, &pcmBnk->data[0], (UINT32)pcmBnk->data.size(), fData[0x03], fData[0x04]);
	return;
}

void VGMPlayer::Cmd_DACCtrl_SetFrequency(void)	// DAC Stream Control: Set Frequency
{
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
		return;
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	
	dacStrm->freq = ReadLE32(&fData[0x02]);
	daccontrol_set_frequency(dacStrm->defInf.dataPtr, dacStrm->freq);
	return;
}

void VGMPlayer::Cmd_DACCtrl_PlayData_Loc(void)	// DAC Stream Control: Play Data (verbose, by specifying location/size)
{
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
		return;
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	
	UINT32 startOfs = ReadLE32(&fData[0x02]);
	UINT32 soundLen = ReadLE32(&fData[0x07]);
	dacStrm->lastItem = (UINT32)-1;
	dacStrm->pbMode = fData[0x06];
	daccontrol_start(dacStrm->defInf.dataPtr, startOfs, dacStrm->pbMode, soundLen);
	return;
}

void VGMPlayer::Cmd_DACCtrl_Stop(void)	// DAC Stream Control: Stop immediately
{
	if (fData[0x01] == 0xFF)
	{
		for (size_t curStrm = 0; curStrm < _dacStreams.size(); curStrm++)
		{
			DACSTRM_DEV* dacStrm = &_dacStreams[curStrm];
			dacStrm->lastItem = (UINT32)-1;
			daccontrol_stop(dacStrm->defInf.dataPtr);
		}
		return;
	}
	
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
		return;
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	
	dacStrm->lastItem = (UINT32)-1;
	daccontrol_stop(dacStrm->defInf.dataPtr);
	return;
}

void VGMPlayer::Cmd_DACCtrl_PlayData_Blk(void)	// DAC Stream Control: Play Data Block (using sound ID)
{
	size_t dsID = _dacStrmMap[fData[0x01]];
	if (dsID == (size_t)-1)
		return;
	DACSTRM_DEV* dacStrm = &_dacStreams[dsID];
	if (dacStrm->bankID >= _PCM_BANK_COUNT)
		return;
	PCM_BANK* pcmBnk = &_pcmBank[dacStrm->bankID];
	
	UINT16 sndID = ReadLE16(&fData[0x02]);
	dacStrm->lastItem = sndID;
	dacStrm->maxItems = (UINT32)pcmBnk->bankOfs.size();
	if (sndID >= pcmBnk->bankOfs.size())
		return;
	UINT32 startOfs = pcmBnk->bankOfs[sndID];
	UINT32 soundLen = pcmBnk->bankSize[sndID];
	dacStrm->pbMode = DCTRL_LMODE_BYTES |
					((fData[0x04] & 0x10) << 0) |	// Reverse Mode
					((fData[0x04] & 0x01) << 7);	// Looping
	daccontrol_start(dacStrm->defInf.dataPtr, startOfs, dacStrm->pbMode, soundLen);
	return;
}

void VGMPlayer::Cmd_GGStereo(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x00] == 0x3F) ? 1 : 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, SN76496_W_GGST, fData[0x01]);
	return;
}

void VGMPlayer::Cmd_SN76489(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x00] == 0x30) ? 1 : 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, SN76496_W_REG, fData[0x01]);
	return;
}

void VGMPlayer::Cmd_Reg8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x00] >= 0xA0) ? 1 : 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	SendYMCommand(cDev, 0, fData[0x01], fData[0x02]);
	return;
}

void VGMPlayer::Cmd_CPort_Reg8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x00] >= 0xA0) ? 1 : 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	SendYMCommand(cDev, fData[0x00] & 0x01, fData[0x01], fData[0x02]);
	return;
}

void VGMPlayer::Cmd_Port_Reg8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	SendYMCommand(cDev, fData[0x01] & 0x7F, fData[0x02], fData[0x03]);
	return;
}

void VGMPlayer::Cmd_Ofs8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, fData[0x01] & 0x7F, fData[0x02]);
	return;
}

void VGMPlayer::Cmd_Ofs16_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeM8 == NULL)
		return;
	
	UINT16 ofs = ReadBE16(&fData[0x01]) & 0x7FFF;
	cDev->writeM8(cDev->base.defInf.dataPtr, ofs, fData[0x03]);
	return;
}

void VGMPlayer::Cmd_Ofs8_Data16(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeD16 == NULL)
		return;
	
	UINT16 value = ReadLE16(&fData[0x02]);
	cDev->writeD16(cDev->base.defInf.dataPtr, fData[0x01] & 0x7F, value);
	return;
}

void VGMPlayer::Cmd_Ofs16_Data16(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeM16 == NULL)
		return;
	
	UINT16 ofs = ReadBE16(&fData[0x01]) & 0x7FFF;
	UINT16 value = ReadBE16(&fData[0x03]);
	cDev->writeM16(cDev->base.defInf.dataPtr, ofs, value);
	return;
}

void VGMPlayer::Cmd_Port_Ofs8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, fData[0x02], fData[0x03]);
	return;
}

void VGMPlayer::Cmd_DReg8_Data8(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	SendYMCommand(cDev, 0, fData[0x01] & 0x7F, fData[0x02]);
	return;
}

void VGMPlayer::Cmd_SegaPCM_Mem(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x02] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeM8 == NULL)
		return;
	
	UINT16 memOfs = ReadLE16(&fData[0x01]) & 0x7FFF;
	cDev->writeM8(cDev->base.defInf.dataPtr, memOfs, fData[0x03]);
	return;
}

void VGMPlayer::Cmd_RF5C_Mem(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeM8 == NULL)
		return;
	
	UINT16 memOfs = ReadLE16(&fData[0x01]);
	if (memOfs & 0xF000)
		emu_logf(&_logger, PLRLOG_WARN, "RF5C mem write to out-of-window offset 0x%04X\n", memOfs);
	cDev->writeM8(cDev->base.defInf.dataPtr, memOfs, fData[0x03]);
	return;
}

void VGMPlayer::Cmd_RF5C_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	UINT8 ofs = fData[0x01] & 0x7F;
	cDev->write8(cDev->base.defInf.dataPtr, ofs, fData[0x02]);
	
	// RF5C68 bank patch
	if (ofs == 0x07 && ! (fData[0x02] & 0x40))
	{
		UINT8 rfNum = (chipType == 0x10) ? 1 : 0;
		_rf5cBank[rfNum][chipID] = fData[0x02] & 0x0F;
	}
	
	return;
}

void VGMPlayer::Cmd_PWM_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->writeD16 == NULL)
		return;
	
	UINT8 ofs = (fData[0x01] >> 4) & 0x0F;
	UINT16 value = ReadBE16(&fData[0x01]) & 0x0FFF;
	cDev->writeD16(cDev->base.defInf.dataPtr, ofs, value);
	return;
}

void VGMPlayer::Cmd_QSound_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = 0;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	QSOUND_WORK* qsWork = &_qsWork[chipID];
	if (cDev == NULL || qsWork->write == NULL)
		return;
	
	if (cDev->flags & 0x01)	// enable hacks for proper playback of old VGMs with a good QSound core
	{
		UINT8 offset = fData[0x03];
		// need to handle three cases, as vgm_cmp can remove writes to both phase and bank
		// registers, depending on version.
		// - start address was written before end/loop, but phase register is written
		// - as above, but phase is not written (we use bank as a backup then)
		// - voice parameters are written during a note (we can't rewrite the address then)
		if (offset < 0x80)
		{
			UINT8 chn = offset >> 3;
			UINT16 data = ReadBE16(&fData[0x01]);
			
			switch(offset & 0x07)
			{
			case 0x01:	// Start Address
				qsWork->startAddrCache[chn] = data;
				break;
			case 0x02:	// Pitch
				// old HLE assumed writing a non-zero value after a zero value was Key On
				if (! qsWork->pitchCache[chn] && data)
					qsWork->write(cDev, (chn << 3) | 0x01, qsWork->startAddrCache[chn]);
				qsWork->pitchCache[chn] = data;
				break;
			case 0x03: // Phase (old HLE also assumed this was Key On)
				qsWork->write(cDev, (chn << 3) | 0x01, qsWork->startAddrCache[chn]);
				break;
			}
		}
	}
	
	qsWork->write(cDev, fData[0x03], ReadBE16(&fData[0x01]));
	return;
}

/*static*/ void VGMPlayer::WriteQSound_A(CHIP_DEVICE* cDev, UINT8 ofs, UINT16 data)
{
	cDev->writeD16(cDev->base.defInf.dataPtr, ofs, data);
	return;
}

/*static*/ void VGMPlayer::WriteQSound_B(CHIP_DEVICE* cDev, UINT8 ofs, UINT16 data)
{
	cDev->write8(cDev->base.defInf.dataPtr, 0x00, (data >> 8) & 0xFF);	// Data MSB
	cDev->write8(cDev->base.defInf.dataPtr, 0x01, (data >> 0) & 0xFF);	// Data LSB
	cDev->write8(cDev->base.defInf.dataPtr, 0x02, ofs);	// Register
	return;
}

void VGMPlayer::Cmd_WSwan_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, 0x80 + (fData[0x01] & 0x7F), fData[0x02]);
	return;
}

void VGMPlayer::Cmd_NES_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	UINT8 ofs = fData[0x01] & 0x7F;
	
	// remap FDS registers
	if (ofs == 0x3F)
		ofs = 0x23;	// FDS I/O enable
	else if ((ofs & 0xE0) == 0x20)
		ofs = 0x80 | (ofs & 0x1F);	// FDS register
	
	cDev->write8(cDev->base.defInf.dataPtr, ofs, fData[0x02]);
	return;
}

void VGMPlayer::Cmd_YMW_Bank(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	UINT8 bankmask = fData[0x01] & 0x03;
	// fData[0x03] is ignored as we don't support YMW258 ROMs > 16 MB
	if (bankmask == 0x03 && ! (fData[0x02] & 0x08))
	{
		// 1 MB banking (reg 0x10)
		cDev->write8(cDev->base.defInf.dataPtr, 0x10, fData[0x02] / 0x10);
	}
	else
	{
		// 512 KB banking (regs 0x11/0x12)
		if (bankmask & 0x02)	// low bank
			cDev->write8(cDev->base.defInf.dataPtr, 0x11, fData[0x02] / 0x08);
		if (bankmask & 0x01)	// high bank
			cDev->write8(cDev->base.defInf.dataPtr, 0x12, fData[0x02] / 0x08);
	}
	
	return;
}

void VGMPlayer::Cmd_SAA_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	cDev->write8(cDev->base.defInf.dataPtr, 0x01, fData[0x01] & 0x7F);	// SAA commands are at offset 1, not 0
	cDev->write8(cDev->base.defInf.dataPtr, 0x00, fData[0x02]);
	return;
}

void VGMPlayer::Cmd_OKIM6295_Reg(void)
{
	UINT8 chipType = _CMD_INFO[fData[0x00]].chipType;
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	if (cDev == NULL || cDev->write8 == NULL)
		return;
	
	UINT8 ofs = fData[0x01] & 0x7F;
	UINT8 data = fData[0x02];
	if (ofs == 0x0B)
	{
		if (data & 0x80)
		{
			data &= 0x7F;	// remove "pin7" bit (bug in some MAME VGM logs)
			//emu_logf(&_logger, PLRLOG_WARN, "OKIM6295 SetClock command (%02X %02X) includes Pin7 bit!\n",
			//	fData[0x00], fData[0x01]);
		}
	}
	
	cDev->write8(cDev->base.defInf.dataPtr, ofs, data);
	return;
}

void VGMPlayer::Cmd_AY_Stereo(void)
{
	UINT8 chipType = (fData[0x01] & 0x40) ? 0x06 : 0x12;	// YM2203 SSG or AY8910
	UINT8 chipID = (fData[0x01] & 0x80) >> 7;
	CHIP_DEVICE* cDev = GetDevicePtr(chipType, chipID);
	VGM_BASEDEV* clDev;
	DEVFUNC_OPTMASK writeStMask = NULL;
	UINT8 retVal;
	
	if (cDev == NULL)
		return;
	if (chipType == 0x12)	// AY8910
		clDev = &cDev->base;
	else if (chipType == 0x06)	// YM2203 SSG
		clDev = cDev->base.linkDev;
	if (clDev == NULL)
		return;
	
	retVal = SndEmu_GetDeviceFunc(clDev->defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_ALL, 0x5354, (void**)&writeStMask);
	if (writeStMask != NULL)
		writeStMask(cDev->base.defInf.dataPtr, fData[0x01] & 0x3F);
	return;
}
