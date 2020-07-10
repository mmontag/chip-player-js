#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

#ifndef NO_EXTERNAL_ROM
#include <stdio.h>
#endif

#define INLINE	static inline

#include "../common_def.h"
#include "vgmplayer.hpp"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"
#include "../emu/Resampler.h"
#include "../emu/SoundDevs.h"
#include "../emu/EmuCores.h"
#include "../emu/dac_control.h"
#include "../emu/cores/sn764intf.h"	// for SN76496_CFG
#include "../emu/cores/segapcm.h"		// for SEGAPCM_CFG
#include "../emu/cores/ayintf.h"		// for AY8910_CFG
#include "../emu/cores/okim6258.h"		// for OKIM6258_CFG
#include "../emu/cores/k054539.h"
#include "../emu/cores/c140.h"
#include "../emu/cores/es5503.h"
#include "../emu/cores/es5506.h"

#include "dblk_compr.h"
#include "../utils/StrUtils.h"
#include "helper.h"
#include "logging.h"

/*static*/ const UINT8 VGMPlayer::_OPT_DEV_LIST[_OPT_DEV_COUNT] =
{
	DEVID_SN76496, DEVID_YM2413, DEVID_YM2612, DEVID_YM2151, DEVID_SEGAPCM, DEVID_RF5C68, DEVID_YM2203, DEVID_YM2608,
	DEVID_YM2610, DEVID_YM3812, DEVID_YM3526, DEVID_Y8950, DEVID_YMF262, DEVID_YMF278B, DEVID_YMF271, DEVID_YMZ280B,
	DEVID_32X_PWM, DEVID_AY8910, DEVID_GB_DMG, DEVID_NES_APU, DEVID_YMW258, DEVID_uPD7759, DEVID_OKIM6258, DEVID_OKIM6295,
	DEVID_K051649, DEVID_K054539, DEVID_C6280, DEVID_C140, DEVID_C219, DEVID_K053260, DEVID_POKEY, DEVID_QSOUND,
	DEVID_SCSP, DEVID_WSWAN, DEVID_VBOY_VSU, DEVID_SAA1099, DEVID_ES5503, DEVID_ES5506, DEVID_X1_010, DEVID_C352,
	DEVID_GA20,
};

/*static*/ const UINT8 VGMPlayer::_DEV_LIST[_CHIP_COUNT] =
{
	DEVID_SN76496, DEVID_YM2413, DEVID_YM2612, DEVID_YM2151, DEVID_SEGAPCM, DEVID_RF5C68, DEVID_YM2203, DEVID_YM2608,
	DEVID_YM2610, DEVID_YM3812, DEVID_YM3526, DEVID_Y8950, DEVID_YMF262, DEVID_YMF278B, DEVID_YMF271, DEVID_YMZ280B,
	DEVID_RF5C68, DEVID_32X_PWM, DEVID_AY8910, DEVID_GB_DMG, DEVID_NES_APU, DEVID_YMW258, DEVID_uPD7759, DEVID_OKIM6258,
	DEVID_OKIM6295, DEVID_K051649, DEVID_K054539, DEVID_C6280, DEVID_C140, DEVID_K053260, DEVID_POKEY, DEVID_QSOUND,
	DEVID_SCSP, DEVID_WSWAN, DEVID_VBOY_VSU, DEVID_SAA1099, DEVID_ES5503, DEVID_ES5506, DEVID_X1_010, DEVID_C352,
	DEVID_GA20,
};

/*static*/ const UINT32 VGMPlayer::_CHIPCLK_OFS[_CHIP_COUNT] =
{
	0x0C, 0x10, 0x2C, 0x30, 0x38, 0x40, 0x44, 0x48,
	0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68,
	0x6C, 0x70, 0x74, 0x80, 0x84, 0x88, 0x8C, 0x90,
	0x98, 0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4,
	0xB8, 0xC0, 0xC4, 0xC8, 0xCC, 0xD0, 0xD8, 0xDC,
	0xE0,
};
/*static*/ const UINT16 VGMPlayer::_CHIP_VOLUME[_CHIP_COUNT] =
{	0x80, 0x200, 0x100, 0x100, 0x180, 0xB0, 0x100, 0x80,
	0x80, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x98,
	0x80, 0xE0, 0x100, 0xC0, 0x100, 0x40, 0x11E, 0x1C0,
	0x100, 0xA0, 0x100, 0x100, 0x55, 0xB3, 0x100, 0x100,
	0x20, 0x100, 0x100, 0x100, 0x40, 0x20, 0x100, 0x40,
	0x280,
};
/*static*/ const UINT16 VGMPlayer::_PB_VOL_AMNT[_CHIP_COUNT] =
{	0x100, 0x80, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100,
	0x100, 0x200, 0x200, 0x200, 0x200, 0x100, 0x100, 0x1AF,
	0x200, 0x100, 0x200, 0x200, 0x200, 0x400, 0x100, 0x200,
	0x200, 0x100, 0x100, 0x100, 0x300, 0x100, 0x100, 0x100,
	0x800, 0x100, 0x100, 0x100, 0x800, 0x1000, 0x100, 0x800,
	0x100,
};

