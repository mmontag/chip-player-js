#include <stdlib.h>
#include <string.h>
#include <stdio.h>	// for snprintf()
#include <ctype.h>
#include <vector>
#include <string>
#include <algorithm>

#define INLINE	static inline

#include "../common_def.h"
#include "s98player.hpp"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"
#include "../emu/Resampler.h"
#include "../emu/SoundDevs.h"
#include "../emu/EmuCores.h"
#include "../emu/cores/sn764intf.h"	// for SN76496_CFG
#include "../emu/cores/ayintf.h"		// for AY8910_CFG
#include "../utils/StrUtils.h"
#include "helper.h"
#include "../emu/logging.h"

#ifdef _MSC_VER
#define snprintf	_snprintf
#endif


enum S98_DEVTYPES
{
	S98DEV_NONE = 0,	// S98 v2 End-Of-Device marker
	S98DEV_PSGYM = 1,	// YM2149
	S98DEV_OPN = 2,		// YM2203
	S98DEV_OPN2 = 3,	// YM2612
	S98DEV_OPNA = 4,	// YM2608
	S98DEV_OPM = 5,		// YM2151
	// S98 v3 device types
	S98DEV_OPLL = 6,	// YM2413
	S98DEV_OPL = 7,		// YM3526
	S98DEV_OPL2 = 8,	// YM3812
	S98DEV_OPL3 = 9,	// YMF262
	S98DEV_PSGAY = 15,	// AY-3-8910
	S98DEV_DCSG = 16,	// SN76489
	S98DEV_END
};
static const UINT8 S98_DEV_LIST[S98DEV_END] = {
	0xFF,
	DEVID_AY8910, DEVID_YM2203, DEVID_YM2612, DEVID_YM2608,
	DEVID_YM2151, DEVID_YM2413, DEVID_YM3526, DEVID_YM3812,
	DEVID_YMF262, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, DEVID_AY8910, DEVID_SN76496,
};

static const char* const S98_TAG_MAPPING[] =
{
	"TITLE", "TITLE",
	"ARTIST", "ARTIST",
	"GAME", "GAME",
	"YEAR", "DATE",
	"GENRE", "GENRE",
	"COMMENT", "COMMENT",
	"COPYRIGHT", "COPYRIGHT",
	"S98BY", "ENCODED_BY",
	"SYSTEM", "SYSTEM",
	NULL,
};

/*static*/ const UINT8 S98Player::_OPT_DEV_LIST[_OPT_DEV_COUNT] =
{
	DEVID_AY8910, DEVID_YM2203, DEVID_YM2612, DEVID_YM2608,
	DEVID_YM2151, DEVID_YM2413, DEVID_YM3526, DEVID_YM3812,
	DEVID_YMF262, DEVID_SN76496,
};


INLINE UINT16 ReadLE16(const UINT8* data)
{
	return (data[0x01] << 8) | (data[0x00] << 0);
}

INLINE UINT32 ReadLE32(const UINT8* data)
{
	return	(data[0x03] << 24) | (data[0x02] << 16) |
			(data[0x01] <<  8) | (data[0x00] <<  0);
}

INLINE void SaveDeviceConfig(std::vector<UINT8>& dst, const void* srcData, size_t srcLen)
{
	const UINT8* srcPtr = (const UINT8*)srcData;
	dst.assign(srcPtr, srcPtr + srcLen);
	return;
}

S98Player::S98Player() :
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

	_playOpts.genOpts.pbSpeed = 0x10000;

	_lastTsMult = 0;
	_lastTsDiv = 0;
	
	dev_logger_set(&_logger, this, S98Player::PlayerLogCB, NULL);
	
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
			InitDeviceOptions(_devOpts[optID]);
			_devOptMap[_OPT_DEV_LIST[optChip]][chipID] = optID;
		}
	}
	
	retVal = CPConv_Init(&_cpcSJIS, "CP932", "UTF-8");
	if (retVal)
		_cpcSJIS = NULL;
	_tagList.reserve(16);
	_tagList.push_back(NULL);
}

S98Player::~S98Player()
{
	_eventCbFunc = NULL;	// prevent any callbacks during destruction
	
	if (_playState & PLAYSTATE_PLAY)
		Stop();
	UnloadFile();
	
	if (_cpcSJIS != NULL)
		CPConv_Deinit(_cpcSJIS);
}

UINT32 S98Player::GetPlayerType(void) const
{
	return FCC_S98;
}

const char* S98Player::GetPlayerName(void) const
{
	return "S98";
}

/*static*/ UINT8 S98Player::PlayerCanLoadFile(DATA_LOADER *dataLoader)
{
	DataLoader_ReadUntil(dataLoader,0x20);
	if (DataLoader_GetSize(dataLoader) < 0x20)
		return 0xF1;	// file too small
	if (memcmp(&DataLoader_GetData(dataLoader)[0x00], "S98", 3))
		return 0xF0;	// invalid signature
	return 0x00;
}

UINT8 S98Player::CanLoadFile(DATA_LOADER *dataLoader) const
{
	return this->PlayerCanLoadFile(dataLoader);
}

