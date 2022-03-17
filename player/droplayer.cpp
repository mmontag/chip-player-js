#include <stdlib.h>
#include <string.h>
#include <stdio.h>	// for snprintf()
#include <vector>
#include <algorithm>

#define INLINE	static inline

#include "../common_def.h"
#include "droplayer.hpp"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"
#include "../emu/Resampler.h"
#include "../emu/SoundDevs.h"
#include "../emu/EmuCores.h"
#include "helper.h"
#include "../emu/logging.h"

#ifdef _MSC_VER
#define snprintf	_snprintf
#endif


enum DRO_HWTYPES
{
	DROHW_OPL2 = 0,
	DROHW_DUALOPL2 = 1,
	DROHW_OPL3 = 2,
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

DROPlayer::DROPlayer() :
	_tickFreq(1000),
	_filePos(0),
	_fileTick(0),
	_playTick(0),
	_playSmpl(0),
	_playState(0x00),
	_psTrigger(0x00)
{
	size_t curDev;
	
	dev_logger_set(&_logger, this, DROPlayer::PlayerLogCB, NULL);
	
	_playOpts.genOpts.pbSpeed = 0x10000;
	_playOpts.v2opl3Mode = DRO_V2OPL3_DETECT;
	
	_lastTsMult = 0;
	_lastTsDiv = 0;
	
	for (curDev = 0; curDev < 3; curDev ++)
		InitDeviceOptions(_devOpts[curDev]);
	_initRegSet.resize(0x200, false);
}

DROPlayer::~DROPlayer()
{
	_eventCbFunc = NULL;	// prevent any callbacks during destruction
	
	if (_playState & PLAYSTATE_PLAY)
		Stop();
	UnloadFile();
}

UINT32 DROPlayer::GetPlayerType(void) const
{
	return FCC_DRO;
}

const char* DROPlayer::GetPlayerName(void) const
{
	return "DRO";
}

/*static*/ UINT8 DROPlayer::PlayerCanLoadFile(DATA_LOADER *dataLoader)
{
	DataLoader_ReadUntil(dataLoader,0x10);
	if (DataLoader_GetSize(dataLoader) < 0x10)
		return 0xF1;	// file too small
	if (memcmp(&DataLoader_GetData(dataLoader)[0x00], "DBRAWOPL", 8))
		return 0xF0;	// invalid signature
	return 0x00;
}

UINT8 DROPlayer::CanLoadFile(DATA_LOADER *dataLoader) const
{
	return this->PlayerCanLoadFile(dataLoader);
}

UINT8 DROPlayer::LoadFile(DATA_LOADER *dataLoader)
{
	UINT32 tempLng;
	
	_dLoad = NULL;
	DataLoader_ReadUntil(dataLoader,0x10);
	_fileData = DataLoader_GetData(dataLoader);
	if (DataLoader_GetSize(dataLoader) < 0x10 || memcmp(&_fileData[0x00], "DBRAWOPL", 8))
		return 0xF0;	// invalid file
	
	// --- try to detect the DRO version ---
	tempLng = ReadLE32(&_fileData[0x08]);
	if (tempLng & 0xFF00FF00)
	{
		// DRO v0 - This version didn't write version bytes.
		_fileHdr.verMajor = 0x00;
		_fileHdr.verMinor = 0x00;
	}
	else if (! (tempLng & 0x0000FFFF))
	{
		// DRO v1 - order is: minor, major
		_fileHdr.verMinor = ReadLE16(&_fileData[0x08]);
		_fileHdr.verMajor = ReadLE16(&_fileData[0x0A]);
	}
	else
	{
		// DRO v2 - order is: major, minor
		_fileHdr.verMajor = ReadLE16(&_fileData[0x08]);
		_fileHdr.verMinor = ReadLE16(&_fileData[0x0A]);
	}
	if (_fileHdr.verMajor > 2)
		return 0xF1;	// unsupported version
	
	_dLoad = dataLoader;
	DataLoader_ReadAll(_dLoad);
	_fileData = DataLoader_GetData(_dLoad);
	
	switch(_fileHdr.verMajor)
	{
	case 0:	// version 0 (DOSBox 0.62)
	case 1:	// version 1 (DOSBox 0.63)
		switch(_fileHdr.verMajor)
		{
		case 0:
			_fileHdr.lengthMS = ReadLE32(&_fileData[0x08]);
			_fileHdr.dataSize = ReadLE32(&_fileData[0x0C]);
			_fileHdr.hwType = _fileData[0x10];
			_fileHdr.dataOfs = 0x11;
			break;
		case 1:
			_fileHdr.lengthMS = ReadLE32(&_fileData[0x0C]);
			_fileHdr.dataSize = ReadLE32(&_fileData[0x10]);
			tempLng = ReadLE32(&_fileData[0x14]);
			_fileHdr.hwType = (tempLng <= 0xFF) ? (UINT8)tempLng : 0xFF;
			_fileHdr.dataOfs = 0x18;
			break;
		}
		// swap DualOPL2 and OPL3 values
		if (_fileHdr.hwType == 0x01)
			_fileHdr.hwType = DROHW_OPL3;
		else if (_fileHdr.hwType == 0x02)
			_fileHdr.hwType = DROHW_DUALOPL2;
		_fileHdr.format = 0x00;
		_fileHdr.compression = 0x00;
		_fileHdr.cmdDlyShort = 0x00;
		_fileHdr.cmdDlyLong = 0x01;
		_fileHdr.regCmdCnt = 0x00;
		break;
	case 2:	// version 2 (DOSBox 0.73)
		_fileHdr.dataSize = ReadLE32(&_fileData[0x0C]) * 2;
		_fileHdr.lengthMS = ReadLE32(&_fileData[0x10]);
		_fileHdr.hwType = _fileData[0x14];
		_fileHdr.format = _fileData[0x15];
		_fileHdr.compression = _fileData[0x16];
		_fileHdr.cmdDlyShort = _fileData[0x17];
		_fileHdr.cmdDlyLong = _fileData[0x18];
		_fileHdr.regCmdCnt = _fileData[0x19];
		_fileHdr.dataOfs = 0x1A + _fileHdr.regCmdCnt;
		
		if (_fileHdr.regCmdCnt > 0x80)
			_fileHdr.regCmdCnt = 0x80;	// only 0x80 values are possible
		memcpy(_fileHdr.regCmdMap, &_fileData[0x1A], _fileHdr.regCmdCnt);
		
		break;
	}
	
	ScanInitBlock();
	
	_realHwType = _fileHdr.hwType;
	if (_fileHdr.verMajor >= 2)
	{
		// DOSBox puts "DualOPL2" into the header of DROs that log OPL3 data ...
		// ... unless the "4op enable" register is accessed while OPL3 mode is active.
		// This bug was introduced when DRO logging was rewritten for DOSBox 0.73.
		if (_realHwType == DROHW_DUALOPL2)
		{
			switch(_playOpts.v2opl3Mode)
			{
			case DRO_V2OPL3_DETECT:
				// if OPL3 enable is set, it definitely is an OPL3 file
				if (_initRegSet[0x105] && (_initOPL3Enable & 0x01))
					_realHwType = DROHW_OPL3;
				break;
			case DRO_V2OPL3_HEADER:
				// keep the value from the header
				break;
			case DRO_V2OPL3_ENFORCE:
				_realHwType = DROHW_OPL3;
				break;
			}
		}
	}
	
	_devTypes.clear();
	_devPanning.clear();
	_portShift = 0;
	switch(_realHwType)
	{
	case DROHW_OPL2:	// single OPL2
		_devTypes.push_back(DEVID_YM3812);	_devPanning.push_back(0x00);
		break;
	case DROHW_DUALOPL2:	// dual OPL2
		_devTypes.push_back(DEVID_YM3812);	_devPanning.push_back(0x01);
		_devTypes.push_back(DEVID_YM3812);	_devPanning.push_back(0x02);
		break;
	case DROHW_OPL3:	// single OPL3
	default:
		_devTypes.push_back(DEVID_YMF262);	_devPanning.push_back(0x00);
		_portShift = 1;
		break;
	}
	_portMask = (1 << _portShift) - 1;
	
	_totalTicks = _fileHdr.lengthMS;
	GenerateDeviceConfig();
	
	return 0x00;
}

void DROPlayer::ScanInitBlock(void)
{
	// Scan initialization block of the DRO in order to be able to apply special fixes.
	// Fixes like:
	//	- DualOPL2 vs. OPL3 detection (because DOSBox sets hwType to DualOPL2 more often that it should)
	//	- [DRO v1] detect size of initialization block (for filtering out unescaped writes to register 01/04)
	UINT32 filePos;
	UINT8 curCmd;
	UINT8 selPort;
	UINT16 curReg;
	UINT16 lastReg;
	
	std::fill(_initRegSet.begin(), _initRegSet.end(), false);
	_initOPL3Enable = 0x00;
	
	filePos = _fileHdr.dataOfs;
	if (_fileHdr.verMajor < 2)
	{
		selPort = 0;
		lastReg = 0x000;
		// The file begins with a register dump with increasing register numbers.
		while(filePos < DataLoader_GetSize(_dLoad))
		{
			curCmd = _fileData[filePos];
			if (curCmd == 0x02 || curCmd == 0x03)
			{
				// make an exception for the chip select commands
				selPort = curCmd & 0x01;
				filePos ++;
				continue;
			}
			
			curReg = (selPort << 8) | (curCmd << 0);
			if (curReg < lastReg)
				break;
			
			_initRegSet[curReg] = true;
			if (curReg == 0x105)
				_initOPL3Enable = _fileData[filePos + 0x01];
			lastReg = curReg;
			filePos += 0x02;
		}
		while(filePos < DataLoader_GetSize(_dLoad))
		{
			curCmd = _fileData[filePos];
			
			if (curCmd == 0x00 || curCmd == 0x01)
				break;	// delay command - stop scanning
			if (curCmd == 0x02 || curCmd == 0x03)
			{
				selPort = curCmd & 0x01;
				filePos ++;
				continue;
			}
			if (curCmd == 0x04)
			{
				if (_fileData[filePos + 0x01] < 0x08)
					break;	// properly escaped command - stop scanning
			}
			curReg = (selPort << 8) | (curCmd << 0);
			_initRegSet[curReg] = true;
			if (curReg == 0x105)
				_initOPL3Enable = _fileData[filePos + 0x01];
			filePos += 0x02;
		}
	}
	else //if (_fileHdr.verMajor == 2)
	{
		lastReg = 0x000;
		// The file begins with a register dump with increasing register numbers.
		while(filePos < DataLoader_GetSize(_dLoad))
		{
			curCmd = _fileData[filePos];
			if (curCmd == _fileHdr.cmdDlyShort || curCmd == _fileHdr.cmdDlyLong)
				break;
			
			if ((curCmd & 0x7F) >= _fileHdr.regCmdCnt)
				break;
			curReg = ((curCmd & 0x80) << 1) | (_fileHdr.regCmdMap[curCmd & 0x7F] << 0);
			_initRegSet[curReg] = true;
			if (curReg == 0x105)
				_initOPL3Enable = _fileData[filePos + 0x01];
			filePos += 0x02;
		}
	}
	_initBlkEndOfs = filePos;
	
	return;
}

UINT8 DROPlayer::UnloadFile(void)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0xFF;
	