/*static*/ const char* const VGMPlayer::_TAG_TYPE_LIST[_TAG_COUNT] =
{
	"TITLE", "TITLE-JPN",
	"GAME", "GAME-JPN",
	"SYSTEM", "SYSTEM-JPN",
	"ARTIST", "ARTIST-JPN",
	"DATE",
	"ENCODED_BY",
	"COMMENT",
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

INLINE UINT32 ReadRelOfs(const UINT8* data, UINT32 fileOfs)
{
	// read a VGM-style (relative) offset
	UINT32 ofs = ReadLE32(&data[fileOfs]);
	return ofs ? (fileOfs + ofs) : ofs;	// add base to offset, if offset != 0
}

INLINE UINT16 MulFixed8x8(UINT16 a, UINT16 b)	// 8.8 fixed point multiplication
{
	UINT32 res16;	// 16.16 fixed point result
	
	res16 = (UINT32)a * b;
	return (res16 + 0x80) >> 8;	// round to nearest neighbour + scale back to 8.8 fixed point
}

INLINE void SaveDeviceConfig(std::vector<UINT8>& dst, const void* srcData, size_t srcLen)
{
	const UINT8* srcPtr = (const UINT8*)srcData;
	dst.assign(srcPtr, srcPtr + srcLen);
	return;
}

VGMPlayer::VGMPlayer() :
	_filePos(0),
	_fileTick(0),
	_playTick(0),
	_playSmpl(0),
	_curLoop(0),
	_playState(0x00),
	_psTrigger(0x00)
{
	UINT8 retVal;
	UINT16 optChip;
	UINT8 chipID;
	
	_playOpts.playbackHz = 0;
	_playOpts.hardStopOld = 0;
	
	for (optChip = 0x00; optChip < 0x100; optChip ++)
	{
		for (chipID = 0; chipID < 2; chipID ++)
			_devOptMap[optChip][chipID] = (size_t)-1;
	}
	for (optChip = 0; optChip < _OPT_DEV_COUNT; optChip ++)
	{
		for (chipID = 0; chipID < 2; chipID ++)
		{
			size_t optID = optChip * 2 + chipID;
			UINT8 devID = _OPT_DEV_LIST[optChip];
			PLR_DEV_OPTS& devOpts = _devOpts[optID];
			
			InitDeviceOptions(devOpts);
			if (devID == DEVID_NES_APU)
				devOpts.coreOpts = 0x01B7;
			else if (devID == DEVID_SCSP)
				devOpts.coreOpts = 0x01;
			_devOptMap[devID][chipID] = optID;
		}
	}
	
	retVal = CPConv_Init(&_cpcUTF16, "UTF-16LE", "UTF-8");
	if (retVal)
		_cpcUTF16 = NULL;
	memset(&_pcmComprTbl, 0x00, sizeof(PCM_COMPR_TBL));
	_tagList[0] = NULL;
	return;
}

VGMPlayer::~VGMPlayer()
{
	_eventCbFunc = NULL;	// prevent any callbacks during destruction
	
	if (_playState & PLAYSTATE_PLAY)
		Stop();
	UnloadFile();
	
	if (_cpcUTF16 != NULL)
		CPConv_Deinit(_cpcUTF16);
	
	return;
}

UINT32 VGMPlayer::GetPlayerType(void) const
{
	return FCC_VGM;
}

const char* VGMPlayer::GetPlayerName(void) const
{
	return "VGM";
}

/*static*/ UINT8 VGMPlayer::IsMyFile(DATA_LOADER *dataLoader)
{
	DataLoader_ReadUntil(dataLoader,0x38);
	if (DataLoader_GetSize(dataLoader) < 0x38)
		return 0xF1;	// file too small
	if (memcmp(&DataLoader_GetData(dataLoader)[0x00], "Vgm ", 4))
		return 0xF0;	// invalid signature
	return 0x00;
}

UINT8 VGMPlayer::LoadFile(DATA_LOADER *dataLoader)
{
	_dLoad = NULL;
	DataLoader_ReadUntil(dataLoader,0x38);
	_fileData = DataLoader_GetData(dataLoader);
	if (DataLoader_GetSize(dataLoader) < 0x38 || memcmp(&_fileData[0x00], "Vgm ", 4))
		return 0xF0;	// invalid file
	
	_dLoad = dataLoader;
	DataLoader_ReadAll(_dLoad);
	_fileData = DataLoader_GetData(_dLoad);
	
	// parse main header
	ParseHeader();
	
	// parse extra headers
	ParseXHdr_Data32(_fileHdr.xhChpClkOfs, _xHdrChipClk);
	ParseXHdr_Data16(_fileHdr.xhChpVolOfs, _xHdrChipVol);
	
	// parse tags
	LoadTags();
	
	return 0x00;
}

UINT8 VGMPlayer::ParseHeader(void)
{
	memset(&_fileHdr, 0x00, sizeof(VGM_HEADER));
	
	_fileHdr.fileVer = ReadLE32(&_fileData[0x08]);
	
	_fileHdr.dataOfs = (_fileHdr.fileVer >= 0x150) ? ReadRelOfs(_fileData, 0x34) : 0x00;
	if (! _fileHdr.dataOfs)
		_fileHdr.dataOfs = 0x40;
	_hdrLenFile = _fileHdr.dataOfs;
	
	_fileHdr.extraHdrOfs = (_hdrLenFile >= 0xC0) ? ReadRelOfs(_fileData, 0xBC) : 0x00;
	if (_fileHdr.extraHdrOfs && _hdrLenFile > _fileHdr.extraHdrOfs)
		_hdrLenFile = _fileHdr.extraHdrOfs;	// the main header ends where the extra header begins
	
	if (_hdrLenFile > _HDR_BUF_SIZE)
		_hdrLenFile = _HDR_BUF_SIZE;
	memset(_hdrBuffer, 0x00, _HDR_BUF_SIZE);
	memcpy(_hdrBuffer, _fileData, _hdrLenFile);
	
	_fileHdr.eofOfs = ReadRelOfs(_hdrBuffer, 0x04);
	_fileHdr.gd3Ofs = ReadRelOfs(_hdrBuffer, 0x14);
	_fileHdr.numTicks = ReadLE32(&_hdrBuffer[0x18]);
	_fileHdr.loopOfs = ReadRelOfs(_hdrBuffer, 0x1C);
	_fileHdr.loopTicks = ReadLE32(&_hdrBuffer[0x20]);
	
	_fileHdr.loopBase = (INT8)_hdrBuffer[0x7E];
	_fileHdr.loopModifier = _hdrBuffer[0x7F];
	if (_hdrBuffer[0x7C] <= 0xC0)
		_fileHdr.volumeGain = _hdrBuffer[0x7C];
	else if (_hdrBuffer[0x7C] == 0xC1)
		_fileHdr.volumeGain = -0x40;
	else
		_fileHdr.volumeGain = _hdrBuffer[0x7C] - 0x100;
	_fileHdr.volumeGain <<= 3;	// 3.5 fixed point -> 8.8 fixed point
	
	if (_fileHdr.extraHdrOfs)
	{
		UINT32 xhLen;
		
		xhLen = ReadLE32(&_fileData[_fileHdr.extraHdrOfs]);
		if (xhLen >= 0x08)
			_fileHdr.xhChpClkOfs = ReadRelOfs(_fileData, _fileHdr.extraHdrOfs + 0x04);
		if (xhLen >= 0x0C)
			_fileHdr.xhChpVolOfs = ReadRelOfs(_fileData, _fileHdr.extraHdrOfs + 0x08);
	}
	
	if (! _fileHdr.eofOfs || _fileHdr.eofOfs > DataLoader_GetSize(_dLoad))
		_fileHdr.eofOfs = DataLoader_GetSize(_dLoad);	// catch invalid EOF values
	_fileHdr.dataEnd = _fileHdr.eofOfs;
	// command data ends at the GD3 offset if:
	//	GD3 is used && GD3 offset < EOF (just to be sure) && GD3 offset > dataOfs (catch files with GD3 between header and data)
	if (_fileHdr.gd3Ofs && (_fileHdr.gd3Ofs < _fileHdr.dataEnd && _fileHdr.gd3Ofs >= _fileHdr.dataOfs))
		_fileHdr.dataEnd = _fileHdr.gd3Ofs;
	
	if (_fileHdr.loopOfs)
	{
		if (_fileHdr.loopOfs < _fileHdr.dataOfs || _fileHdr.loopOfs >= _fileHdr.dataEnd)
		{
			debug("Invalid VGM loop offset 0x%06X - ignoring!\n", _fileHdr.loopOfs);
			_fileHdr.loopOfs = 0x00;
		}
	}
	
	return 0x00;
}

void VGMPlayer::ParseXHdr_Data32(UINT32 fileOfs, std::vector<XHDR_DATA32>& xData)
{
	xData.clear();
	if (! fileOfs || fileOfs >= DataLoader_GetSize(_dLoad))
		return;
	
	UINT32 curPos = fileOfs;
	size_t curChip;
	
	xData.resize(_fileData[curPos]);	curPos ++;
	for (curChip = 0; curChip < xData.size(); curChip ++, curPos += 0x05)
	{
		if (curPos + 0x05 > DataLoader_GetSize(_dLoad))
		{
			xData.resize(curChip);
			break;
		}
		
		XHDR_DATA32& cData = xData[curChip];
		cData.type = _fileData[curPos + 0x00];
		cData.data = ReadLE32(&_fileData[curPos + 0x01]);
	}
	
	return;
}

void VGMPlayer::ParseXHdr_Data16(UINT32 fileOfs, std::vector<XHDR_DATA16>& xData)
{
	xData.clear();
	if (! fileOfs || fileOfs >= DataLoader_GetSize(_dLoad))
		return;
	
	UINT32 curPos = fileOfs;
	size_t curChip;
	
	xData.resize(_fileData[curPos]);	curPos ++;
	for (curChip = 0; curChip < xData.size(); curChip ++, curPos += 0x04)
	{
		if (curPos + 0x04 > DataLoader_GetSize(_dLoad))
		{
			xData.resize(curChip);
			break;
		}
		
		XHDR_DATA16& cData = xData[curChip];
		cData.type = _fileData[curPos + 0x00];
		cData.flags = _fileData[curPos + 0x01];
		cData.data = ReadLE16(&_fileData[curPos + 0x02]);
	}
	
	return;
}

UINT8 VGMPlayer::LoadTags(void)
{
	for (size_t curTag = 0; curTag < _TAG_COUNT; curTag ++)
		_tagData[curTag].clear();
	_tagList[0] = NULL;
	if (! _fileHdr.gd3Ofs)
		return 0x00;
	
	UINT32 curPos;
	UINT32 eotPos;
	
	if (memcmp(&_fileData[_fileHdr.gd3Ofs + 0x00], "Gd3 ", 4))
		return 0xF0;	// bad tag
	
	_tagVer = ReadLE32(&_fileData[_fileHdr.gd3Ofs + 0x04]);
	if (_tagVer < 0x100 || _tagVer >= 0x200)
		return 0xF1;	// unsupported tag version
	
	eotPos = ReadLE32(&_fileData[_fileHdr.gd3Ofs + 0x08]);
	curPos = _fileHdr.gd3Ofs + 0x0C;
	eotPos += curPos;
	if (eotPos > _fileHdr.eofOfs)
		eotPos = _fileHdr.eofOfs;
	
	const char **tagListEnd = _tagList;
	for (size_t curTag = 0; curTag < _TAG_COUNT; curTag ++)
	{
		UINT32 startPos = curPos;
		if (curPos >= eotPos)
			break;
		
		// search for UTF-16 L'\0' character
		while(curPos < eotPos && ReadLE16(&_fileData[curPos]) != L'\0')
			curPos += 0x02;
		_tagData[curTag] = GetUTF8String(&_fileData[startPos], &_fileData[curPos]);
		curPos += 0x02;	// skip '\0'
		
		*(tagListEnd++) = _TAG_TYPE_LIST[curTag];
		*(tagListEnd++) = _tagData[curTag].c_str();
	}
	
	*tagListEnd = NULL;
	
	return 0x00;
}

std::string VGMPlayer::GetUTF8String(const UINT8* startPtr, const UINT8* endPtr)
{
	if (_cpcUTF16 == NULL || startPtr == endPtr)
		return std::string();
	
	size_t convSize = 0;
	char* convData = NULL;
	std::string result;
	UINT8 retVal;
	
	retVal = CPConv_StrConvert(_cpcUTF16, &convSize, &convData, endPtr - startPtr, (const char*)startPtr);
	
	result.assign(convData, convData + convSize);
	free(convData);
	return result;
}

UINT8 VGMPlayer::UnloadFile(void)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0xFF;
	
	_playState = 0x00;
	_dLoad = NULL;
	_fileData = NULL;
	_fileHdr.fileVer = 0xFFFFFFFF;
	_fileHdr.dataOfs = 0x00;
	_songDevCfg.clear();
	_devices.clear();
	for (size_t curTag = 0; curTag < _TAG_COUNT; curTag ++)
		_tagData[curTag].clear();
	_tagList[0] = NULL;
	
	return 0x00;
}