UINT8 S98Player::LoadFile(DATA_LOADER *dataLoader)
{
	UINT32 devCount;
	UINT32 curDev;
	UINT32 curPos;
	
	_dLoad = NULL;
	DataLoader_ReadUntil(dataLoader,0x20);
	_fileData = DataLoader_GetData(dataLoader);
	if (DataLoader_GetSize(dataLoader) < 0x20 || memcmp(&_fileData[0x00], "S98", 3))
		return 0xF0;	// invalid file
	if (! (_fileData[0x03] >= '0' && _fileData[0x03] <= '3'))
		return 0xF1;	// unsupported version
	
	_dLoad = dataLoader;
	DataLoader_ReadAll(_dLoad);
	_fileData = DataLoader_GetData(_dLoad);
	
	_fileHdr.fileVer = _fileData[0x03] - '0';
	_fileHdr.tickMult = ReadLE32(&_fileData[0x04]);
	_fileHdr.tickDiv = ReadLE32(&_fileData[0x08]);
	_fileHdr.compression = ReadLE32(&_fileData[0x0C]);
	_fileHdr.tagOfs = ReadLE32(&_fileData[0x10]);
	_fileHdr.dataOfs = ReadLE32(&_fileData[0x14]);
	_fileHdr.loopOfs = ReadLE32(&_fileData[0x18]);
	
	_devHdrs.clear();
	switch(_fileHdr.fileVer)
	{
	case 0:
		_fileHdr.tickMult = 0;
		// fall through
	case 1:
		_fileHdr.tickDiv = 0;
		// only default device available
		break;
	case 2:
		curPos = 0x20;
		for (devCount = 0; ; devCount ++, curPos += 0x10)
		{
			if (ReadLE32(&_fileData[curPos + 0x00]) == S98DEV_NONE)
				break;	// stop at device type 0
		}
		
		curPos = 0x20;
		_devHdrs.resize(devCount);
		for (curDev = 0; curDev < devCount; curDev ++, curPos += 0x10)
		{
			_devHdrs[curDev].devType = ReadLE32(&_fileData[curPos + 0x00]);
			_devHdrs[curDev].clock = ReadLE32(&_fileData[curPos + 0x04]);
			_devHdrs[curDev].pan = 0;
			_devHdrs[curDev].app_spec = ReadLE32(&_fileData[curPos + 0x0C]);
		}
		break;	// not supported yet
	case 3:
		devCount = ReadLE32(&_fileData[0x1C]);
		curPos = 0x20;
		_devHdrs.resize(devCount);
		for (curDev = 0; curDev < devCount; curDev ++, curPos += 0x10)
		{
			_devHdrs[curDev].devType = ReadLE32(&_fileData[curPos + 0x00]);
			_devHdrs[curDev].clock = ReadLE32(&_fileData[curPos + 0x04]);
			_devHdrs[curDev].pan = ReadLE32(&_fileData[curPos + 0x08]);
			_devHdrs[curDev].app_spec = 0;
		}
		break;
	}
	if (_devHdrs.empty())
	{
		_devHdrs.resize(1);
		curDev = 0;
		_devHdrs[curDev].devType = S98DEV_OPNA;
		_devHdrs[curDev].clock = 7987200;
		_devHdrs[curDev].pan = 0;
		_devHdrs[curDev].app_spec = 0;
	}
	
	if (! _fileHdr.tickMult)
		_fileHdr.tickMult = 10;
	if (! _fileHdr.tickDiv)
		_fileHdr.tickDiv = 1000;
	
	GenerateDeviceConfig();
	CalcSongLength();
	
	if (_fileHdr.loopOfs)
	{
		if (_fileHdr.loopOfs < _fileHdr.dataOfs || _fileHdr.loopOfs >= DataLoader_GetSize(_dLoad))
		{
			emu_logf(&_logger, PLRLOG_WARN, "Invalid loop offset 0x%06X - ignoring!\n", _fileHdr.loopOfs);
			_fileHdr.loopOfs = 0x00;
		}
		if (_fileHdr.loopOfs && _loopTick == _totalTicks)
		{
			// 0-Sample-Loops causes the program to hang in the playback routine
			emu_logf(&_logger, PLRLOG_WARN, "Warning! Ignored Zero-Sample-Loop!\n");
			_fileHdr.loopOfs = 0x00;
		}
	}
	
	// parse tags
	LoadTags();
	
	RefreshTSRates();	// make Tick2Sample etc. work
	
	return 0x00;
}

void S98Player::CalcSongLength(void)
{
	UINT32 filePos;
	bool fileEnd;
	UINT8 curCmd;
	
	_totalTicks = 0;
	_loopTick = 0;
	
	fileEnd = false;
	filePos = _fileHdr.dataOfs;
	while(! fileEnd && filePos < DataLoader_GetSize(_dLoad))
	{
		if (filePos == _fileHdr.loopOfs)
			_loopTick = _totalTicks;
		
		curCmd = _fileData[filePos];
		filePos ++;
		switch(curCmd)
		{
		case 0xFF:	// advance 1 tick
			_totalTicks ++;
			break;
		case 0xFE:	// advance multiple ticks
			_totalTicks += 2 + ReadVarInt(filePos);
			break;
		case 0xFD:
			fileEnd = true;
			break;
		default:
			filePos += 0x02;
			break;
		}
	}
	
	return;
}

