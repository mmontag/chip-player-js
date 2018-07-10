#ifndef __VGMPLAYER_HPP__
#define __VGMPLAYER_HPP__

#include <stdtype.h>
#include <emu/EmuStructs.h>
#include <emu/Resampler.h>
#include <iconv.h>
#include "helper.h"
#include "playerbase.hpp"
#include "../vgm/dblk_compr.h"
#include <vector>
#include <string>


#define FCC_VGM 	0x56474D00

// This structure contains only some basic information about the VGM file,
// not the full header.
struct VGM_HEADER
{
	UINT32 fileVer;
	UINT32 eofOfs;
	UINT32 extraHdrOfs;
	UINT32 dataOfs;		// command data start offset
	UINT32 loopOfs;		// loop offset
	UINT32 dataEnd;		// command data end offset
	UINT32 gd3Ofs;		// GD3 tag offset
	
	UINT32 numTicks;	// total number of samples
	UINT32 loopTicks;	// number of samples for the looping part
	
	UINT32 xhChpClkOfs;	// extra header offset: chip clocks
	UINT32 xhChpVolOfs;	// extra header offset: chip volume
};

class VGMPlayer : public PlayerBase
{
public:
	struct CHIP_DEVICE
	{
		VGM_BASEDEV base;
		UINT8 vgmChipType;
		UINT8 chipID;
		DEVFUNC_WRITE_A8D8 write8;		// write 8-bit data to 8-bit register/offset
		DEVFUNC_WRITE_A16D8 writeM8;	// write 8-bit data to 16-bit memory offset
		DEVFUNC_WRITE_A8D16 writeD16;	// write 16-bit data to 8-bit register/offset
		DEVFUNC_WRITE_A16D16 writeM16;	// write 16-bit data to 16-bit register/offset
		DEVFUNC_WRITE_MEMSIZE romSize;
		DEVFUNC_WRITE_BLOCK romWrite;
		DEVFUNC_WRITE_MEMSIZE romSizeB;
		DEVFUNC_WRITE_BLOCK romWriteB;
	};
	
private:
	struct HDR_CHIP_DEF
	{
		UINT8 devType;
		UINT16 volume;
		UINT32 clock;
	};
	
	struct DACSTRM_DEV
	{
		DEV_INFO defInf;
		UINT8 streamID;
		UINT8 bankID;
	};
	struct PCM_BANK
	{
		std::vector<UINT8> data;
		std::vector<UINT32> bankOfs;
		std::vector<UINT32> bankSize;
	};
	
	typedef void (VGMPlayer::*COMMAND_FUNC)(void);	// VGM command member function callback
	struct DEVLINK_CB_DATA
	{
		VGMPlayer* object;
		UINT8 chipType;
	};
	struct COMMAND_INFO
	{
		UINT8 chipType;
		UINT32 cmdLen;
		COMMAND_FUNC func;
	};
	
public:
	VGMPlayer();
	~VGMPlayer();
	
	UINT32 GetPlayerType(void) const;
	const char* GetPlayerName(void) const;
	UINT8 LoadFile(const char* fileName);
	UINT8 UnloadFile(void);
	const VGM_HEADER* GetFileHeader(void) const;
	const char* GetSongTitle(void);
	
	//UINT32 GetSampleRate(void) const;
	UINT8 SetSampleRate(UINT32 sampleRate);
	//UINT8 SetPlaybackSpeed(double speed);
	//void SetCallback(PLAYER_EVENT_CB cbFunc, void* cbParam);
	UINT32 Tick2Sample(UINT32 ticks) const;
	UINT32 Sample2Tick(UINT32 samples) const;
	double Tick2Second(UINT32 ticks) const;
	//double Sample2Second(UINT32 samples) const;
	
	UINT8 GetState(void) const;
	UINT32 GetCurFileOfs(void) const;
	UINT32 GetCurTick(void) const;
	UINT32 GetCurSample(void) const;
	UINT32 GetTotalTicks(void) const;	// get time for playing once in ticks
	UINT32 GetLoopTicks(void) const;	// get time for one loop in ticks
	//UINT32 GetTotalPlayTicks(UINT32 numLoops) const;	// get time for playing + looping (without fading)
	UINT32 GetCurrentLoop(void) const;
	
	UINT8 Start(void);
	UINT8 Stop(void);
	UINT8 Reset(void);
	UINT32 Render(UINT32 smplCnt, WAVE_32BS* data);
	//UINT8 Seek(...); // TODO
	
private:
	UINT8 ParseHeader(VGM_HEADER* vgmHdr);
	
	UINT8 LoadTags(void);
	std::string GetUTF8String(const UINT8* startPtr, const UINT8* endPtr);
	
	void RefreshTSRates(void);
	
	UINT32 GetHeaderChipClock(UINT8 chipID);	// returns raw chip clock value from VGM header
	void InitDevices(void);
	
	static void DeviceLinkCallback(void* userParam, VGM_BASEDEV* cDev, DEVLINK_INFO* dLink);
	void LoadOPL4ROM(CHIP_DEVICE* chipDev);
	CHIP_DEVICE* GetDevicePtr(UINT8 chipType, UINT8 chipID);
	
	void ParseFile(UINT32 ticks);
	