const VGM_HEADER* VGMPlayer::GetFileHeader(void) const
{
	return &_fileHdr;
}

const char* const* VGMPlayer::GetTags(void)
{
	return _tagList;
}

UINT8 VGMPlayer::GetSongInfo(PLR_SONG_INFO& songInf)
{
	if (_dLoad == NULL)
		return 0xFF;
	
	UINT8 vgmChip;
	
	songInf.format = FCC_VGM;
	songInf.fileVerMaj = (_fileHdr.fileVer >> 8) & 0xFFFFFF;
	songInf.fileVerMin = (_fileHdr.fileVer >> 0) & 0xFF;
	songInf.tickRateMul = 1;
	songInf.tickRateDiv = 44100;
	songInf.songLen = GetTotalTicks();
	songInf.loopTick = _fileHdr.loopOfs ? GetLoopTicks() : (UINT32)-1;
	songInf.deviceCnt = 0;
	for (vgmChip = 0x00; vgmChip < _CHIP_COUNT; vgmChip ++)
		songInf.deviceCnt += GetChipCount(vgmChip);
	
	return 0x00;
}

UINT8 VGMPlayer::GetSongDeviceInfo(std::vector<PLR_DEV_INFO>& devInfList) const
{
	if (_dLoad == NULL)
		return 0xFF;
	
	devInfList.clear();
	if (_playState & PLAYSTATE_PLAY)
	{
		// when playing, get information from device structures (may feature modified volume levels)
		size_t curDev;
		
		devInfList.reserve(_devices.size());
		for (curDev = 0; curDev < _songDevCfg.size(); curDev ++)
		{
			const SONG_DEV_CFG& sdCfg = _songDevCfg[curDev];
			const DEV_GEN_CFG* dCfg = (const DEV_GEN_CFG*)&sdCfg.cfgData[0];
			const CHIP_DEVICE* cDev = (sdCfg.deviceID < _devices.size()) ? &_devices[sdCfg.deviceID] : NULL;
			PLR_DEV_INFO devInf;
			
			memset(&devInf, 0x00, sizeof(PLR_DEV_INFO));
			devInf.id = sdCfg.deviceID;
			devInf.type = sdCfg.type;
			devInf.instance = (UINT8)sdCfg.instance;
			devInf.clock = dCfg->clock;
			devInf.cParams = dCfg->flags;
			if (devInf.type == DEVID_SN76496)
			{
				const SN76496_CFG* snCfg = (const SN76496_CFG*)dCfg;
				devInf.cParams = (snCfg->noiseTaps << 0) | (snCfg->shiftRegWidth << 16) | (_hdrBuffer[0x2B] << 24);
			}
			else if (devInf.type == DEVID_AY8910)
			{
				const AY8910_CFG* ayCfg = (const AY8910_CFG*)dCfg;
				devInf.cParams = (ayCfg->chipType << 0) | (ayCfg->chipFlags << 8);
			}
			else if (devInf.type == DEVID_SEGAPCM)
			{
				const SEGAPCM_CFG* spCfg = (const SEGAPCM_CFG*)dCfg;
				devInf.cParams = (spCfg->bnkshift << 0) | (spCfg->bnkmask << 8);
			}
			else if (devInf.type == DEVID_OKIM6258)
			{
				//const OKIM6258_CFG* okiCfg = (const OKIM6258_CFG*)okiCfg;
				devInf.cParams = _hdrBuffer[0x94];
			}
			if (cDev != NULL)
			{
				const VGM_BASEDEV* clDev = &cDev->base;
				devInf.core = (clDev->defInf.devDef != NULL) ? clDev->defInf.devDef->coreID : 0x00;
				devInf.volume = (clDev->resmpl.volumeL + clDev->resmpl.volumeR) / 2;
				devInf.smplRate = clDev->defInf.sampleRate;
			}
			devInfList.push_back(devInf);
		}
		return 0x01;	// returned "live" data
	}
	else
	{
		UINT8 vgmChip;
		UINT8 chipID;
		UINT32 chipCnt;
		
		// get information from VGM header
		chipCnt = 0;
		for (vgmChip = 0x00; vgmChip < _CHIP_COUNT; vgmChip ++)
			chipCnt += GetChipCount(vgmChip);
		devInfList.reserve(chipCnt);
		
		for (vgmChip = 0x00; vgmChip < _CHIP_COUNT; vgmChip ++)
		{
			for (chipID = 0; chipID < GetChipCount(vgmChip); chipID ++)
			{
				PLR_DEV_INFO devInf;
				memset(&devInf, 0x00, sizeof(PLR_DEV_INFO));
				
				devInf.type = _DEV_LIST[vgmChip];
				if (devInf.type == DEVID_C140 && _hdrBuffer[0x96] == 2)
					devInf.type = DEVID_C219;
				devInf.id = chipID;
				devInf.core = 0x00;
				devInf.clock = GetChipClock(vgmChip, chipID) & ~0xC0000000;
				devInf.cParams = 0x00;
				devInf.volume = GetChipVolume(vgmChip, chipID, 0);
				devInf.smplRate = 0;
				devInfList.push_back(devInf);
			}
		}
		return 0x00;	// returned data based on file header
	}
}