UINT8 S98Player::LoadTags(void)
{
	_tagData.clear();
	_tagList.clear();
	_tagList.push_back(NULL);
	if (! _fileHdr.tagOfs)
		return 0x00;
	if (_fileHdr.tagOfs >= DataLoader_GetSize(_dLoad))
		return 0xF3;	// tag error (offset out-of-range)
	
	const char* startPtr;
	const char* endPtr;
	
	// find end of string (can be either '\0' or EOF)
	startPtr = (const char*)&_fileData[_fileHdr.tagOfs];
	endPtr = (const char*)memchr(startPtr, '\0', DataLoader_GetSize(_dLoad) - _fileHdr.tagOfs);
	if (endPtr == NULL)
		endPtr = (const char*)_fileData + DataLoader_GetSize(_dLoad);
	
	if (_fileHdr.fileVer < 3)
	{
		// tag offset = song title (\0-terminated)
		_tagData["TITLE"] = GetUTF8String(startPtr, endPtr);
	}
	else
	{
		std::string tagData;
		bool tagIsUTF8 = false;
		
		// tag offset = PSF tag
		if (endPtr - startPtr < 5 || memcmp(startPtr, "[S98]", 5))
		{
			emu_logf(&_logger, PLRLOG_ERROR, "Invalid S98 tag data!\n");
			emu_logf(&_logger, PLRLOG_DEBUG, "tagData size: %zu, Signature: %.5s\n", endPtr - startPtr, startPtr);
			return 0xF0;
		}
		startPtr += 5;
		if (endPtr - startPtr >= 3)
		{
			if (! memcmp(&startPtr[0], "\xEF\xBB\xBF", 3))	// check for UTF-8 BOM
			{
				tagIsUTF8 = true;
				startPtr += 3;
				emu_logf(&_logger, PLRLOG_DEBUG, "Note: Tags are UTF-8 encoded.\n");
			}
		}
		
		if (tagIsUTF8)
			tagData.assign(startPtr, endPtr);
		else
			tagData = GetUTF8String(startPtr, endPtr);
		ParsePSFTags(tagData);
	}
	
	_tagList.clear();
	
	std::map<std::string, std::string>::const_iterator mapIt;
	for (mapIt = _tagData.begin(); mapIt != _tagData.end(); ++ mapIt)
	{
		std::string curKey = mapIt->first;
		std::transform(curKey.begin(), curKey.end(), curKey.begin(), ::toupper);
		
		const char *tagName = NULL;
		for (const char* const* t = S98_TAG_MAPPING; *t != NULL; t += 2)
		{
			if (curKey == t[0])
			{
				tagName = t[1];
				break;
			}
		}
		
		if (tagName)
			_tagList.push_back(tagName);
		else
			_tagList.push_back(curKey.c_str());
		_tagList.push_back(mapIt->second.c_str());
	}
	
	_tagList.push_back(NULL);
	
	return 0x00;
}

std::string S98Player::GetUTF8String(const char* startPtr, const char* endPtr)
{
	if (startPtr == endPtr)
		return std::string();
	
	if (_cpcSJIS != NULL)
	{
		size_t convSize = 0;
		char* convData = NULL;
		std::string result;
		UINT8 retVal;
		
		retVal = CPConv_StrConvert(_cpcSJIS, &convSize, &convData, endPtr - startPtr, startPtr);
		
		result.assign(convData, convData + convSize);
		free(convData);
		if (retVal < 0x80)
			return result;
	}
	// unable to convert - fallback using the original string
	return std::string(startPtr, endPtr);
}

static std::string TrimPSFTagWhitespace(const std::string& data)
{
	size_t posStart;
	size_t posEnd;
	
	// according to the PSF tag specification, all characters 0x01..0x20 are considered whitespace
	// http://wiki.neillcorlett.com/PSFTagFormat
	for (posStart = 0; posStart < data.length(); posStart ++)
	{
		if ((unsigned char)data[posStart] > 0x20)
			break;
	}
	for (posEnd = data.length(); posEnd > 0; posEnd --)
	{
		if ((unsigned char)data[posEnd - 1] > 0x20)
			break;
	}
	return data.substr(posStart, posEnd - posStart);
}

static UINT8 ExtractKeyValue(const std::string& line, std::string& retKey, std::string& retValue)
{
	size_t equalPos;
	
	equalPos = line.find('=');
	if (equalPos == std::string::npos)
		return 0xFF;	// not a "key=value" line
	
	retKey = line.substr(0, equalPos);
	retValue = line.substr(equalPos + 1);
	
	retKey = TrimPSFTagWhitespace(retKey);
	if (retKey.empty())
		return 0x01;	// invalid key
	retValue = TrimPSFTagWhitespace(retValue);
	
	return 0x00;
}