	_playState = 0x00;
	_dLoad = NULL;
	_fileData = NULL;
	_fileHdr.verMajor = 0xFF;
	_fileHdr.dataOfs = 0x00;
	_devTypes.clear();
	_devPanning.clear();
	_devCfgs.clear();
	_devices.clear();
	_devNames.clear();
	
	return 0x00;
}

const DRO_HEADER* DROPlayer::GetFileHeader(void) const
{
	return &_fileHdr;
}

const char* const* DROPlayer::GetTags(void)
{
	static const char* const tagList[] = { NULL };
	return tagList;
}

UINT8 DROPlayer::GetSongInfo(PLR_SONG_INFO& songInf)
{
	if (_dLoad == NULL)
		return 0xFF;
	
	songInf.format = FCC_DRO;
	songInf.fileVerMaj = _fileHdr.verMajor;
	songInf.fileVerMin = _fileHdr.verMinor;
	songInf.tickRateMul = 1;
	songInf.tickRateDiv = _tickFreq;
	songInf.songLen = GetTotalTicks();
	songInf.loopTick = (UINT32)-1;
	songInf.volGain = 0x10000;
	songInf.deviceCnt = (UINT32)_devTypes.size();
	
	return 0x00;
}

UINT8 DROPlayer::GetSongDeviceInfo(std::vector<PLR_DEV_INFO>& devInfList) const
{
	if (_dLoad == NULL)
		return 0xFF;
	
	size_t curDev;
	
	devInfList.clear();
	devInfList.reserve(_devTypes.size());
	for (curDev = 0; curDev < _devTypes.size(); curDev ++)
	{
		const DEV_GEN_CFG* devCfg = &_devCfgs[curDev];
		PLR_DEV_INFO devInf;
		memset(&devInf, 0x00, sizeof(PLR_DEV_INFO));
		
		devInf.id = (UINT32)curDev;
		devInf.type = _devTypes[curDev];
		devInf.instance = (UINT8)curDev;
		devInf.devCfg = devCfg;
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

size_t DROPlayer::DeviceID2OptionID(UINT32 id) const
{
	UINT8 type;
	UINT8 instance;
	
	if (id & 0x80000000)
	{
		type = (id >> 0) & 0xFF;
		instance = (id >> 16) & 0xFF;
	}
	else if (id < _devTypes.size())
	{
		type = _devTypes[id];
		instance = (UINT8)id;
	}
	else
	{
		return (size_t)-1;
	}
	
	if (type == DEVID_YM3812)
	{
		if (instance < 2)
			return 0 + instance;
	}
	else if (type == DEVID_YMF262)
	{
		if (instance < 1)
			return 2 + instance;
	}
	return (size_t)-1;
}

void DROPlayer::RefreshMuting(DRO_CHIPDEV& chipDev, const PLR_MUTE_OPTS& muteOpts)
{
	DEV_INFO* devInf = &chipDev.base.defInf;
	if (devInf->dataPtr != NULL && devInf->devDef->SetMuteMask != NULL)
		devInf->devDef->SetMuteMask(devInf->dataPtr, muteOpts.chnMute[0]);
	
	return;
}

void DROPlayer::RefreshPanning(DRO_CHIPDEV& chipDev, const PLR_PAN_OPTS& panOpts)
{
	DEV_INFO* devInf = &chipDev.base.defInf;
	if (devInf->dataPtr == NULL)
		return;
	DEVFUNC_PANALL funcPan = NULL;
	UINT8 retVal = SndEmu_GetDeviceFunc(devInf->devDef, RWF_CHN_PAN | RWF_WRITE, DEVRW_ALL, 0, (void**)&funcPan);
	if (retVal != EERR_NOT_FOUND && funcPan != NULL)
		funcPan(devInf->dataPtr, &panOpts.chnPan[0][0]);
	
	return;
}

UINT8 DROPlayer::SetDeviceOptions(UINT32 id, const PLR_DEV_OPTS& devOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID] = devOpts;
	// no immediate changes necessary for OPL2/OPL3
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
	return 0x00;
}

UINT8 DROPlayer::GetDeviceOptions(UINT32 id, PLR_DEV_OPTS& devOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	devOpts = _devOpts[optID];
	return 0x00;
}

UINT8 DROPlayer::SetDeviceMuting(UINT32 id, const PLR_MUTE_OPTS& muteOpts)
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

UINT8 DROPlayer::GetDeviceMuting(UINT32 id, PLR_MUTE_OPTS& muteOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	muteOpts = _devOpts[optID].muteOpts;
	return 0x00;
}

UINT8 DROPlayer::SetPlayerOptions(const DRO_PLAY_OPTIONS& playOpts)
{
	_playOpts = playOpts;
	RefreshTSRates();
	return 0x00;
}

UINT8 DROPlayer::GetPlayerOptions(DRO_PLAY_OPTIONS& playOpts) const
{
	playOpts = _playOpts;
	return 0x00;
}

UINT8 DROPlayer::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0x01;	// can't set during playback
	
	_outSmplRate = sampleRate;
	return 0x00;
}