	// --- VGM command functions ---
	void Cmd_invalid(void);
	void Cmd_unknown(void);
	void Cmd_EndOfData(void);				// command 66
	void Cmd_DelaySamples2B(void);			// command 61 - wait for N samples (2-byte parameter)
	void Cmd_Delay60Hz(void);				// command 62 - wait 735 samples (1/60 second)
	void Cmd_Delay50Hz(void);				// command 63 - wait 882 samples (1/50 second)
	void Cmd_DelaySamplesN1(void);			// command 70..7F - wait (N+1) samples
	void DoRAMOfsPatches(UINT8 chipType, UINT8 chipID, UINT32& dataOfs, UINT32& dataLen);
	void Cmd_DataBlock(void);				// command 67
	void Cmd_PcmRamWrite(void);				// command 68
	void Cmd_YM2612PCM_Delay(void);			// command 80..8F - write YM2612 PCM from data block + delay by N samples
	void Cmd_YM2612PCM_Seek(void);			// command E0 - set YM2612 PCM data offset
	void Cmd_DACCtrl_Setup(void);			// command 90
	void Cmd_DACCtrl_SetData(void);			// command 91
	void Cmd_DACCtrl_SetFrequency(void);	// command 92
	void Cmd_DACCtrl_PlayData_Loc(void);	// command 93
	void Cmd_DACCtrl_Stop(void);			// command 94
	void Cmd_DACCtrl_PlayData_Blk(void);	// command 95
	
	void Cmd_GGStereo(void);				// command 4F - set GameGear Stereo mask
	void Cmd_SN76489(void);					// command 50 - SN76489 register write
	void Cmd_Reg8_Data8(void);				// command 51/54/55/5A..5D - Register, Data (8-bit)
	void Cmd_CPort_Reg8_Data8(void);		// command 52/53/56..59/5E/5F - Port (in command byte), Register, Data (8-bit)
	void Cmd_Port_Reg8_Data8(void);			// command D0..D2 - Port, Register, Data (8-bit)
	void Cmd_Ofs8_Data8(void);				// command B3/B5..BB/BE/BF - Offset (8-bit), Data (8-bit)
	void Cmd_Ofs16_Data8(void);				// command C5..C8/D3/D4/D6/E5 - Offset (16-bit), Data (8-bit)
	void Cmd_Ofs8_Data16(void);				// unused - Offset (8-bit), Data (16-bit)
	void Cmd_Ofs16_Data16(void);			// command E1 - Offset (16-bit), Data (16-bit)
	void Cmd_Port_Ofs8_Data8(void);			// command D5 - Port, Offset (8-bit), Data (8-bit)
	void Cmd_DReg8_Data8(void);				// command A0 - Register (with dual-chip bit), Data (8-bit)
	void Cmd_SegaPCM_Mem(void);				// command C0 - SegaPCM memory write
	void Cmd_RF5C_Mem(void);				// command C1/C2 - RF5C68/164 memory write
	void Cmd_RF5C_Reg(void);				// command B0/B1 - RF5C68/164 register write
	void Cmd_PWM_Reg(void);					// command B2 - PWM register write (4-bit offset, 12-bit data)
	void Cmd_QSound_Reg(void);				// command C4 - QSound register write (16-bit data, 8-bit offset)
	void Cmd_WSwan_Reg(void);				// command BC - WonderSwan register write (Reg8_Data8 with remapping)
	void Cmd_NES_Reg(void);					// command B4 - NES APU register write (Reg8_Data8 with remapping)
	void Cmd_YMW_Bank(void);				// command C3 - YMW258 bank write (Ofs8_Data16 with remapping)
	void Cmd_SAA_Reg(void);					// command BD - SAA1099 register write (Reg8_Data8 with remapping)
	void Cmd_AY_Stereo(void);				// command 30 - set AY8910 stereo mask
	
	iconv_t _icUTF16;	// UTF-16 LE -> UTF-8 iconv object
	std::vector<UINT8> _fileData;
	
	enum
	{
		_HDR_BUF_SIZE = 0x100,
		_CHIP_COUNT = 0x29,
		_PCM_BANK_COUNT = 0x40
	};
	
	VGM_HEADER _fileHdr;
	UINT8 _hdrBuffer[_HDR_BUF_SIZE];	// buffer containing the file header
	UINT32 _hdrLenFile;
	UINT32 _tagVer;
	std::vector<std::string> _tagData;
	
	//UINT32 _outSmplRate;
	
	// tick/sample conversion rates
	UINT64 _tsMult;
	UINT64 _tsDiv;
	
	UINT32 _filePos;
	UINT32 _fileTick;
	UINT32 _playTick;
	UINT32 _playSmpl;
	UINT32 _curLoop;
	
	UINT8 _playState;
	UINT8 _psTrigger;	// used to temporarily trigger special commands
	//PLAYER_EVENT_CB _eventCbFunc;
	//void* _eventCbParam;
	
	static const UINT8 _DEV_LIST[_CHIP_COUNT];
	static const UINT32 _CHIPCLK_OFS[_CHIP_COUNT];
	static const COMMAND_INFO _CMD_INFO[0x100];
	static const UINT8 _VGM_BANK_CHIPS[_PCM_BANK_COUNT];
	static const UINT8 _VGM_ROM_CHIPS[0x40][2];
	static const UINT8 _VGM_RAM_CHIPS[0x40];
	
	size_t _devMap[_CHIP_COUNT][2];	// maps VGM device ID to _devices vector
	std::vector<CHIP_DEVICE> _devices;
	
	size_t _dacStrmMap[0x100];	// maps VGM DAC stream ID -> _dacStreams vector
	std::vector<DACSTRM_DEV> _dacStreams;
	
	PCM_BANK _pcmBank[_PCM_BANK_COUNT];
	PCM_COMPR_TBL _pcmComprTbl;
	
	UINT32 _ym2612pcm_bnkPos;
	UINT8 _rf5cBank[2][2];	// [0 RF5C68 / 1 RF5C164][chipID]
};

#endif	// __VGMPLAYER_HPP__