UINT8 S98Player::ParsePSFTags(const std::string& tagData)
{
	size_t lineStart;
	size_t lineEnd;
	std::string curLine;
	std::string curKey;
	std::string curVal;
	UINT8 retVal;
	
	lineStart = 0;
	while(lineStart < tagData.length())
	{
		lineEnd = tagData.find('\n', lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = tagData.length();
		
		curLine = tagData.substr(lineStart, lineEnd - lineStart);
		retVal = ExtractKeyValue(curLine, curKey, curVal);
		if (! retVal)
		{
			std::map<std::string, std::string>::iterator mapIt;
			
			// keys are case insensitive, so let's make it uppercase
			std::transform(curKey.begin(), curKey.end(), curKey.begin(), ::toupper);
			mapIt = _tagData.find(curKey);
			if (mapIt == _tagData.end())
				_tagData[curKey] = curVal;	// new value
			else
				mapIt->second = mapIt->second + '\n' + curVal;	// multiline-value
		}
		
		lineStart = lineEnd + 1;
	}
	
	return 0x00;
}

UINT8 S98Player::UnloadFile(void)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0xFF;
	
	_playState = 0x00;
	_dLoad = NULL;
	_fileData = NULL;
	_fileHdr.fileVer = 0xFF;
	_fileHdr.dataOfs = 0x00;
	_devHdrs.clear();
	_devices.clear();
	_tagData.clear();
	
	return 0x00;
}

const S98_HEADER* S98Player::GetFileHeader(void) const
{
	return &_fileHdr;
}

const char* const* S98Player::GetTags(void)
{
	return &_tagList[0];
}

UINT8 S98Player::GetSongInfo(PLR_SONG_INFO& songInf)
{
	if (_dLoad == NULL)
		return 0xFF;
	
	songInf.format = FCC_S98;
	songInf.fileVerMaj = _fileHdr.fileVer;
	songInf.fileVerMin = 0x00;
	songInf.tickRateMul = _fileHdr.tickMult;
	songInf.tickRateDiv = _fileHdr.tickDiv;
	songInf.songLen = GetTotalTicks();
	songInf.loopTick = _fileHdr.loopOfs ? GetLoopTicks() : (UINT32)-1;
	songInf.volGain = 0x10000;
	songInf.deviceCnt = (UINT32)_devHdrs.size();
	
	return 0x00;
}

UINT8 S98Player::GetSongDeviceInfo(std::vector<PLR_DEV_INFO>& devInfList) const
{
	if (_dLoad == NULL)
		return 0xFF;
	
	size_t curDev;
	
	devInfList.clear();
	devInfList.reserve(_devHdrs.size());
	for (curDev = 0; curDev < _devHdrs.size(); curDev ++)
	{
		const S98_DEVICE* devHdr = &_devHdrs[curDev];
		PLR_DEV_INFO devInf;
		memset(&devInf, 0x00, sizeof(PLR_DEV_INFO));
		
		devInf.id = (UINT32)curDev;
		devInf.type = S98_DEV_LIST[devHdr->devType];
		devInf.instance = GetDeviceInstance(curDev);
		devInf.devCfg = (const DEV_GEN_CFG*)&_devCfgs[curDev].data[0];
		if (! _devices.empty())
		{
			const VGM_BASEDEV& cDev = _devices[curDev].base;
			devInf.core = (cDev.defInf.devDef != NULL) ? cDev.defInf.devDef->coreID : 0x00;
			devInf.volume = (cDev.resmpl.volumeL + cDev.resmpl.volumeR) / 2;
			devInf.smplRate = cDev.defInf.sampleRate;
		}
		else
		{
			devInf.core = 0x00;
			devInf.volume = 0x100;
			devInf.smplRate = 0;
		}
		devInfList.push_back(devInf);
	}
	if (! _devices.empty())
		return 0x01;	// returned "live" data
	else
		return 0x00;	// returned data based on file header
}

UINT8 S98Player::GetDeviceInstance(size_t id) const
{
	const S98_DEVICE* mainDHdr = &_devHdrs[id];
	UINT8 mainDType = (mainDHdr->devType < S98DEV_END) ? S98_DEV_LIST[mainDHdr->devType] : 0xFF;
	UINT8 instance = 0;
	size_t curDev;
	
	for (curDev = 0; curDev < id; curDev ++)
	{
		const S98_DEVICE* dHdr = &_devHdrs[curDev];
		UINT8 dType = (dHdr->devType < S98DEV_END) ? S98_DEV_LIST[dHdr->devType] : 0xFF;
		if (dType == mainDType)
			instance ++;
	}
	
	return instance;
}