size_t VGMPlayer::DeviceID2OptionID(UINT32 id) const
{
	UINT8 type;
	UINT8 instance;
	
	if (id & 0x80000000)
	{
		type = (id >> 0) & 0xFF;
		instance = (id >> 16) & 0xFF;
	}
	else if (id < _devices.size())
	{
		type = _DEV_LIST[_devices[id].vgmChipType];
		instance = _devices[id].chipID;
	}
	else
	{
		return (size_t)-1;
	}
	
	if (instance < 2)
		return _devOptMap[type][instance];
	else
		return (size_t)-1;
}

void VGMPlayer::RefreshMuting(VGMPlayer::CHIP_DEVICE& chipDev, const PLR_MUTE_OPTS& muteOpts)
{
	VGM_BASEDEV* clDev;
	UINT8 linkCntr = 0;
	
	for (clDev = &chipDev.base; clDev != NULL && linkCntr < 2; clDev = clDev->linkDev, linkCntr ++)
	{
		DEV_INFO* devInf = &clDev->defInf;
		if (devInf->dataPtr != NULL && devInf->devDef->SetMuteMask != NULL)
			devInf->devDef->SetMuteMask(devInf->dataPtr, muteOpts.chnMute[linkCntr]);
	}
	
	return;
}

UINT8 VGMPlayer::SetDeviceOptions(UINT32 id, const PLR_DEV_OPTS& devOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (UINT32)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID] = devOpts;
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
	{
		DEV_INFO* devInf = &_devices[devID].base.defInf;
		if (devInf->devDef->SetOptionBits != NULL)
			devInf->devDef->SetOptionBits(devInf->dataPtr, _devOpts[optID].coreOpts);
		
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
	}
	return 0x00;
}

UINT8 VGMPlayer::GetDeviceOptions(UINT32 id, PLR_DEV_OPTS& devOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (UINT32)-1)
		return 0x80;	// bad device ID
	
	devOpts = _devOpts[optID];
	return 0x00;
}

UINT8 VGMPlayer::SetDeviceMuting(UINT32 id, const PLR_MUTE_OPTS& muteOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (UINT32)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID].muteOpts = muteOpts;
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
	return 0x00;
}

UINT8 VGMPlayer::GetDeviceMuting(UINT32 id, PLR_MUTE_OPTS& muteOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (UINT32)-1)
		return 0x80;	// bad device ID
	
	muteOpts = _devOpts[optID].muteOpts;
	return 0x00;
}

UINT8 VGMPlayer::SetPlayerOptions(const VGM_PLAY_OPTIONS& playOpts)
{
	_playOpts = playOpts;
	return 0x00;
}

UINT8 VGMPlayer::GetPlayerOptions(VGM_PLAY_OPTIONS& playOpts) const
{
	playOpts = _playOpts;
	return 0x00;
}

UINT8 VGMPlayer::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0x01;	// can't set during playback
	
	_outSmplRate = sampleRate;
	return 0x00;
}

/*UINT8 VGMPlayer::SetPlaybackSpeed(double speed)
{
	return 0xFF;	// not yet supported
}*/


void VGMPlayer::RefreshTSRates(void)
{
	_tsMult = _outSmplRate * 1;
	_tsDiv = 44100;
	if (_playOpts.playbackHz && _fileHdr.recordHz)
	{
		// TODO: fix at other places
		_tsMult *= _fileHdr.recordHz;
		_tsDiv *= _playOpts.playbackHz;
	}
	
	return;
}

UINT32 VGMPlayer::Tick2Sample(UINT32 ticks) const
{
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 VGMPlayer::Sample2Tick(UINT32 samples) const
{
	return (UINT32)(samples * _tsDiv / _tsMult);
}

double VGMPlayer::Tick2Second(UINT32 ticks) const
{
	return ticks * 1 / 44100.0;
}

UINT8 VGMPlayer::GetState(void) const
{
	return _playState;
}

UINT32 VGMPlayer::GetCurPos(UINT8 unit) const
{
	switch(unit)
	{
	case PLAYPOS_FILEOFS:
		return _filePos;
	case PLAYPOS_TICK:
		return _playTick;
	case PLAYPOS_SAMPLE:
		return _playSmpl;
	case PLAYPOS_COMMAND:
	default:
		return (UINT32)-1;
	}
}

UINT32 VGMPlayer::GetCurLoop(void) const
{
	return _curLoop;
}

UINT32 VGMPlayer::GetTotalTicks(void) const
{
	return _fileHdr.numTicks;
}

UINT32 VGMPlayer::GetLoopTicks(void) const
{
	if (! _fileHdr.loopOfs)
		return 0;
	else
		return _fileHdr.loopTicks;
}


UINT8 VGMPlayer::Start(void)
{
	size_t curStrm;
	
	InitDevices();
	
	_dacStreams.clear();
	for (curStrm = 0; curStrm < 0x100; curStrm ++)
		_dacStrmMap[curStrm] = (size_t)-1;
	//memset(&_pcmBank, 0x00, sizeof(PCM_BANK) * _PCM_BANK_COUNT);
	memset(&_pcmComprTbl, 0x00, sizeof(PCM_COMPR_TBL));
	
	_playState |= PLAYSTATE_PLAY;
	Reset();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_START, NULL);
	
	return 0x00;
}