UINT8 DROPlayer::SetPlaybackSpeed(double speed)
{
	_playOpts.genOpts.pbSpeed = (UINT32)(0x10000 * speed);
	RefreshTSRates();
	return 0x00;
}


void DROPlayer::RefreshTSRates(void)
{
	_tsMult = _outSmplRate;
	_tsDiv = _tickFreq;
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

UINT32 DROPlayer::Tick2Sample(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1;
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 DROPlayer::Sample2Tick(UINT32 samples) const
{
	if (samples == (UINT32)-1)
		return -1;
	return (UINT32)(samples * _tsDiv / _tsMult);
}

double DROPlayer::Tick2Second(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1.0;
	return ticks / (double)_tickFreq;
}

UINT8 DROPlayer::GetState(void) const
{
	return _playState;
}

UINT32 DROPlayer::GetCurPos(UINT8 unit) const
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

UINT32 DROPlayer::GetCurLoop(void) const
{
	return 0;
}

UINT32 DROPlayer::GetTotalTicks(void) const
{
	return _totalTicks;
}

UINT32 DROPlayer::GetLoopTicks(void) const
{
	return 0;
}

/*static*/ void DROPlayer::PlayerLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	DROPlayer* player = (DROPlayer*)source;
	if (player->_logCbFunc == NULL)
		return;
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_PLR, NULL, message);
	return;
}

