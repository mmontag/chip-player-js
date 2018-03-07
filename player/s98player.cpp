#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <string>
#include <algorithm>

#include <iconv.h>

#define INLINE	static inline

#include <common_def.h>
#include "s98player.hpp"
#include <emu/EmuStructs.h>
#include <emu/SoundEmu.h>
#include <emu/Resampler.h>
#include <emu/SoundDevs.h>
#include <emu/EmuCores.h>
#include <emu/cores/sn764intf.h>	// for SN76496_CFG
#include <emu/cores/ayintf.h>		// for AY8910_CFG
#include "helper.h"




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
static const UINT8 S98_DEV_LIST[17] = {
	0xFF,
	DEVID_AY8910, DEVID_YM2203, DEVID_YM2612, DEVID_YM2608,
	DEVID_YM2151, DEVID_YM2413, DEVID_YM3526, DEVID_YM3812,
	DEVID_YMF262, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, DEVID_AY8910, DEVID_SN76496,
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

S98Player::S98Player() :
	_outSmplRate(0),
	_filePos(0),
	_fileTick(0),
	_playTick(0),
	_playSmpl(0),
	_curLoop(0),
	_playState(0x00),
	_psTrigger(0x00),
	_eventCbFunc(NULL),
	_eventCbParam(NULL)
{
	_icSJIS = iconv_open("UTF-8", "CP932");
}

S98Player::~S98Player()
{
	if (_icSJIS != (iconv_t)-1)
		iconv_close(_icSJIS);
}

UINT8 S98Player::LoadFile(const char* fileName)
{
	FILE* hFile;
	char fileSig[4];
	size_t readBytes;
	size_t fileSize;
	UINT32 devCount;
	UINT32 curDev;
	UINT32 curPos;
	
	hFile = fopen(fileName, "rb");
	if (hFile == NULL)
		return 0xFF;
	
	fseek(hFile, 0, SEEK_END);
	fileSize = ftell(hFile);
	rewind(hFile);
	
	readBytes = fread(fileSig, 1, 4, hFile);
	if (readBytes < 4 || memcmp(fileSig, "S98", 3) || fileSize < 0x20)
	{
		fclose(hFile);
		return 0xF0;	// invalid file
	}
	if (! (fileSig[3] >= '0' && fileSig[3] <= '3'))
	{
		fclose(hFile);
		return 0xF1;	// unsupported version
	}
	
	_fileData.resize(fileSize);
	memcpy(&_fileData[0], fileSig, 4);
	
	readBytes = 4 + fread(&_fileData[4], 1, fileSize - 4, hFile);
	if (readBytes < _fileData.size())
		_fileData.resize(readBytes);	// file was not completely read
	
	fclose(hFile);
	
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
	
	LoadTags();
	
	return 0x00;
}

UINT8 S98Player::LoadTags(void)
{
	_tagData.clear();
	if (! _fileHdr.tagOfs)
		return 0x00;
	
	char* startPtr;
	char* endPtr;
	
	// find end of string (can be either '\0' or EOF)
	startPtr = (char*)&_fileData[_fileHdr.tagOfs];
	endPtr = (char*)memchr(startPtr, '\0', _fileData.size() - _fileHdr.tagOfs);
	if (endPtr == NULL)
		endPtr = (char*)&_fileData[0] + _fileData.size();
	
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
			printf("Invalid S98 tag data!\n");
			printf("tagData size: %zu, Signature: %.5s\n", endPtr - startPtr, startPtr);
			return 0xF0;
		}
		startPtr += 5;
		if (endPtr - startPtr >= 3)
		{
			if (startPtr[0] == 0xEF && startPtr[1] == 0xBB && startPtr[2] == 0xBF)	// check for UTF-8 BOM
			{
				tagIsUTF8 = true;
				startPtr += 3;
				printf("Info: Tags are UTF-8 encoded.");
			}
		}
		
		if (! tagIsUTF8)
			tagData = GetUTF8String(startPtr, endPtr);
		else
			tagData.assign(startPtr, endPtr);
		ParsePSFTags(tagData);
	}
	
	return 0x00;
}