UINT8 VGMPlayer::Stop(void)
{
	size_t curDev;
	size_t curBank;
	DEV_INFO* devInf;
	PCM_BANK* pcmBnk;
	
	_playState &= ~PLAYSTATE_PLAY;
	
	for (curDev = 0; curDev < _dacStreams.size(); curDev ++)
	{
		devInf = &_dacStreams[curDev].defInf;
		devInf->devDef->Stop(devInf->dataPtr);
		devInf->dataPtr = NULL;
	}
	_dacStreams.clear();
	
	for (curBank = 0x00; curBank < _PCM_BANK_COUNT; curBank ++)
	{
		pcmBnk = &_pcmBank[curBank];
		pcmBnk->bankOfs.clear();
		pcmBnk->bankSize.clear();
		pcmBnk->data.clear();
	}
	free(_pcmComprTbl.values.d8);	_pcmComprTbl.values.d8 = NULL;
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
		FreeDeviceTree(&_devices[curDev].base, 0);
	_devices.clear();
	_songDevCfg.clear();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_STOP, NULL);
	
	return 0x00;
}

UINT8 VGMPlayer::Reset(void)
{
	size_t curDev;
	
	_filePos = _fileHdr.dataOfs;
	_fileTick = 0;
	_playTick = 0;
	_playSmpl = 0;
	_playState &= ~PLAYSTATE_END;
	_psTrigger = 0x00;
	_curLoop = 0;
	
	RefreshTSRates();
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		VGM_BASEDEV* clDev;
		
		clDev = &_devices[curDev].base;
		clDev->defInf.devDef->Reset(clDev->defInf.dataPtr);
		for (; clDev != NULL; clDev = clDev->linkDev)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
		}
	}
	
	return 0x00;
}

UINT32 VGMPlayer::GetHeaderChipClock(UINT8 chipType) const
{
	if (chipType >= _CHIP_COUNT)
		return 0;
	
	return ReadLE32(&_hdrBuffer[_CHIPCLK_OFS[chipType]]);
}

inline UINT32 VGMPlayer::GetChipCount(UINT8 chipType) const
{
	UINT32 clock = GetHeaderChipClock(chipType);
	if (! clock)
		return 0;
	return (clock & 0x40000000) ? 2 : 1;
}

UINT32 VGMPlayer::GetChipClock(UINT8 chipType, UINT8 chipID) const
{
	size_t curChip;
	UINT32 clock = GetHeaderChipClock(chipType);
	
	if (chipID == 0)
		return clock & ~0x40000000;	// return clock without dual-chip bit
	if (! (clock & 0x40000000))
		return 0;	// dual-chip bit not set - no second chip used
	
	for (curChip = 0; curChip < _xHdrChipClk.size(); curChip ++)
	{
		const XHDR_DATA32& cData = _xHdrChipClk[curChip];
		if (cData.type == chipType)
			return cData.data;
	}
	
	return clock & ~0x40000000;	// return clock without dual-chip bit
}

UINT16 VGMPlayer::GetChipVolume(UINT8 chipType, UINT8 chipID, UINT8 isLinked) const
{
	if (chipType >= _CHIP_COUNT)
		return 0;
	
	size_t curChip;
	UINT16 numChips;
	UINT16 vol = _CHIP_VOLUME[chipType];
	
	numChips = GetChipCount(chipType);
	if (chipType == 0x00)
	{
		// The T6W28 consists of 2 "half" chips, so we need to treat it as 1.
		if (GetHeaderChipClock(chipType) & 0x80000000)
			numChips = 1;
	}
	
	if (isLinked)
	{
		if (chipType == 0x06)
			vol /= 2;	// the YM2203's SSG should be half as loud as the FM part
	}
	if (numChips > 1)
		vol /= numChips;
	
	chipType = (isLinked << 7) | (chipType & 0x7F);
	for (curChip = 0; curChip < _xHdrChipVol.size(); curChip ++)
	{
		const XHDR_DATA16& cData = _xHdrChipVol[curChip];
		if (cData.type == chipType && (cData.flags & 0x01) == chipID)
		{
			// Bit 15 - absolute/relative volume
			//	0 - absolute
			//	1 - relative (0x0100 = 1.0, 0x80 = 0.5, etc.)
			if (cData.data & 0x8000)
				return MulFixed8x8(vol, cData.data & 0x7FFF);
			else
				return cData.data;
		}
	}
	
	return vol;
}

UINT16 VGMPlayer::EstimateOverallVolume(void) const
{
	size_t curChip;
	const VGM_BASEDEV* clDev;
	UINT16 absVol;
	
	absVol = 0x00;
	for (curChip = 0; curChip < _devices.size(); curChip ++)
	{
		const CHIP_DEVICE& chipDev = _devices[curChip];
		for (clDev = &chipDev.base; clDev != NULL; clDev = clDev->linkDev)
		{
			absVol += MulFixed8x8(clDev->resmpl.volumeL + clDev->resmpl.volumeR,
									_PB_VOL_AMNT[chipDev.vgmChipType]) / 2;
		}
	}
	
	return absVol;
}

void VGMPlayer::NormalizeOverallVolume(UINT16 overallVol)
{
	if (! overallVol)
		return;
	
	UINT16 volFactor;
	size_t curChip;
	VGM_BASEDEV* clDev;
	
	if (overallVol <= 0x180)
	{
		volFactor = 1;
		while(overallVol <= 0x180)
		{
			volFactor *= 2;
			overallVol *= 2;
		}
		
		for (curChip = 0; curChip < _devices.size(); curChip ++)
		{
			CHIP_DEVICE& chipDev = _devices[curChip];
			for (clDev = &chipDev.base; clDev != NULL; clDev = clDev->linkDev)
			{
				clDev->resmpl.volumeL *= volFactor;
				clDev->resmpl.volumeR *= volFactor;
			}
		}
	}
	else if (overallVol > 0x300)
	{
		volFactor = 1;
		while(overallVol > 0x300)
		{
			volFactor *= 2;
			overallVol /= 2;
		}
		
		for (curChip = 0; curChip < _devices.size(); curChip ++)
		{
			CHIP_DEVICE& chipDev = _devices[curChip];
			for (clDev = &chipDev.base; clDev != NULL; clDev = clDev->linkDev)
			{
				clDev->resmpl.volumeL /= volFactor;
				clDev->resmpl.volumeR /= volFactor;
			}
		}
	}
	
	return;
}