/*static*/ void DROPlayer::SndEmuLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	DEVLOG_CB_DATA* cbData = (DEVLOG_CB_DATA*)userParam;
	DROPlayer* player = cbData->player;
	if (player->_logCbFunc == NULL)
		return;
	if ((player->_playState & PLAYSTATE_SEEK) && level > PLRLOG_ERROR)
		return;	// prevent message spam while seeking
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_EMU,
		player->_devNames[cbData->chipDevID].c_str(), message);
	return;
}


void DROPlayer::GenerateDeviceConfig(void)
{
	size_t curDev;
	const char* chipName;
	char chipNameFull[0x10];
	
	_devCfgs.clear();
	_devCfgs.resize(_devTypes.size());
	_devNames.clear();
	for (curDev = 0; curDev < _devCfgs.size(); curDev ++)
	{
		DEV_GEN_CFG* devCfg = &_devCfgs[curDev];
		memset(devCfg, 0x00, sizeof(DEV_GEN_CFG));
		
		devCfg->clock = 3579545;
		if (_devTypes[curDev] == DEVID_YMF262)
			devCfg->clock *= 4;	// OPL3 uses a 14 MHz clock
		devCfg->flags = 0x00;
		
		chipName = (_devTypes[curDev] == DEVID_YMF262) ? "OPL3" : "OPL2";
		if (_devCfgs.size() <= 1)
		{
			_devNames.push_back(chipName);
		}
		else
		{
			snprintf(chipNameFull, 0x10, "%s #%u", chipName, 1 + (unsigned int)curDev);
			_devNames.push_back(chipNameFull);
		}
	}
	
	return;
}