std::string S98Player::GetUTF8String(const char* startPtr, const char* endPtr)
{
	if (_icSJIS != (iconv_t)-1)
	{
		size_t convSize = 0;
		char* convData = NULL;
		std::string result;
		UINT8 retVal;
		
		retVal = StrCharsetConv(_icSJIS, &convSize, &convData, endPtr - startPtr, startPtr);
		
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
	_playState = 0x00;
	_fileData.clear();
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

const char* S98Player::GetSongTitle(void)
{
	std::map<std::string, std::string>::const_iterator mapIt;
	
	mapIt = _tagData.find("TITLE");
	if (mapIt != _tagData.end())
		return mapIt->second.c_str();
	else
		return NULL;
}

UINT32 S98Player::GetSampleRate(void) const
{
	return _outSmplRate;
}

UINT8 S98Player::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return false;
	
	_outSmplRate = sampleRate;
	return true;
}

UINT8 S98Player::SetPlaybackSpeed(double speed)
{
	return false;	// not yet supported
}

void S98Player::SetCallback(PLAYER_EVENT_CB cbFunc, void* cbParam)
{
	_eventCbFunc = cbFunc;
	_eventCbParam = cbParam;
}


void S98Player::RefreshTSRates(void)
{
	_tsMult = _outSmplRate * _fileHdr.tickMult;
	_tsDiv = _fileHdr.tickDiv;
	
	return;
}

UINT32 S98Player::Tick2Sample(UINT32 ticks) const
{
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 S98Player::Sample2Tick(UINT32 samples) const
{
	return (UINT32)(samples * _tsDiv / _tsMult);
}

UINT8 S98Player::GetState(void) const
{
	return _playState;
}

UINT32 S98Player::GetCurFileOfs(void) const
{
	return _filePos;
}

UINT32 S98Player::GetCurTick(void) const
{
	return _playTick;
}

UINT32 S98Player::GetCurSample(void) const
{
	return _playSmpl;
}

UINT32 S98Player::GetTotalTicks(void) const
{
	return 0;
}

UINT32 S98Player::GetTotalPlayTicks(UINT32 numLoops) const
{
	return 0;
}

UINT32 S98Player::GetCurrentLoop(void) const
{
	return _curLoop;
}


UINT8 S98Player::Start(void)
{
	size_t curDev;
	UINT8 retVal;
	
	_devices.clear();
	_devices.resize(_devHdrs.size());
	for (curDev = 0; curDev < _devHdrs.size(); curDev ++)
	{
		const S98_DEVICE* devHdr = &_devHdrs[curDev];
		S98_CHIPDEV* cDev = &_devices[curDev];
		DEV_GEN_CFG devCfg;
		VGM_BASEDEV* clDev;
		UINT8 deviceID;
		
		cDev->base.defInf.dataPtr = NULL;
		cDev->base.linkDev = NULL;
		devCfg.emuCore = 0x00;
		devCfg.srMode = DEVRI_SRMODE_NATIVE;
		devCfg.flags = 0x00;
		devCfg.clock = devHdr->clock;
		devCfg.smplRate = _outSmplRate;
		
		deviceID = (devHdr->devType < S98DEV_END) ? S98_DEV_LIST[devHdr->devType] : 0xFF;
		if (deviceID == 0xFF)
		{
			cDev->base.defInf.dataPtr = NULL;
			cDev->base.defInf.devDef = NULL;
			continue;
		}
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
				}
				else
				{
					ayCfg.chipType = AYTYPE_AY8910;
					ayCfg.chipFlags = 0x00;
					devCfg.clock /= 2;
				}
				
				retVal = SndEmu_Start(deviceID, (DEV_GEN_CFG*)&ayCfg, &cDev->base.defInf);
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
				
				retVal = SndEmu_Start(deviceID, (DEV_GEN_CFG*)&snCfg, &cDev->base.defInf);
			}
			break;
		default:
			retVal = SndEmu_Start(deviceID, &devCfg, &cDev->base.defInf);
			break;
		}
		if (retVal)
		{
			cDev->base.defInf.dataPtr = NULL;
			cDev->base.defInf.devDef = NULL;
			continue;
		}
		SndEmu_GetDeviceFunc(cDev->base.defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write);
		
		// TODO: use callback to fix SSG volume
		SetupLinkedDevices(&cDev->base, NULL, NULL);
		
		clDev = &cDev->base;
		while(clDev != NULL)
		{
			Resmpl_SetVals(&clDev->resmpl, 0xFF, 0x100, _outSmplRate);
			Resmpl_DevConnect(&clDev->resmpl, &clDev->defInf);
			Resmpl_Init(&clDev->resmpl);
			clDev = clDev->linkDev;
		}
	}
	
	_playState |= PLAYSTATE_PLAY;
	Reset();
	if (_eventCbFunc != NULL)
		_eventCbFunc(_eventCbParam, PLREVT_START, NULL);
	
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
		_eventCbFunc(_eventCbParam, PLREVT_STOP, NULL);
	
	return 0x00;
}