void VGMPlayer::InitDevices(void)
{
	UINT8 vgmChip;
	UINT8 chipType;
	UINT8 chipID;
	size_t curChip;
	UINT8 retVal;
	
	_songDevCfg.clear();
	_devices.clear();
	for (vgmChip = 0x00; vgmChip < _CHIP_COUNT; vgmChip ++)
	{
		for (chipID = 0; chipID < 2; chipID ++)
			_vdDevMap[vgmChip][chipID] = (size_t)-1;
	}
	for (curChip = 0; curChip < _OPT_DEV_COUNT * 2; curChip ++)
		_optDevMap[curChip] = (size_t)-1;
	
	for (vgmChip = 0x00; vgmChip < _CHIP_COUNT; vgmChip ++)
	{
		for (chipID = 0; chipID < GetChipCount(vgmChip); chipID ++)
		{
			UINT32 hdrClock;
			DEV_GEN_CFG devCfg;
			CHIP_DEVICE chipDev;
			DEV_INFO* devInf;
			SONG_DEV_CFG sdCfg;
			PLR_DEV_OPTS* devOpts;
			
			memset(&chipDev, 0x00, sizeof(CHIP_DEVICE));
			devInf = &chipDev.base.defInf;
			
			chipType = _DEV_LIST[vgmChip];
			hdrClock = GetChipClock(vgmChip, chipID);
			chipDev.vgmChipType = vgmChip;
			chipDev.chipID = chipID;
			chipDev.optID = _devOptMap[chipType][chipID];
			chipDev.base.defInf.dataPtr = NULL;
			chipDev.base.linkDev = NULL;
			
			devOpts = (chipDev.optID != (size_t)-1) ? &_devOpts[chipDev.optID] : NULL;
			devCfg.emuCore = (devOpts != NULL) ? devOpts->emuCore : 0x00;
			devCfg.srMode = (devOpts != NULL) ? devOpts->srMode : DEVRI_SRMODE_NATIVE;
			devCfg.flags = (hdrClock & 0x80000000) >> 31;
			devCfg.clock = hdrClock & ~0xC0000000;
			if (devOpts != NULL && devOpts->smplRate)
				devCfg.smplRate = devOpts->smplRate;
			else
				devCfg.smplRate = _outSmplRate;
			
			switch(chipType)
			{
			case DEVID_SN76496:
				{
					SN76496_CFG snCfg;
					
					if (! devCfg.emuCore)
						devCfg.emuCore = FCC_MAME;
					devCfg.flags = 0x00;
					snCfg._genCfg = devCfg;
					snCfg.shiftRegWidth = _hdrBuffer[0x2A];
					if (! snCfg.shiftRegWidth)
						snCfg.shiftRegWidth = 0x10;
					snCfg.noiseTaps = ReadLE16(&_hdrBuffer[0x28]);
					if (! snCfg.noiseTaps)
						snCfg.noiseTaps = 0x09;
					snCfg.segaPSG = (_hdrBuffer[0x2B] & 0x01) ? 0 : 1;
					snCfg.negate = (_hdrBuffer[0x2B] & 0x02) ? 1 : 0;
					snCfg.stereo = (_hdrBuffer[0x2B] & 0x04) ? 0 : 1;
					snCfg.clkDiv = (_hdrBuffer[0x2B] & 0x08) ? 1 : 8;
					snCfg.ncrPSG = (_hdrBuffer[0x2B] & 0x10) ? 1 : 0;
					snCfg.t6w28_tone = NULL;
					
					if ((chipID & 0x01) && (hdrClock & 0x80000000))	// must be 2nd chip + T6W28 mode
					{
						CHIP_DEVICE* otherDev = GetDevicePtr(vgmChip, chipID ^ 0x01);
						if (otherDev != NULL)
							snCfg.t6w28_tone = otherDev->base.defInf.dataPtr;
					}
					
					SaveDeviceConfig(sdCfg.cfgData, &snCfg, sizeof(SN76496_CFG));
					retVal = SndEmu_Start(chipType, (DEV_GEN_CFG*)&snCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				}
				break;
			case DEVID_SEGAPCM:
				{
					SEGAPCM_CFG spCfg;
					
					spCfg._genCfg = devCfg;
					spCfg.bnkshift = _hdrBuffer[0x3C];
					spCfg.bnkmask = _hdrBuffer[0x3E];
					SaveDeviceConfig(sdCfg.cfgData, &spCfg, sizeof(SEGAPCM_CFG));
					retVal = SndEmu_Start(chipType, (DEV_GEN_CFG*)&spCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_RF5C68:
				if (! devCfg.emuCore)
				{
					if (vgmChip == 0x05)	// RF5C68
						devCfg.emuCore = FCC_MAME;
					else //if (vgmChip == 0x10)	// RF5C164
						devCfg.emuCore = FCC_GENS;
				}
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				break;
			case DEVID_YM2610:
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'A', (void**)&chipDev.romSize);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'A', (void**)&chipDev.romWrite);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 'B', (void**)&chipDev.romSizeB);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 'B', (void**)&chipDev.romWriteB);
				break;
			case DEVID_YMF278B:
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x524F, (void**)&chipDev.romSize);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x524F, (void**)&chipDev.romWrite);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0x5241, (void**)&chipDev.romSizeB);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0x5241, (void**)&chipDev.romWriteB);
				LoadOPL4ROM(&chipDev);
				break;
			case DEVID_AY8910:
				{
					AY8910_CFG ayCfg;
					
					if (! devCfg.emuCore)
						devCfg.emuCore = FCC_EMU_;
					ayCfg._genCfg = devCfg;
					ayCfg.chipType = _hdrBuffer[0x78];
					ayCfg.chipFlags = _hdrBuffer[0x79];
					
					SaveDeviceConfig(sdCfg.cfgData, &ayCfg, sizeof(AY8910_CFG));
					retVal = SndEmu_Start(chipType, (DEV_GEN_CFG*)&ayCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				}
				break;
			case DEVID_32X_PWM:
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D16, 0, (void**)&chipDev.writeD16);
				break;
			case DEVID_YMW258:
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D16, 0, (void**)&chipDev.writeD16);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				break;
			case DEVID_OKIM6258:
				{
					OKIM6258_CFG okiCfg;
					
					okiCfg._genCfg = devCfg;
					okiCfg.divider = (_hdrBuffer[0x94] & 0x03) >> 0;
					okiCfg.adpcmBits = (_hdrBuffer[0x94] & 0x04) ? OKIM6258_ADPCM_4B : OKIM6258_ADPCM_3B;
					okiCfg.outputBits = (_hdrBuffer[0x94] & 0x08) ? OKIM6258_OUT_12B : OKIM6258_OUT_10B;
					
					SaveDeviceConfig(sdCfg.cfgData, &okiCfg, sizeof(OKIM6258_CFG));
					retVal = SndEmu_Start(chipType, (DEV_GEN_CFG*)&okiCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				}
				break;
			case DEVID_K054539:
				{
					if (devCfg.clock < 1000000)	// if < 1 MHz, then it's the sample rate, not the clock
						devCfg.clock *= 384;	// (for backwards compatibility with old VGM logs from 2012/13)
					devCfg.flags = _hdrBuffer[0x95];
					
					SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
					retVal = SndEmu_Start(chipType, &devCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_C140:
				{
					chipDev.flags = 0x00;
					if (_hdrBuffer[0x96] == 2)	// Namco ASIC 219
					{
						chipDev.flags |= 0x01;	// enable 16-bit byteswap patch on all ROM data
						if (devCfg.clock == 44100)
							devCfg.clock = 25056500;
						else if (devCfg.clock < 1000000)	// if < 1 MHz, then it's the (incorrect) sample rate, not the clock
							devCfg.clock *= 576;	// (for backwards compatibility with old VGM logs from 2013/14)
						chipType = DEVID_C219;
					}
					else
					{
						if (devCfg.clock == 21390)
							devCfg.clock = 12288000;
						else if (devCfg.clock < 1000000)	// if < 1 MHz, then it's the (incorrect) sample rate, not the clock
							devCfg.clock *= 576;	// (for backwards compatibility with old VGM logs from 2013/14)
						devCfg.flags = _hdrBuffer[0x96];	// banking type
						chipType = DEVID_C140;
					}
					SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
					retVal = SndEmu_Start(chipType, &devCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_C352:
				{
					devCfg.clock = devCfg.clock * 72 / _hdrBuffer[0xD6];	// real clock = VGM clock / (VGM clkDiv * 4) * 288
					
					SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
					retVal = SndEmu_Start(chipType, &devCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D16, 0, (void**)&chipDev.writeM16);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_QSOUND:
				chipDev.flags = 0x00;
				if (devCfg.clock < 5000000)	// QSound clock used to be 4 MHz
				{
					devCfg.clock = devCfg.clock * 15;	// 4 MHz -> 60 MHz
					chipDev.flags |= 0x01;	// enable QSound hacks (required for proper playback of old VGMs)
				}
				if (! devCfg.emuCore)
					devCfg.emuCore = FCC_CTR_;
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_QUICKWRITE, DEVRW_A8D16, 0, (void**)&chipDev.writeD16);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				
				memset(&_qsWork[chipID], 0x00, sizeof(QSOUND_WORK));
				if (devCfg.emuCore == FCC_MAME)
					chipDev.flags &= ~0x01;	// MAME's old HLE doesn't need those hacks
				if (chipDev.writeD16 != NULL)
					_qsWork[chipID].write = &VGMPlayer::WriteQSound_A;
				else if (chipDev.write8 != NULL)
					_qsWork[chipID].write = &VGMPlayer::WriteQSound_B;
				else
					_qsWork[chipID].write = NULL;
				break;
			case DEVID_WSWAN:
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
				break;
			case DEVID_ES5503:
				{
					devCfg.flags = _hdrBuffer[0xD4];	// output channels
					
					SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
					retVal = SndEmu_Start(chipType, &devCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_ES5506:
				{
					devCfg.flags = _hdrBuffer[0xD5];	// output channels
					
					SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
					retVal = SndEmu_Start(chipType, &devCfg, devInf);
					if (retVal)
						break;
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D16, 0, (void**)&chipDev.writeD16);
					SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				}
				break;
			case DEVID_SCSP:
				if (devCfg.clock < 1000000)	// if < 1 MHz, then it's the sample rate, not the clock
					devCfg.clock *= 512;	// (for backwards compatibility with old VGM logs from 2012-14)
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D16, 0, (void**)&chipDev.writeM16);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				break;
			default:
				if (! devCfg.emuCore)
				{
					if (chipType == DEVID_YM2612)
						devCfg.emuCore = FCC_GPGX;
					else if (chipType == DEVID_YM3812)
						devCfg.emuCore = FCC_ADLE;
					else if (chipType == DEVID_YMF262)
						devCfg.emuCore = FCC_NUKE;
					else if (chipType == DEVID_NES_APU)
						devCfg.emuCore = FCC_NSFP;
					else if (chipType == DEVID_C6280)
						devCfg.emuCore = FCC_OOTK;
					else if (chipType == DEVID_SAA1099)
						devCfg.emuCore = FCC_VBEL;
				}
				
				SaveDeviceConfig(sdCfg.cfgData, &devCfg, sizeof(DEV_GEN_CFG));
				retVal = SndEmu_Start(chipType, &devCfg, devInf);
				if (retVal)
					break;
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&chipDev.write8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A16D8, 0, (void**)&chipDev.writeM8);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&chipDev.romSize);
				SndEmu_GetDeviceFunc(devInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&chipDev.romWrite);
				if (chipType == DEVID_YM2612)
					devInf->devDef->SetOptionBits(devInf->dataPtr, 0x80);	// enable legacy mode
				else if (chipType == DEVID_GB_DMG)
					devInf->devDef->SetOptionBits(devInf->dataPtr, 0x80);	// enable legacy mode
				break;
			}
			
			sdCfg.deviceID = (! retVal) ? _devices.size() : (size_t)-1;
			sdCfg.vgmChipType = vgmChip;
			sdCfg.type = chipType;
			sdCfg.instance = chipID;
			_songDevCfg.push_back(sdCfg);	// chip init working doesn't matter here - configuration is song-info
			
			if (retVal)
			{
				devInf->dataPtr = NULL;
				devInf->devDef = NULL;
				continue;
			}
			
			SetupLinkedDevices(&chipDev.base, &DeviceLinkCallback, this);
			// already done by SndEmu_Start()
			//devInf->devDef->Reset(devInf->dataPtr);
			
			if (devOpts != NULL)
			{
				if (devInf->devDef->SetOptionBits != NULL)
					devInf->devDef->SetOptionBits(devInf->dataPtr, devOpts->coreOpts);
				RefreshMuting(chipDev, devOpts->muteOpts);
			}
			if (devInf->devDef->SetPanning != NULL)
			{
				if (chipType == DEVID_SN76496)
				{
					INT16 panPos[4] = {0x00, -0x80, +0x80, 0x00};
					devInf->devDef->SetPanning(devInf->dataPtr, panPos);
				}
				else if (chipType == DEVID_YM2413)
				{
					INT16 panPos[14] = {
						-0x100, +0x100, -0x80, +0x80, -0x40, +0x40, -0xC0, +0xC0, 0x00,
						-0x60, +0x60, 0x00, -0xC0, +0xC0};
					devInf->devDef->SetPanning(devInf->dataPtr, panPos);
				}
				else if (chipType == DEVID_AY8910)
				{
					INT16 panPos[3] = {-0x80, +0x80, 0x00};
					devInf->devDef->SetPanning(devInf->dataPtr, panPos);
				}
			}
			
			_vdDevMap[vgmChip][chipID] = _devices.size();
			if (chipDev.optID != (size_t)-1)
				_optDevMap[chipDev.optID] = _devices.size();
			_devices.push_back(chipDev);
		}	// for (chipID)
	}	// end for (vgmChip)
	
	// Initializing the resampler has to be done separately due to reallocations happening above
	// and the memory address of the RESMPL_STATE mustn't change in order to allow callbacks from the devices.
	for (curChip = 0; curChip < _devices.size(); curChip ++)
	{
		CHIP_DEVICE& chipDev = _devices[curChip];
		VGM_BASEDEV* clDev;
		UINT8 linkCntr = 0;
		for (clDev = &chipDev.base; clDev != NULL; clDev = clDev->linkDev, linkCntr ++)
		{
			UINT16 chipVol = GetChipVolume(chipDev.vgmChipType, chipDev.chipID, linkCntr);
			
			Resmpl_SetVals(&clDev->resmpl, 0xFF, chipVol, _outSmplRate);
			Resmpl_DevConnect(&clDev->resmpl, &clDev->defInf);
			Resmpl_Init(&clDev->resmpl);
		}
		
		if (_DEV_LIST[chipDev.vgmChipType] == DEVID_YM3812)
		{
			if (GetChipClock(chipDev.vgmChipType, chipDev.chipID) & 0x80000000)
			{
				// Dual-OPL with Stereo - 1st chip is panned to the left, 2nd chip is panned to the right
				for (clDev = &chipDev.base; clDev != NULL; clDev = clDev->linkDev, linkCntr ++)
				{
					if (chipDev.chipID & 0x01)
					{
						clDev->resmpl.volumeL = 0x00;
						clDev->resmpl.volumeR *= 2;
					}
					else
					{
						clDev->resmpl.volumeL *= 2;
						clDev->resmpl.volumeR = 0x00;
					}
				}
			}
		}
	}
	
	NormalizeOverallVolume(EstimateOverallVolume());
	
	return;
}