UINT8 DROPlayer::Start(void)
{
	size_t curDev;
	UINT8 retVal;
	
	for (curDev = 0; curDev < 3; curDev ++)
		_optDevMap[curDev] = (size_t)-1;
	
	_devices.clear();
	_devices.resize(_devTypes.size());
	for (curDev = 0; curDev < _devTypes.size(); curDev ++)
	{
		DRO_CHIPDEV* cDev = &_devices[curDev];
		PLR_DEV_OPTS* devOpts;
		DEV_GEN_CFG* devCfg = &_devCfgs[curDev];
		VGM_BASEDEV* clDev;
		
		cDev->base.defInf.dataPtr = NULL;
		cDev->base.linkDev = NULL;
		cDev->optID = DeviceID2OptionID((UINT32)curDev);
		
		devOpts = (cDev->optID != (size_t)-1) ? &_devOpts[cDev->optID] : NULL;
		devCfg->emuCore = (devOpts != NULL) ? devOpts->emuCore[0] : 0x00;
		devCfg->srMode = (devOpts != NULL) ? devOpts->srMode : DEVRI_SRMODE_NATIVE;
		if (devOpts != NULL && devOpts->smplRate)
			devCfg->smplRate = devOpts->smplRate;
		else
			devCfg->smplRate = _outSmplRate;
		
		retVal = SndEmu_Start(_devTypes[curDev], devCfg, &cDev->base.defInf);
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
			cDev->base.defInf.devDef->SetLogCB(cDev->base.defInf.dataPtr, DROPlayer::SndEmuLogCB, &cDev->logCbData);
		SetupLinkedDevices(&cDev->base, NULL, NULL);
		
		if (devOpts != NULL)
		{
			if (cDev->base.defInf.devDef->SetOptionBits != NULL)
				cDev->base.defInf.devDef->SetOptionBits(cDev->base.defInf.dataPtr, devOpts->coreOpts);
			
			_optDevMap[cDev->optID] = curDev;
		}
		
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			if (devOpts != NULL && clDev->defInf.devDef->SetMuteMask != NULL)
				clDev->defInf.devDef->SetMuteMask(clDev->defInf.dataPtr, devOpts->muteOpts.chnMute[0]);
			
			Resmpl_SetVals(&clDev->resmpl, 0xFF, 0x100, _outSmplRate);
			// do DualOPL2 hard panning by muting either the left or right speaker
			if (_devPanning[curDev] & 0x02)
				clDev->resmpl.volumeL = 0x00;
			if (_devPanning[curDev] & 0x01)
				clDev->resmpl.volumeR = 0x00;
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

UINT8 DROPlayer::Stop(void)
{
	size_t curDev;
	
	_playState &= ~PLAYSTATE_PLAY;
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		DRO_CHIPDEV* cDev = &_devices[curDev];
		FreeDeviceTree(&cDev->base, 0);
	}
	_devices.clear();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_STOP, NULL);
	
	return 0x00;
}