size_t S98Player::DeviceID2OptionID(UINT32 id) const
{
	UINT8 type;
	UINT8 instance;
	
	if (id & 0x80000000)
	{
		type = (id >> 0) & 0xFF;
		instance = (id >> 16) & 0xFF;
	}
	else if (id < _devHdrs.size())
	{
		UINT32 s98DevType;
		
		s98DevType = _devHdrs[id].devType;
		type = (s98DevType < S98DEV_END) ? S98_DEV_LIST[s98DevType] : 0xFF;
		instance = GetDeviceInstance(id);
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

void S98Player::RefreshMuting(S98_CHIPDEV& chipDev, const PLR_MUTE_OPTS& muteOpts)
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

void S98Player::RefreshPanning(S98_CHIPDEV& chipDev, const PLR_PAN_OPTS& panOpts)
{
	VGM_BASEDEV* clDev;
	UINT8 linkCntr = 0;
	
	for (clDev = &chipDev.base; clDev != NULL && linkCntr < 2; clDev = clDev->linkDev, linkCntr ++)
	{
		DEV_INFO* devInf = &clDev->defInf;
		if (devInf->dataPtr == NULL)
			continue;
		DEVFUNC_PANALL funcPan = NULL;
		UINT8 retVal = SndEmu_GetDeviceFunc(devInf->devDef, RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, (void**)&funcPan);
		if (retVal != EERR_NOT_FOUND && funcPan != NULL)
			funcPan(devInf->dataPtr, &panOpts.chnPan[linkCntr][0]);
	}
	
	return;
}

UINT8 S98Player::SetDeviceOptions(UINT32 id, const PLR_DEV_OPTS& devOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID] = devOpts;
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
	{
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
		RefreshPanning(_devices[devID], _devOpts[optID].panOpts);
	}
	return 0x00;
}

UINT8 S98Player::GetDeviceOptions(UINT32 id, PLR_DEV_OPTS& devOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	devOpts = _devOpts[optID];
	return 0x00;
}

UINT8 S98Player::SetDeviceMuting(UINT32 id, const PLR_MUTE_OPTS& muteOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID].muteOpts = muteOpts;
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
	return 0x00;
}

UINT8 S98Player::GetDeviceMuting(UINT32 id, PLR_MUTE_OPTS& muteOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	muteOpts = _devOpts[optID].muteOpts;
	return 0x00;
}

UINT8 S98Player::SetPlayerOptions(const S98_PLAY_OPTIONS& playOpts)
{
	_playOpts = playOpts;
	RefreshTSRates();
	return 0x00;
}

UINT8 S98Player::GetPlayerOptions(S98_PLAY_OPTIONS& playOpts) const
{
	playOpts = _playOpts;
	return 0x00;
}

UINT8 S98Player::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0x01;	// can't set during playback
	
	_outSmplRate = sampleRate;
	return 0x00;
}

UINT8 S98Player::SetPlaybackSpeed(double speed)
{
	_playOpts.genOpts.pbSpeed = (UINT32)(0x10000 * speed);
	RefreshTSRates();
	return 0x00;
}


void S98Player::RefreshTSRates(void)
{
	_tsMult = _outSmplRate * _fileHdr.tickMult;
	_tsDiv = _fileHdr.tickDiv;
	if (_playOpts.genOpts.pbSpeed != 0 && _playOpts.genOpts.pbSpeed != 0x10000)
	{
		_tsMult *= 0x10000;
		_tsDiv *= _playOpts.genOpts.pbSpeed;
	}
	if (_tsMult != _lastTsMult ||
	    _tsDiv != _lastTsDiv)
	{
		if (_lastTsMult && _lastTsDiv)	// the order * / * / is required to avoid overflow
			_playSmpl = (UINT32)(_playSmpl * _lastTsDiv / _lastTsMult * _tsMult / _tsDiv);
		_lastTsMult = _tsMult;
		_lastTsDiv = _tsDiv;
	}
	return;
}

UINT32 S98Player::Tick2Sample(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1;
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 S98Player::Sample2Tick(UINT32 samples) const
{
	if (samples == (UINT32)-1)
		return -1;
	return (UINT32)(samples * _tsDiv / _tsMult);
}

double S98Player::Tick2Second(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1.0;
	return ticks * _fileHdr.tickMult / (double)_fileHdr.tickDiv;
}

UINT8 S98Player::GetState(void) const
{
	return _playState;
}

UINT32 S98Player::GetCurPos(UINT8 unit) const
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

UINT32 S98Player::GetCurLoop(void) const
{
	return _curLoop;
}

UINT32 S98Player::GetTotalTicks(void) const
{
	return _totalTicks;
}

UINT32 S98Player::GetLoopTicks(void) const
{
	if (! _fileHdr.loopOfs)
		return 0;
	else
		return _totalTicks - _loopTick;
}

/*static*/ void S98Player::PlayerLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	S98Player* player = (S98Player*)source;
	if (player->_logCbFunc == NULL)
		return;
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_PLR, NULL, message);
	return;
}

/*static*/ void S98Player::SndEmuLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	DEVLOG_CB_DATA* cbData = (DEVLOG_CB_DATA*)userParam;
	S98Player* player = cbData->player;
	if (player->_logCbFunc == NULL)
		return;
	if ((player->_playState & PLAYSTATE_SEEK) && level > PLRLOG_ERROR)
		return;	// prevent message spam while seeking
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_EMU,
		player->_devNames[cbData->chipDevID].c_str(), message);
	return;
}