UINT8 S98Player::Reset(void)
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
		S98_CHIPDEV* cDev = &_devices[curDev];
		VGM_BASEDEV* clDev;
		
		cDev->base.defInf.devDef->Reset(cDev->base.defInf.dataPtr);
		clDev = &cDev->base;
		while(clDev != NULL)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
			clDev = clDev->linkDev;
		}
		
		if (_devHdrs[curDev].devType == S98DEV_OPNA)
		{
			DEV_INFO* defInf = &cDev->base.defInf;
			DEVFUNC_WRITE_MEMSIZE SetRamSize = NULL;
			
			// setup DeltaT RAM size
			SndEmu_GetDeviceFunc(defInf->devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE, 0, (void**)&SetRamSize);
			if (SetRamSize != NULL)
				SetRamSize(defInf->dataPtr, 0x40000);	// 256 KB
			
			// The YM2608 defaults to OPN mode. (YM2203 fallback),
			// so put it into OPNA mode (6 channels).
			cDev->write(defInf->dataPtr, 0, 0x29);
			cDev->write(defInf->dataPtr, 1, 0x80);
		}
	}
	
	return 0x00;
}

UINT32 S98Player::Render(UINT32 smplCnt, WAVE_32BS* data)
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
			S98_CHIPDEV* cDev = &_devices[curDev];
			VGM_BASEDEV* clDev;
			
			clDev = &cDev->base;
			while(clDev != NULL)
			{
				if (clDev->defInf.dataPtr != NULL)
					Resmpl_Execute(&clDev->resmpl, 1, &data[curSmpl]);
				clDev = clDev->linkDev;
			};
		}
		if (_psTrigger & PLAYSTATE_END)
		{
			_psTrigger &= ~PLAYSTATE_END;
			return curSmpl + 1;
		}
	}
	
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

void S98Player::DoCommand(void)
{
	if (_filePos >= _fileData.size())
	{
		_playState |= PLAYSTATE_END;
		_psTrigger |= PLAYSTATE_END;
		return;
	}
	
	switch(_fileData[_filePos])
	{
	case 0xFF:	// advance 1 tick
		_fileTick ++;
		_filePos ++;
		return;
	case 0xFE:	// advance multiple ticks
		_filePos ++;
		{
			UINT32 tickVal = 0;
			UINT8 tickShift = 0;
			UINT8 moreFlag;
			
			do
			{
				moreFlag = _fileData[_filePos] & 0x80;
				tickVal |= (_fileData[_filePos] & 0x7F) << tickShift;
				tickShift += 7;
				_filePos ++;
			} while(moreFlag);
			
			_fileTick += 2 + tickVal;
		}
		return;
	case 0xFD:
		if (! _fileHdr.loopOfs)
		{
			_playState |= PLAYSTATE_END;
			_psTrigger |= PLAYSTATE_END;
			if (_eventCbFunc != NULL)
				_eventCbFunc(_eventCbParam, PLREVT_END, NULL);
		}
		else
		{
			_curLoop ++;
			if (_eventCbFunc != NULL)
			{
				UINT8 retVal;
				
				retVal = _eventCbFunc(_eventCbParam, PLREVT_LOOP, &_curLoop);
				if (retVal == 0x01)
				{
					_playState |= PLAYSTATE_END;
					_psTrigger |= PLAYSTATE_END;
					return;
				}
			}
			_filePos = _fileHdr.loopOfs;
		}
		return;
	}
	
	{
		UINT8 deviceID = _fileData[_filePos + 0x00] >> 1;
		UINT8 port = _fileData[_filePos + 0x00] & 0x01;
		UINT8 reg = _fileData[_filePos + 0x01];
		UINT8 data = _fileData[_filePos + 0x02];
		if (deviceID < _devices.size())
		{
			S98_CHIPDEV* cDev = &_devices[deviceID];
			DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
			if (_devHdrs[deviceID].devType == S98DEV_DCSG)
			{
				if (reg == 1)	// GG stereo
					cDev->write(dataPtr, 0x01, data);
				else
					cDev->write(dataPtr, 0x00, data);
			}
			else
			{
				cDev->write(dataPtr, (port << 1) | 0, reg);
				cDev->write(dataPtr, (port << 1) | 1, data);
			}
		}
	}
	_filePos += 0x03;
	return;
}