UINT8 DROPlayer::Reset(void)
{
	size_t curDev;
	UINT8 curReg;
	UINT8 curPort;
	UINT8 devport;
	
	_filePos = _fileHdr.dataOfs;
	_fileTick = 0;
	_playTick = 0;
	_playSmpl = 0;
	_playState &= ~PLAYSTATE_END;
	_psTrigger = 0x00;
	_selPort = 0;

	RefreshTSRates();	

	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		DRO_CHIPDEV* cDev = &_devices[curDev];
		VGM_BASEDEV* clDev;
		if (cDev->base.defInf.dataPtr == NULL)
			continue;
		
		cDev->base.defInf.devDef->Reset(cDev->base.defInf.dataPtr);
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
		}
	}
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		if (_devices[curDev].base.defInf.dataPtr == NULL)
			continue;
		
		if (_devTypes[curDev] == DEVID_YMF262)
			WriteReg((UINT8)(curDev << _portShift) | 1, 0x05, 0x01);	// temporary OPL3 enable for proper register reset
		
		for (curPort = 0; curPort <= _portMask; curPort ++)
		{
			devport = (UINT8)(curDev << _portShift) | curPort;
			for (curReg = 0xFF; curReg >= 0x20; curReg --)
			{
				// [optimization] only send registers that are NOT part of the initialization block
				if (! _initRegSet[(curPort << 8) | curReg])
					WriteReg(devport, curReg, 0x00);
			}
		}
		devport = (UINT8)(curDev << _portShift);
		WriteReg(devport | 0, 0x08, 0x00);
		WriteReg(devport | 0, 0x01, 0x00);
		
		if (_devTypes[curDev] == DEVID_YMF262)
		{
			// send OPL3 mode from DRO file now (DOSBox dumps the registers in the wrong order)
			WriteReg(devport | 1, 0x05, _initOPL3Enable);
			WriteReg(devport | 1, 0x04, 0x00);	// disable 4op mode
		}
	}
	
	return 0x00;
}

UINT8 DROPlayer::Seek(UINT8 unit, UINT32 pos)
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

UINT8 DROPlayer::SeekToTick(UINT32 tick)
{
	_playState |= PLAYSTATE_SEEK;
	if (tick > _playTick)
		ParseFile(tick - _playTick);
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	return 0x00;
}

UINT8 DROPlayer::SeekToFilePos(UINT32 pos)
{
	_playState |= PLAYSTATE_SEEK;
	if (_fileHdr.verMajor < 2)
	{
		while(_filePos <= pos && ! (_playState & PLAYSTATE_END))
			DoCommand_v1();
	}
	else
	{
		while(_filePos <= pos && ! (_playState & PLAYSTATE_END))
			DoCommand_v2();
	}
	_playTick = _fileTick;
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	
	return 0x00;
}

UINT32 DROPlayer::Render(UINT32 smplCnt, WAVE_32BS* data)
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
			DRO_CHIPDEV* cDev = &_devices[curDev];
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

void DROPlayer::ParseFile(UINT32 ticks)
{
	_playTick += ticks;
	if (_playState & PLAYSTATE_END)
		return;
	
	if (_fileHdr.verMajor < 2)
	{
		while(_fileTick <= _playTick && ! (_playState & PLAYSTATE_END))
			DoCommand_v1();
	}
	else
	{
		while(_fileTick <= _playTick && ! (_playState & PLAYSTATE_END))
			DoCommand_v2();
	}
	
	return;
}