void S98Player::GenerateDeviceConfig(void)
{
	size_t curDev;
	
	_devCfgs.clear();
	_devNames.clear();
	_devCfgs.resize(_devHdrs.size());
	for (curDev = 0; curDev < _devCfgs.size(); curDev ++)
	{
		const S98_DEVICE* devHdr = &_devHdrs[curDev];
		DEV_GEN_CFG devCfg;
		UINT8 deviceID;
		
		memset(&devCfg, 0x00, sizeof(DEV_GEN_CFG));
		devCfg.clock = devHdr->clock;
		devCfg.flags = 0x00;
		
		deviceID = (devHdr->devType < S98DEV_END) ? S98_DEV_LIST[devHdr->devType] : 0xFF;
		const char* devName = SndEmu_GetDevName(deviceID, 0x00, &devCfg);	// use short name for now
		switch(deviceID)
		{
		case DEVID_AY8910:
			{
				AY8910_CFG ayCfg;
				
				ayCfg._genCfg = devCfg;
				if (devHdr->devType == S98DEV_PSGYM)
				{
					ayCfg.chipType = AYTYPE_YM2149;
					ayCfg.chipFlags = YM2149_PIN26_LOW;
					devName = "YM2149";
				}
				else
				{
					ayCfg.chipType = AYTYPE_AY8910;
					ayCfg.chipFlags = 0x00;
					ayCfg._genCfg.clock /= 2;
					devName = "AY8910";
				}
				
				SaveDeviceConfig(_devCfgs[curDev].data, &ayCfg, sizeof(AY8910_CFG));
			}
			break;
		case DEVID_SN76496:
			{
				SN76496_CFG snCfg;
				
				snCfg._genCfg = devCfg;
				snCfg.shiftRegWidth = 0x10;
				snCfg.noiseTaps = 0x09;
				snCfg.segaPSG = 1;
				snCfg.negate = 0;
				snCfg.stereo = 1;
				snCfg.clkDiv = 8;
				snCfg.t6w28_tone = NULL;
				
				SaveDeviceConfig(_devCfgs[curDev].data, &snCfg, sizeof(SN76496_CFG));
			}
			break;
		default:
			SaveDeviceConfig(_devCfgs[curDev].data, &devCfg, sizeof(DEV_GEN_CFG));
			break;
		}
		if (_devCfgs.size() <= 1)
		{
			_devNames.push_back(devName);
		}
		else
		{
			char fullName[0x10];
			snprintf(fullName, 0x10, "%u-%s", 1 + (unsigned int)curDev, devName);
			_devNames.push_back(fullName);
		}
	}
	
	return;
}

/*static*/ void S98Player::DeviceLinkCallback(void* userParam, VGM_BASEDEV* cDev, DEVLINK_INFO* dLink)
{
	DEVLINK_CB_DATA* cbData = (DEVLINK_CB_DATA*)userParam;
	S98Player* oThis = cbData->player;
	const S98_CHIPDEV& chipDev = *cbData->chipDev;
	const PLR_DEV_OPTS* devOpts = (chipDev.optID != (size_t)-1) ? &oThis->_devOpts[chipDev.optID] : NULL;
	
	if (devOpts != NULL && devOpts->emuCore[1])
	{
		// set emulation core of linked device (OPN(A) SSG)
		dLink->cfg->emuCore = devOpts->emuCore[1];
	}
	
	return;
}