/*static*/ void VGMPlayer::DeviceLinkCallback(void* userParam, VGM_BASEDEV* cDev, DEVLINK_INFO* dLink)
{
	DEVLINK_CB_DATA* cbData = (DEVLINK_CB_DATA*)userParam;
	VGMPlayer* oThis = cbData->object;
	
	if (dLink->devID == DEVID_AY8910)
		dLink->cfg->emuCore = FCC_EMU_;
	else if (dLink->devID == DEVID_YMF262)
		dLink->cfg->emuCore = FCC_ADLE;
	
	if (dLink->devID == DEVID_AY8910)
	{
		AY8910_CFG* ayCfg = (AY8910_CFG*)dLink->cfg;
		if (cbData->chipType == DEVID_YM2203)
			ayCfg->chipFlags = oThis->_hdrBuffer[0x7A];	// YM2203 SSG flags
		else if (cbData->chipType == DEVID_YM2608)
			ayCfg->chipFlags = oThis->_hdrBuffer[0x7B];	// YM2608 SSG flags
	}
	
	return;
}

VGMPlayer::CHIP_DEVICE* VGMPlayer::GetDevicePtr(UINT8 chipType, UINT8 chipID)
{
	if (chipType >= _CHIP_COUNT || chipID >= 2)
		return NULL;
	
	size_t devID = _vdDevMap[chipType][chipID];
	if (devID == (size_t)-1)
		return NULL;
	return &_devices[devID];
}