void DROPlayer::DoCommand_v1(void)
{
	if (_filePos >= DataLoader_GetSize(_dLoad))
	{
		DoFileEnd();
		return;
	}
	
	UINT8 curCmd;
	
	//emu_logf(&_logger, PLRLOG_TRACE, "[DRO v1] Ofs %04X, Command %02X data %02X\n", _filePos, _fileData[_filePos], _fileData[_filePos+1]);
	curCmd = _fileData[_filePos];
	_filePos ++;
	switch(curCmd)
	{
	case 0x00:	// 1-byte delay
		_fileTick += 1 + _fileData[_filePos];
		_filePos ++;
		return;
	case 0x01:	// 2-byte delay
		// Note: With DRO v1, the DOSBox developers wanted to change this command from 0x01 to 0x10.
		//       Too bad that they updated the documentation, but not the actual code.
		if (_filePos < _initBlkEndOfs)
			break;	// assume missing escape command during initialization block
		if (! (_fileData[_filePos + 0x00] & ~0x20) &&
			(_fileData[_filePos + 0x01] == 0x08 || _fileData[_filePos + 0x01] >= 0x20))
			break;	// This is an unescaped register write. (e.g. 01 20 08 xx or 01 00 BD C0)
		
		_fileTick += 1 + ReadLE16(&_fileData[_filePos]);
		_filePos += 0x02;
		return;
	case 0x02:	// use 1st OPL2 chip / 1st OPL3 port
	case 0x03:	// use 2nd OPL2 chip / 2nd OPL3 port
		_selPort = curCmd & 0x01;
		if (_selPort >= (_devTypes.size() << _portShift))
		{
			//emu_logf(&_logger, PLRLOG_WARN, "More chips used than defined in header!\n");
			//_shownMsgs[2] = true;
		}
		return;
	case 0x04:	// escape command
		// Note: This command is used by various tools that edit DRO files, but DOSBox itself doesn't write it.
		if (_fileData[_filePos] >= 0x08)
			break;	// It only makes sense to escape register 00..04, so should be a direct write to register 04.
		if (_filePos < _initBlkEndOfs)
			break;	// assume missing escape command during initialization block
		
		// read the next value and treat it as register value
		curCmd = _fileData[_filePos];
		_filePos ++;
		break;
	}
	
	WriteReg(_selPort, curCmd, _fileData[_filePos]);
	_filePos ++;
	
	return;
}

void DROPlayer::DoCommand_v2(void)
{
	if (_filePos >= DataLoader_GetSize(_dLoad))
	{
		DoFileEnd();
		return;
	}
	
	UINT8 port;
	UINT8 reg;
	UINT8 data;
	
	//emu_logf(&_logger, PLRLOG_TRACE, "[DRO v2] Ofs %04X, Command %02X data %02X\n", _filePos, _fileData[_filePos], _fileData[_filePos+1]);
	reg = _fileData[_filePos + 0x00];
	data = _fileData[_filePos + 0x01];
	_filePos += 0x02;
	if (reg == _fileHdr.cmdDlyShort)
	{
		_fileTick += (1 + data);
		return;
	}
	else if (reg == _fileHdr.cmdDlyLong)
	{
		_fileTick += (1 + data) << 8;
		return;
	}
	port = (reg & 0x80) >> 7;
	reg &= 0x7F;
	if (reg >= _fileHdr.regCmdCnt)
		return;	// invalid register
	
	reg = _fileHdr.regCmdMap[reg];
	WriteReg(port, reg, data);
	
	return;
}

void DROPlayer::DoFileEnd(void)
{
	if (_playState & PLAYSTATE_SEEK)	// recalculate playSmpl to fix state when triggering callbacks
		_playSmpl = Tick2Sample(_fileTick);	// Note: fileTick results in more accurate position
	_playState |= PLAYSTATE_END;
	_psTrigger |= PLAYSTATE_END;
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
	
	return;
}

void DROPlayer::WriteReg(UINT8 port, UINT8 reg, UINT8 data)
{
	UINT8 devID = port >> _portShift;
	
	if (devID >= _devices.size())
		return;
	DRO_CHIPDEV* cDev = &_devices[devID];
	DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
	if (dataPtr == NULL || cDev->write == NULL)
		return;
	
	port &= _portMask;
	cDev->write(dataPtr, (port << 1) | 0, reg);
	cDev->write(dataPtr, (port << 1) | 1, data);
	
	return;
}