UINT8 S98Player::Start(void)
{
	size_t curDev;
	UINT8 retVal;
	
	for (curDev = 0; curDev < _OPT_DEV_COUNT * 2; curDev ++)
		_optDevMap[curDev] = (size_t)-1;
	
	_devices.clear();
	_devices.resize(_devHdrs.size());
	for (curDev = 0; curDev < _devHdrs.size(); curDev ++)
	{
		const S98_DEVICE* devHdr = &_devHdrs[curDev];
		S98_CHIPDEV* cDev = &_devices[curDev];
		DEV_GEN_CFG* devCfg = (DEV_GEN_CFG*)&_devCfgs[curDev].data[0];
		VGM_BASEDEV* clDev;
		PLR_DEV_OPTS* devOpts;
		UINT8 deviceID;
		UINT8 instance;
		
		cDev->base.defInf.dataPtr = NULL;
		cDev->base.defInf.devDef = NULL;
		cDev->base.linkDev = NULL;
		deviceID = (devHdr->devType < S98DEV_END) ? S98_DEV_LIST[devHdr->devType] : 0xFF;
		if (deviceID == 0xFF)
			continue;
		
		instance = GetDeviceInstance(curDev);
		if (instance < 2)
		{
			cDev->optID = _devOptMap[deviceID][instance];
			devOpts = &_devOpts[cDev->optID];
		}
		else
		{
			cDev->optID = (size_t)-1;
			devOpts = NULL;
		}
		devCfg->emuCore = (devOpts != NULL) ? devOpts->emuCore[0] : 0x00;
		devCfg->srMode = (devOpts != NULL) ? devOpts->srMode : DEVRI_SRMODE_NATIVE;
		if (devOpts != NULL && devOpts->smplRate)
			devCfg->smplRate = devOpts->smplRate;
		else
			devCfg->smplRate = _outSmplRate;
		
		retVal = SndEmu_Start(deviceID, devCfg, &cDev->base.defInf);
		if (retVal)
		{
			cDev->base.defInf.dataPtr = NULL;
			cDev->base.defInf.devDef = NULL;
			continue;
		}
		SndEmu_GetDeviceFunc(cDev->base.defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write);
		
		cDev->logCbData.player = this;
		cDev->logCbData.chipDevID = curDev;
		if (cDev->base.defInf.devDef->SetLogCB != NULL)
			cDev->base.defInf.devDef->SetLogCB(cDev->base.defInf.dataPtr, S98Player::SndEmuLogCB, &cDev->logCbData);
		{
			DEVLINK_CB_DATA dlCbData;
			dlCbData.player = this;
			dlCbData.chipDev = cDev;
			SetupLinkedDevices(&cDev->base, &DeviceLinkCallback, &dlCbData);
		}
		
		if (devOpts != NULL)
		{
			if (cDev->base.defInf.devDef->SetOptionBits != NULL)
				cDev->base.defInf.devDef->SetOptionBits(cDev->base.defInf.dataPtr, devOpts->coreOpts);
			RefreshMuting(*cDev, devOpts->muteOpts);
			RefreshPanning(*cDev, devOpts->panOpts);
			
			_optDevMap[cDev->optID] = curDev;
		}
		if (cDev->base.defInf.linkDevCount > 0 && cDev->base.defInf.linkDevs[0].devID == DEVID_AY8910)
		{
			VGM_BASEDEV* clDev = cDev->base.linkDev;
			size_t optID = DeviceID2OptionID(PLR_DEV_ID(DEVID_AY8910, instance));
			if (optID != (size_t)-1 && clDev != NULL && clDev->defInf.devDef->SetOptionBits != NULL)
				clDev->defInf.devDef->SetOptionBits(cDev->base.defInf.dataPtr, _devOpts[optID].coreOpts);
		}
		
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			Resmpl_SetVals(&clDev->resmpl, 0xFF, 0x100, _outSmplRate);
			if (deviceID == DEVID_YM2203 || deviceID == DEVID_YM2608)
			{
				// set SSG volume
				if (clDev != &cDev->base)
					clDev->resmpl.volumeL = clDev->resmpl.volumeR = 0xCD;
			}
			Resmpl_DevConnect(&clDev->resmpl, &clDev->defInf);
			Resmpl_Init(&clDev->resmpl);
		}
	}
	
	_playState |= PLAYSTATE_PLAY;
	Reset();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_START, NULL);
	
	return 0x00;
}

UINT8 S98Player::Stop(void)
{
	size_t curDev;
	
	_playState &= ~PLAYSTATE_PLAY;
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		S98_CHIPDEV* cDev = &_devices[curDev];
		FreeDeviceTree(&cDev->base, 0);
	}
	_devices.clear();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_STOP, NULL);
	
	return 0x00;
}

UINT8 S98Player::Reset(void)
{
	size_t curDev;
	std::vector<UINT8> tmp(0x40000, 0x00);
	
	_filePos = _fileHdr.dataOfs;
	_fileTick = 0;
	_playTick = 0;
	_playSmpl = 0;
	_playState &= ~PLAYSTATE_END;
	_psTrigger = 0x00;
	_curLoop = 0;
	_lastLoopTick = 0;
	
	RefreshTSRates();
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		S98_CHIPDEV* cDev = &_devices[curDev];
		DEV_INFO* defInf = &cDev->base.defInf;
		VGM_BASEDEV* clDev;
		if (defInf->dataPtr == NULL)
			continue;
		
		defInf->devDef->Reset(defInf->dataPtr);
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
		}
		
		if (_devHdrs[curDev].devType == S98DEV_OPNA)
		{
			DEVFUNC_WRITE_MEMSIZE SetRamSize = NULL;
			DEVFUNC_WRITE_BLOCK SetRamData = NULL;
			
			// setup DeltaT RAM size
			SndEmu_GetDeviceFunc(defInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&SetRamSize);
			SndEmu_GetDeviceFunc(defInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK, 0, (void**)&SetRamData);
			if (SetRamSize != NULL)
				SetRamSize(defInf->dataPtr, 0x40000);	// 256 KB
			// initialize DeltaT RAM with 00, because come S98 files seem to expect that (e.g. King Breeder/11.s98)
			if (SetRamData != NULL)
				SetRamData(defInf->dataPtr, 0x00, (UINT32)tmp.size(), &tmp[0]);
			
			// The YM2608 defaults to OPN mode. (YM2203 fallback),
			// so put it into OPNA mode (6 channels).
			cDev->write(defInf->dataPtr, 0, 0x29);
			cDev->write(defInf->dataPtr, 1, 0x80);
		}
	}
	
	return 0x00;
}

UINT8 S98Player::Seek(UINT8 unit, UINT32 pos)
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

UINT8 S98Player::SeekToTick(UINT32 tick)
{
	_playState |= PLAYSTATE_SEEK;
	if (tick > _playTick)
		ParseFile(tick - _playTick);
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	return 0x00;
}