void VGMPlayer::LoadOPL4ROM(CHIP_DEVICE* chipDev)
{
#ifndef NO_EXTERNAL_ROM
	if (chipDev->romWrite == NULL)
		return;
	
	const char* romFile = "yrw801.rom";
	FILE* hFile;
	std::vector<UINT8> yrwData;
	
	hFile = fopen(romFile, "rb");
	if (hFile == NULL)
	{
		debug("Warning: Couldn't load %s!\n", romFile);
		return;
	}
	
	fseek(hFile, 0, SEEK_END);
	yrwData.resize(ftell(hFile));
	rewind(hFile);
	fread(&yrwData[0], 1, yrwData.size(), hFile);
	fclose(hFile);
	
	chipDev->romSize(chipDev->base.defInf.dataPtr, yrwData.size());
	chipDev->romWrite(chipDev->base.defInf.dataPtr, 0x00, yrwData.size(), &yrwData[0]);
#endif
}

UINT8 VGMPlayer::Seek(UINT8 unit, UINT32 pos)
{
	switch(unit)
	{
	case PLAYPOS_FILEOFS:
		_playState |= PLAYSTATE_SEEK;
		if (pos < _filePos)
			Reset();
		return SeekToFilePos(pos);
	case PLAYPOS_SAMPLE:
		pos = Sample2Tick(pos);
		// fall through
	case PLAYPOS_TICK:
		_playState |= PLAYSTATE_SEEK;
		if (pos < _playTick)
			Reset();
		return SeekToTick(pos);
	case PLAYPOS_COMMAND:
	default:
		return 0xFF;
	}
}

UINT8 VGMPlayer::SeekToTick(UINT32 tick)
{
	_playState |= PLAYSTATE_SEEK;
	if (tick > _playTick)
		ParseFile(tick - _playTick);
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	return 0x00;
}

UINT8 VGMPlayer::SeekToFilePos(UINT32 pos)
{
	_playState |= PLAYSTATE_SEEK;
	while(_filePos < _fileHdr.dataEnd && _filePos <= pos && ! (_playState & PLAYSTATE_END))
	{
		UINT8 curCmd = _fileData[_filePos];
		COMMAND_FUNC func = _CMD_INFO[curCmd].func;
		(this->*func)();
		_filePos += _CMD_INFO[curCmd].cmdLen;
	}
	_playTick = _fileTick;
	_playSmpl = Tick2Sample(_playTick);
	
	if (_filePos >= _fileHdr.dataEnd)
	{
		_playState |= PLAYSTATE_END;
		_psTrigger |= PLAYSTATE_END;
		if (_eventCbFunc != NULL)
			_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
		debug("VGM file ends early! (filePos 0x%06X, end at 0x%06X)\n", _filePos, _fileHdr.dataEnd);
	}
	_playState &= ~PLAYSTATE_SEEK;
	
	return 0x00;
}

UINT32 VGMPlayer::Render(UINT32 smplCnt, WAVE_32BS* data)
{
	UINT32 curSmpl;
	UINT32 smplFileTick;
	size_t curDev;
	
	for (curSmpl = 0; curSmpl < smplCnt; curSmpl ++)
	{
		smplFileTick = Sample2Tick(_playSmpl);
		ParseFile(smplFileTick - _playTick);
		_playSmpl ++;
		
		for (curDev = 0; curDev < _devices.size(); curDev ++)
		{
			CHIP_DEVICE* cDev = &_devices[curDev];
			UINT8 disable = (cDev->optID != (size_t)-1) ? _devOpts[cDev->optID].muteOpts.disable : 0x00;
			VGM_BASEDEV* clDev;
			
			for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev, disable >>= 1)
			{
				if (clDev->defInf.dataPtr != NULL && ! (disable & 0x01))
					Resmpl_Execute(&clDev->resmpl, 1, &data[curSmpl]);
			}
		}
		for (curDev = 0; curDev < _dacStreams.size(); curDev ++)
		{
			DEV_INFO* dacDInf = &_dacStreams[curDev].defInf;
			dacDInf->devDef->Update(dacDInf->dataPtr, 1, NULL);
		}
		
		if (_psTrigger & PLAYSTATE_END)
		{
			_psTrigger &= ~PLAYSTATE_END;
			return curSmpl + 1;
		}
	}
	
	return curSmpl;
}

void VGMPlayer::ParseFile(UINT32 ticks)
{
	_playTick += ticks;
	if (_playState & PLAYSTATE_END)
		return;
	
	while(_filePos < _fileHdr.dataEnd && _fileTick <= _playTick && ! (_playState & PLAYSTATE_END))
	{
		UINT8 curCmd = _fileData[_filePos];
		COMMAND_FUNC func = _CMD_INFO[curCmd].func;
		(this->*func)();
		_filePos += _CMD_INFO[curCmd].cmdLen;
	}
	
	if (_filePos >= _fileHdr.dataEnd)
	{
		_playState |= PLAYSTATE_END;
		_psTrigger |= PLAYSTATE_END;
		if (_eventCbFunc != NULL)
			_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
		debug("VGM file ends early! (filePos 0x%06X, end at 0x%06X)\n", _filePos, _fileHdr.dataEnd);
	}
	
	return;
}