UINT8 S98Player::SeekToFilePos(UINT32 pos)
{
	_playState |= PLAYSTATE_SEEK;
	while(_filePos <= pos && ! (_playState & PLAYSTATE_END))
		DoCommand();
	_playTick = _fileTick;
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	
	return 0x00;
}

UINT32 S98Player::Render(UINT32 smplCnt, WAVE_32BS* data)
{
	UINT32 curSmpl;
	UINT32 smplFileTick;
	UINT32 maxSmpl;
	INT32 smplStep;	// might be negative due to rounding errors in Tick2Sample
	size_t curDev;
	
	// Note: use do {} while(), so that "smplCnt == 0" can be used to process until reaching the next sample.
	curSmpl = 0;
	do
	{
		smplFileTick = Sample2Tick(_playSmpl);
		ParseFile(smplFileTick - _playTick);
		
		// render as many samples at once as possible (for better performance)
		maxSmpl = Tick2Sample(_fileTick);
		smplStep = maxSmpl - _playSmpl;
		if (smplStep < 1)
			smplStep = 1;	// must render at least 1 sample in order to advance
		if ((UINT32)smplStep > smplCnt - curSmpl)
			smplStep = smplCnt - curSmpl;
		
		for (curDev = 0; curDev < _devices.size(); curDev ++)
		{
			S98_CHIPDEV* cDev = &_devices[curDev];
			UINT8 disable = (cDev->optID != (size_t)-1) ? _devOpts[cDev->optID].muteOpts.disable : 0x00;
			VGM_BASEDEV* clDev;
			
			for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev, disable >>= 1)
			{
				if (clDev->defInf.dataPtr != NULL && ! (disable & 0x01))
					Resmpl_Execute(&clDev->resmpl, smplStep, &data[curSmpl]);
			}
		}
		curSmpl += smplStep;
		_playSmpl += smplStep;
		if (_psTrigger & PLAYSTATE_END)
		{
			_psTrigger &= ~PLAYSTATE_END;
			break;
		}
	} while(curSmpl < smplCnt);
	
	return curSmpl;
}

void S98Player::ParseFile(UINT32 ticks)
{
	_playTick += ticks;
	if (_playState & PLAYSTATE_END)
		return;
	
	while(_fileTick <= _playTick && ! (_playState & PLAYSTATE_END))
		DoCommand();
	
	return;
}

void S98Player::HandleEOF(void)
{
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
			UINT8 retVal;
			
			retVal = _eventCbFunc(this, _eventCbParam, PLREVT_LOOP, &_curLoop);
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
	
	return;
}

void S98Player::DoCommand(void)
{
	if (_filePos >= DataLoader_GetSize(_dLoad))
	{
		if (_playState & PLAYSTATE_SEEK)	// recalculate playSmpl to fix state when triggering callbacks
			_playSmpl = Tick2Sample(_fileTick);	// Note: fileTick results in more accurate position
		_playState |= PLAYSTATE_END;
		_psTrigger |= PLAYSTATE_END;
		if (_eventCbFunc != NULL)
			_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
		emu_logf(&_logger, PLRLOG_WARN, "S98 file ends early! (filePos 0x%06X, fileSize 0x%06X)\n", _filePos, DataLoader_GetSize(_dLoad));
		return;
	}
	
	UINT8 curCmd;
	
	curCmd = _fileData[_filePos];
	_filePos ++;
	switch(curCmd)
	{
	case 0xFF:	// advance 1 tick
		_fileTick ++;
		break;
	case 0xFE:	// advance multiple ticks
		_fileTick += 2 + ReadVarInt(_filePos);
		break;
	case 0xFD:
		HandleEOF();
		break;
	default:
		DoRegWrite(curCmd >> 1, curCmd & 0x01, _fileData[_filePos + 0x00], _fileData[_filePos + 0x01]);
		_filePos += 0x02;
		break;
	}
	
	return;
}

void S98Player::DoRegWrite(UINT8 deviceID, UINT8 port, UINT8 reg, UINT8 data)
{
	if (deviceID >= _devices.size())
		return;
	
	S98_CHIPDEV* cDev = &_devices[deviceID];
	DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
	if (dataPtr == NULL || cDev->write == NULL)
		return;
	
	if (_devHdrs[deviceID].devType == S98DEV_DCSG)
	{
		if (reg == 1)	// GG stereo
			cDev->write(dataPtr, SN76496_W_GGST, data);
		else
			cDev->write(dataPtr, SN76496_W_REG, data);
	}
	else
	{
		cDev->write(dataPtr, (port << 1) | 0, reg);
		cDev->write(dataPtr, (port << 1) | 1, data);
	}
	
	return;
}

UINT32 S98Player::ReadVarInt(UINT32& filePos)
{
	UINT32 tickVal = 0;
	UINT8 tickShift = 0;
	UINT8 moreFlag;
	
	do
	{
		moreFlag = _fileData[filePos] & 0x80;
		tickVal |= (_fileData[filePos] & 0x7F) << tickShift;
		tickShift += 7;
		filePos ++;
	} while(moreFlag);
	
	return tickVal;
}
