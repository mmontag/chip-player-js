#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <zlib.h>

#define INLINE	static inline

#include "../common_def.h"
#include "gymplayer.hpp"
#include "../emu/EmuStructs.h"
#include "../emu/SoundEmu.h"
#include "../emu/Resampler.h"
#include "../emu/SoundDevs.h"
#include "../emu/EmuCores.h"
#include "../emu/cores/sn764intf.h"	// for SN76496_CFG
#include "../utils/StrUtils.h"
#include "helper.h"
#include "../emu/logging.h"


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

GYMPlayer::GYMPlayer() :
	_tickFreq(60),
	_filePos(0),
	_fileTick(0),
	_playTick(0),
	_playSmpl(0),
	_curLoop(0),
	_playState(0x00),
	_psTrigger(0x00)
{
	size_t curDev;
	UINT8 retVal;
	
	dev_logger_set(&_logger, this, GYMPlayer::PlayerLogCB, NULL);

	_playOpts.genOpts.pbSpeed = 0x10000;

	_lastTsMult = 0;
	_lastTsDiv = 0;
	
	for (curDev = 0; curDev < 2; curDev ++)
		InitDeviceOptions(_devOpts[curDev]);
	GenerateDeviceConfig();
	
	retVal = CPConv_Init(&_cpc1252, "CP1252", "UTF-8");
	if (retVal)
		_cpc1252 = NULL;
	_tagList.reserve(16);
	_tagList.push_back(NULL);
}

GYMPlayer::~GYMPlayer()
{
	_eventCbFunc = NULL;	// prevent any callbacks during destruction
	
	if (_playState & PLAYSTATE_PLAY)
		Stop();
	UnloadFile();
	
	if (_cpc1252 != NULL)
		CPConv_Deinit(_cpc1252);
}

UINT32 GYMPlayer::GetPlayerType(void) const
{
	return FCC_GYM;
}

const char* GYMPlayer::GetPlayerName(void) const
{
	return "GYM";
}

/*static*/ UINT8 GYMPlayer::PlayerCanLoadFile(DATA_LOADER *dataLoader)
{
	DataLoader_ReadUntil(dataLoader,0x04);
	if (DataLoader_GetSize(dataLoader) < 0x04)
		return 0xF1;	// file too small
	if (! memcmp(&DataLoader_GetData(dataLoader)[0x00], "GYMX", 4))
		return 0x00;	// valid GYMX header
	if (DataLoader_GetData(dataLoader)[0x00] <= 0x03)	// check for a valid command byte
		return 0x00;
	return 0xF0;	// invalid file
}

UINT8 GYMPlayer::CanLoadFile(DATA_LOADER *dataLoader) const
{
	return this->PlayerCanLoadFile(dataLoader);
}

UINT8 GYMPlayer::LoadFile(DATA_LOADER *dataLoader)
{
	_dLoad = NULL;
	DataLoader_ReadUntil(dataLoader,0x1AC);	// try to read the full GYMX header
	_fileData = DataLoader_GetData(dataLoader);
	if (DataLoader_GetSize(dataLoader) < 0x04)
		return 0xF0;	// invalid file
	
	_fileHdr.hasHeader = ! memcmp(&_fileData[0x00], "GYMX", 4);
	_decFData.clear();
	if (! _fileHdr.hasHeader)
	{
		_fileHdr.uncomprSize = 0;
		_fileHdr.loopFrame = 0;
		_fileHdr.dataOfs = 0x00;
	}
	else
	{
		if (DataLoader_GetSize(dataLoader) < 0x1AC)
			return 0xF1;	// file too small
		_fileHdr.loopFrame = ReadLE32(&_fileData[0x1A4]);
		_fileHdr.uncomprSize = ReadLE32(&_fileData[0x1A8]);
		_fileHdr.dataOfs = 0x1AC;
	}
	
	_dLoad = dataLoader;
	DataLoader_ReadAll(_dLoad);
	_fileData = DataLoader_GetData(_dLoad);
	_fileLen = DataLoader_GetSize(_dLoad);
	
	LoadTags();
	
	if (_fileHdr.uncomprSize > 0)
	{
		UINT8 retVal = DecompressZlibData();
		if (retVal & 0x80)
			return 0xFF;	// decompression error
	}
	_fileHdr.realFileSize = _fileLen;
	
	CalcSongLength();
	
	return 0x00;
}

UINT8 GYMPlayer::DecompressZlibData(void)
{
	z_stream zStream;
	int ret;
	
	_decFData.resize(_fileHdr.dataOfs + _fileHdr.uncomprSize);
	memcpy(&_decFData[0], _fileData, _fileHdr.dataOfs);	// copy file header
	
	zStream.zalloc = Z_NULL;
	zStream.zfree = Z_NULL;
	zStream.opaque = Z_NULL;
	zStream.avail_in = DataLoader_GetSize(_dLoad) - _fileHdr.dataOfs;
	zStream.next_in = (z_const Bytef*)&_fileData[_fileHdr.dataOfs];
	ret = inflateInit2(&zStream, 0x20 | 15);
	if (ret != Z_OK)
		return 0xFF;
	zStream.avail_out = (uInt)(_decFData.size() - _fileHdr.dataOfs);
	zStream.next_out = (Bytef*)&_decFData[_fileHdr.dataOfs];
	
	ret = inflate(&zStream, Z_SYNC_FLUSH);
	if (! (ret == Z_OK || ret == Z_STREAM_END))
	{
		emu_logf(&_logger, PLRLOG_ERROR, "GYM decompression error %d after decompressing %lu bytes.\n",
			ret, zStream.total_out);
	}
	_decFData.resize(_fileHdr.dataOfs + zStream.total_out);
	
	inflateEnd(&zStream);
	
	_fileData = &_decFData[0];
	_fileLen = (UINT32)_decFData.size();
	return (ret == Z_OK || ret == Z_STREAM_END) ? 0x00 : 0x01;
}

void GYMPlayer::CalcSongLength(void)
{
	UINT32 filePos;
	bool fileEnd;
	UINT8 curCmd;
	
	_totalTicks = 0;
	_loopOfs = 0;
	
	fileEnd = false;
	filePos = _fileHdr.dataOfs;
	while(! fileEnd && filePos < _fileLen)
	{
		if (_totalTicks == _fileHdr.loopFrame && _fileHdr.loopFrame != 0)
			_loopOfs = filePos;
		
		curCmd = _fileData[filePos];
		filePos ++;
		switch(curCmd)
		{
		case 0x00:	// wait 1 frame
			_totalTicks ++;
			break;
		case 0x01:
		case 0x02:
			filePos += 0x02;
			break;
		case 0x03:
			filePos += 0x01;
			break;
		default:
			fileEnd = true;
			break;
		}
	}
	
	return;
}

UINT8 GYMPlayer::LoadTags(void)
{
	_tagData.clear();
	_tagList.clear();
	if (! _fileHdr.hasHeader)
	{
		_tagList.push_back(NULL);
		return 0x00;
	}
	
	LoadTag("TITLE",        &_fileData[0x04], 0x20);
	LoadTag("GAME",         &_fileData[0x24], 0x20);
	// no "ARTIST" tag in GYMX files
	LoadTag("PUBLISHER",    &_fileData[0x44], 0x20);
	LoadTag("EMULATOR",     &_fileData[0x64], 0x20);
	LoadTag("ENCODED_BY",   &_fileData[0x84], 0x20);
	LoadTag("COMMENT",      &_fileData[0xA4], 0x100);
	
	_tagList.push_back(NULL);
	return 0x00;
}

void GYMPlayer::LoadTag(const char* tagName, const void* data, size_t maxlen)
{
	const char* startPtr = (const char*)data;
	const char* endPtr = (const char*)memchr(startPtr, '\0', maxlen);
	if (endPtr == NULL)
		endPtr = startPtr + maxlen;
	
	_tagData[tagName] = GetUTF8String(startPtr, endPtr);
	
	std::map<std::string, std::string>::const_iterator mapIt = _tagData.find(tagName);
	_tagList.push_back(mapIt->first.c_str());
	_tagList.push_back(mapIt->second.c_str());
	return;
}

std::string GYMPlayer::GetUTF8String(const char* startPtr, const char* endPtr)
{
	if (startPtr == endPtr)
		return std::string();
	
	if (_cpc1252 != NULL)
	{
		size_t convSize = 0;
		char* convData = NULL;
		std::string result;
		UINT8 retVal;
		
		retVal = CPConv_StrConvert(_cpc1252, &convSize, &convData, endPtr - startPtr, startPtr);
		
		result.assign(convData, convData + convSize);
		free(convData);
		if (retVal < 0x80)
			return result;
	}
	// unable to convert - fallback using the original string
	return std::string(startPtr, endPtr);
}

UINT8 GYMPlayer::UnloadFile(void)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0xFF;
	
	_playState = 0x00;
	_dLoad = NULL;
	_fileData = NULL;
	_decFData = std::vector<UINT8>();	// free allocated memory
	_fileHdr.hasHeader = 0;
	_fileHdr.dataOfs = 0x00;
	_devices.clear();
	
	return 0x00;
}

const GYM_HEADER* GYMPlayer::GetFileHeader(void) const
{
	return &_fileHdr;
}

const char* const* GYMPlayer::GetTags(void)
{
	return &_tagList[0];
}

UINT8 GYMPlayer::GetSongInfo(PLR_SONG_INFO& songInf)
{
	if (_dLoad == NULL)
		return 0xFF;
	
	songInf.format = FCC_GYM;
	songInf.fileVerMaj = 0;
	songInf.fileVerMin = 0;
	songInf.tickRateMul = 1;
	songInf.tickRateDiv = _tickFreq;
	songInf.songLen = GetTotalTicks();
	songInf.loopTick = _loopOfs ? GetLoopTicks() : (UINT32)-1;
	songInf.volGain = 0x10000;
	songInf.deviceCnt = (UINT32)_devCfgs.size();
	
	return 0x00;
}

UINT8 GYMPlayer::GetSongDeviceInfo(std::vector<PLR_DEV_INFO>& devInfList) const
{
	if (_dLoad == NULL)
		return 0xFF;
	
	size_t curDev;
	
	devInfList.clear();
	devInfList.reserve(_devCfgs.size());
	for (curDev = 0; curDev < _devCfgs.size(); curDev ++)
	{
		const DEV_GEN_CFG* devCfg = reinterpret_cast<const DEV_GEN_CFG*>(&_devCfgs[curDev].data[0]);
		PLR_DEV_INFO devInf;
		memset(&devInf, 0x00, sizeof(PLR_DEV_INFO));
		
		devInf.id = (UINT32)curDev;
		devInf.type = _devCfgs[curDev].type;
		devInf.instance = 0;
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
			devInf.volume = _devCfgs[curDev].volume;
			devInf.smplRate = 0;
		}
		devInfList.push_back(devInf);
	}
	if (! _devices.empty())
		return 0x01;	// returned "live" data
	else
		return 0x00;	// returned data based on file header
}

size_t GYMPlayer::DeviceID2OptionID(UINT32 id) const
{
	UINT8 type;
	UINT8 instance;
	
	if (id & 0x80000000)
	{
		type = (id >> 0) & 0xFF;
		instance = (id >> 16) & 0xFF;
	}
	else if (id < _devCfgs.size())
	{
		type = _devCfgs[id].type;
		instance = 0;
	}
	else
	{
		return (size_t)-1;
	}
	
	if (instance == 0)
	{
		if (type == DEVID_YM2612)
			return 0;
		else if (type == DEVID_SN76496)
			return 1;
	}
	return (size_t)-1;
}

void GYMPlayer::RefreshMuting(GYM_CHIPDEV& chipDev, const PLR_MUTE_OPTS& muteOpts)
{
	DEV_INFO* devInf = &chipDev.base.defInf;
	if (devInf->dataPtr != NULL && devInf->devDef->SetMuteMask != NULL)
		devInf->devDef->SetMuteMask(devInf->dataPtr, muteOpts.chnMute[0]);
	
	return;
}

void GYMPlayer::RefreshPanning(GYM_CHIPDEV& chipDev, const PLR_PAN_OPTS& panOpts)
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

UINT8 GYMPlayer::SetDeviceOptions(UINT32 id, const PLR_DEV_OPTS& devOpts)
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	_devOpts[optID] = devOpts;
	
	size_t devID = _optDevMap[optID];
	if (devID < _devices.size())
		RefreshMuting(_devices[devID], _devOpts[optID].muteOpts);
	return 0x00;
}

UINT8 GYMPlayer::GetDeviceOptions(UINT32 id, PLR_DEV_OPTS& devOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	devOpts = _devOpts[optID];
	return 0x00;
}

UINT8 GYMPlayer::SetDeviceMuting(UINT32 id, const PLR_MUTE_OPTS& muteOpts)
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

UINT8 GYMPlayer::GetDeviceMuting(UINT32 id, PLR_MUTE_OPTS& muteOpts) const
{
	size_t optID = DeviceID2OptionID(id);
	if (optID == (size_t)-1)
		return 0x80;	// bad device ID
	
	muteOpts = _devOpts[optID].muteOpts;
	return 0x00;
}

UINT8 GYMPlayer::SetPlayerOptions(const GYM_PLAY_OPTIONS& playOpts)
{
	_playOpts = playOpts;
	return 0x00;
}

UINT8 GYMPlayer::GetPlayerOptions(GYM_PLAY_OPTIONS& playOpts) const
{
	playOpts = _playOpts;
	return 0x00;
}

UINT8 GYMPlayer::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0x01;	// can't set during playback
	
	_outSmplRate = sampleRate;
	return 0x00;
}

UINT8 GYMPlayer::SetPlaybackSpeed(double speed)
{
	_playOpts.genOpts.pbSpeed = (UINT32)(0x10000 * speed);
	RefreshTSRates();
	return 0x00;
}


void GYMPlayer::RefreshTSRates(void)
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

UINT32 GYMPlayer::Tick2Sample(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1;
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 GYMPlayer::Sample2Tick(UINT32 samples) const
{
	if (samples == (UINT32)-1)
		return -1;
	return (UINT32)(samples * _tsDiv / _tsMult);
}

double GYMPlayer::Tick2Second(UINT32 ticks) const
{
	if (ticks == (UINT32)-1)
		return -1.0;
	return ticks / (double)_tickFreq;
}

UINT8 GYMPlayer::GetState(void) const
{
	return _playState;
}

UINT32 GYMPlayer::GetCurPos(UINT8 unit) const
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

UINT32 GYMPlayer::GetCurLoop(void) const
{
	return _curLoop;
}

UINT32 GYMPlayer::GetTotalTicks(void) const
{
	return _totalTicks;
}

UINT32 GYMPlayer::GetLoopTicks(void) const
{
	if (! _loopOfs)
		return 0;
	else
		return _totalTicks - _fileHdr.loopFrame;
}

/*static*/ void GYMPlayer::PlayerLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	GYMPlayer* player = (GYMPlayer*)source;
	if (player->_logCbFunc == NULL)
		return;
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_PLR, NULL, message);
	return;
}

/*static*/ void GYMPlayer::SndEmuLogCB(void* userParam, void* source, UINT8 level, const char* message)
{
	DEVLOG_CB_DATA* cbData = (DEVLOG_CB_DATA*)userParam;
	GYMPlayer* player = cbData->player;
	if (player->_logCbFunc == NULL)
		return;
	if ((player->_playState & PLAYSTATE_SEEK) && level > PLRLOG_ERROR)
		return;	// prevent message spam while seeking
	player->_logCbFunc(player->_logCbParam, player, level, PLRLOGSRC_EMU,
		player->_devNames[cbData->chipDevID].c_str(), message);
	return;
}


void GYMPlayer::GenerateDeviceConfig(void)
{
	_devCfgs.clear();
	_devNames.clear();
	_devCfgs.resize(2);
	{
		DEV_GEN_CFG devCfg;
		memset(&devCfg, 0x00, sizeof(DEV_GEN_CFG));
		devCfg.clock = 7670453;	// YMAMP clock: 7670442
		_devCfgs[0].type = DEVID_YM2612;
		_devCfgs[0].volume = 0x100;
		SaveDeviceConfig(_devCfgs[0].data, &devCfg, sizeof(DEV_GEN_CFG));
		_devNames.push_back("FM");
	}
	{
		SN76496_CFG snCfg;
		memset(&snCfg, 0x00, sizeof(SN76496_CFG));
		snCfg._genCfg.clock = 3579545;	// YMAMP clock: 3579580
		snCfg.shiftRegWidth = 0x10;
		snCfg.noiseTaps = 0x09;
		snCfg.segaPSG = 1;
		snCfg.negate = 0;
		snCfg.stereo = 1;
		snCfg.clkDiv = 8;
		snCfg.t6w28_tone = NULL;
		_devCfgs[1].type = DEVID_SN76496;
		_devCfgs[1].volume = 0x80;
		SaveDeviceConfig(_devCfgs[1].data, &snCfg, sizeof(SN76496_CFG));
		_devNames.push_back("PSG");
	}
	
	return;
}

UINT8 GYMPlayer::Start(void)
{
	size_t curDev;
	UINT8 retVal;
	
	for (curDev = 0; curDev < 2; curDev ++)
		_optDevMap[curDev] = (size_t)-1;
	
	_devices.clear();
	_devices.resize(_devCfgs.size());
	for (curDev = 0; curDev < _devCfgs.size(); curDev ++)
	{
		GYM_CHIPDEV* cDev = &_devices[curDev];
		PLR_DEV_OPTS* devOpts;
		DEV_GEN_CFG* devCfg = reinterpret_cast<DEV_GEN_CFG*>(&_devCfgs[curDev].data[0]);
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
		
		retVal = SndEmu_Start(_devCfgs[curDev].type, devCfg, &cDev->base.defInf);
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
			cDev->base.defInf.devDef->SetLogCB(cDev->base.defInf.dataPtr, GYMPlayer::SndEmuLogCB, &cDev->logCbData);
		SetupLinkedDevices(&cDev->base, NULL, NULL);
		
		if (devOpts != NULL)
		{
			if (cDev->base.defInf.devDef->SetOptionBits != NULL)
				cDev->base.defInf.devDef->SetOptionBits(cDev->base.defInf.dataPtr, devOpts->coreOpts);
			
			_optDevMap[cDev->optID] = curDev;
		}
		
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			Resmpl_SetVals(&clDev->resmpl, 0xFF, _devCfgs[curDev].volume, _outSmplRate);
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

UINT8 GYMPlayer::Stop(void)
{
	size_t curDev;
	
	_playState &= ~PLAYSTATE_PLAY;
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		GYM_CHIPDEV* cDev = &_devices[curDev];
		FreeDeviceTree(&cDev->base, 0);
	}
	_devices.clear();
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_STOP, NULL);
	
	return 0x00;
}

UINT8 GYMPlayer::Reset(void)
{
	size_t curDev;
	
	_filePos = _fileHdr.dataOfs;
	_fileTick = 0;
	_playTick = 0;
	_playSmpl = 0;
	_playState &= ~PLAYSTATE_END;
	_psTrigger = 0x00;
	_curLoop = 0;
	_lastLoopTick = 0;
	
	_pcmBuffer.resize(_outSmplRate / 30);	// that's the buffer size in YMAMP
	_pcmBaseTick = (UINT32)-1;
	_pcmInPos = 0x00;
	_pcmOutPos = (UINT32)-1;

	RefreshTSRates();	

	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		GYM_CHIPDEV* cDev = &_devices[curDev];
		VGM_BASEDEV* clDev;
		if (cDev->base.defInf.dataPtr == NULL)
			continue;
		
		cDev->base.defInf.devDef->Reset(cDev->base.defInf.dataPtr);
		for (clDev = &cDev->base; clDev != NULL; clDev = clDev->linkDev)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
		}
	}
	
	return 0x00;
}

UINT8 GYMPlayer::Seek(UINT8 unit, UINT32 pos)
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

UINT8 GYMPlayer::SeekToTick(UINT32 tick)
{
	_playState |= PLAYSTATE_SEEK;
	if (tick > _playTick)
		ParseFile(tick - _playTick);
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	return 0x00;
}

UINT8 GYMPlayer::SeekToFilePos(UINT32 pos)
{
	_playState |= PLAYSTATE_SEEK;
	while(_filePos <= pos && ! (_playState & PLAYSTATE_END))
		DoCommand();
	_playTick = _fileTick;
	_playSmpl = Tick2Sample(_playTick);
	_playState &= ~PLAYSTATE_SEEK;
	
	return 0x00;
}

UINT32 GYMPlayer::Render(UINT32 smplCnt, WAVE_32BS* data)
{
	UINT32 curSmpl;
	UINT32 smplFileTick;
	UINT32 maxSmpl;
	INT32 smplStep;	// might be negative due to rounding errors in Tick2Sample
	size_t curDev;
	UINT32 pcmLastBase = (UINT32)-1;
	UINT32 pcmSmplStart = 0;
	UINT32 pcmSmplLen = 1;
	
	// Note: use do {} while(), so that "smplCnt == 0" can be used to process until reaching the next sample.
	curSmpl = 0;
	do
	{
		smplFileTick = Sample2Tick(_playSmpl);
		ParseFile(smplFileTick - _playTick);
		
		// render as many samples at once as possible (for better performance)
		maxSmpl = Tick2Sample(_fileTick);
		smplStep = maxSmpl - _playSmpl;
		// reduce sample steps to 1 when PCM is active
		if (smplStep < 1 || _pcmInPos > 0)
			smplStep = 1;	// must render at least 1 sample in order to advance
		if ((UINT32)smplStep > smplCnt - curSmpl)
			smplStep = smplCnt - curSmpl;
		
		if (_pcmInPos > 0)
		{
			// PCM buffer handling
			// Stream all buffered writes to the YM2612, evenly distributed over the current frame.
			if (pcmLastBase != _pcmBaseTick)
			{
				pcmLastBase = _pcmBaseTick;
				pcmSmplStart = Tick2Sample(_pcmBaseTick);
				pcmSmplLen = Tick2Sample(_pcmBaseTick + 1) - pcmSmplStart;
			}
			UINT32 pcmIdx = (_playSmpl - pcmSmplStart) * _pcmInPos / pcmSmplLen;
			if (pcmIdx != _pcmOutPos)
			{
				GYM_CHIPDEV* cDev = &_devices[0];
				DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
				_pcmOutPos = pcmIdx;
				if (! (dataPtr == NULL || cDev->write == NULL) && _pcmOutPos < _pcmInPos)
				{
					cDev->write(dataPtr, 0, 0x2A);
					cDev->write(dataPtr, 1, _pcmBuffer[pcmIdx]);
				}
				if (_pcmOutPos == _pcmInPos - 1)
					_pcmInPos = 0;	// reached the end of the buffer - disable further PCM streaming
			}
		}
		
		for (curDev = 0; curDev < _devices.size(); curDev ++)
		{
			GYM_CHIPDEV* cDev = &_devices[curDev];
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

void GYMPlayer::ParseFile(UINT32 ticks)
{
	_playTick += ticks;
	if (_playState & PLAYSTATE_END)
		return;
	
	while(_fileTick <= _playTick && ! (_playState & PLAYSTATE_END))
		DoCommand();
	
	return;
}

void GYMPlayer::DoCommand(void)
{
	if (_filePos >= _fileLen)
	{
		DoFileEnd();
		return;
	}
	
	UINT8 curCmd;
	
	curCmd = _fileData[_filePos];
	_filePos ++;
	switch(curCmd)
	{
	case 0x00:	// wait 1 frame
		_fileTick ++;
		return;
	case 0x01:	// write to YM2612 port 0
	case 0x02:	// write to YM2612 port 1
		{
			UINT8 port = curCmd - 0x01;
			UINT8 reg = _fileData[_filePos + 0x00];
			UINT8 data = _fileData[_filePos + 0x01];
			_filePos += 0x02;
			
			if (port == 0 && reg == 0x2A)
			{
				if (_playState & PLAYSTATE_SEEK)
					return;	// ignore PCM buffer while seeking
				// special handling for PCM writes: put them into a separate buffer
				if (_pcmBaseTick != _fileTick)
				{
					_pcmBaseTick = _fileTick;
					_pcmInPos = 0;
					_pcmOutPos = (UINT32)-1;
				}
				if (_pcmInPos < _pcmBuffer.size())
				{
					_pcmBuffer[_pcmInPos] = data;
					_pcmInPos ++;
				}
				return;
			}
			
			GYM_CHIPDEV* cDev = &_devices[0];
			DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
			if (dataPtr == NULL || cDev->write == NULL)
				return;
			
			if ((reg & 0xF0) == 0xA0)
			{
				// Note: The OPN series has a particular behaviour with frequency registers (Ax) that
				// wasn't known when GYM files were created.
				// In short: reg A4..A6 must be followed by reg A0..A2 of the same channel to have the intended effect.
				// (same for reg AC..AE and A8..AA)
				//
				// Thus we apply the following patches, in order to be able to play back GYMs without glitches:
				// - "reg A4" only -> send "reg A4", then "reg A0" (with data from cache)
				// - "reg A0" only -> send "reg A4" (with data from cache), then "reg A0"
				UINT8 cacheReg = (port << 4) | (reg & 0x0F);
				UINT8 isLatch = (reg & 0x04);
				UINT8 latchID = (reg & 0x08) >> 3;
				_ymFreqRegs[cacheReg] = data;
				
				if (isLatch)
				{
					bool needPatch = true;
					if (_filePos + 0x01 < _fileLen &&
						_fileData[_filePos + 0x00] == curCmd && _fileData[_filePos + 0x01] == (reg ^ 0x04))
						needPatch = false;	// the next write is the 2nd part - no patch needed
					
					cDev->write(dataPtr, (port << 1) | 0, reg);
					cDev->write(dataPtr, (port << 1) | 1, data);
					_ymLatch[latchID] = data;
					if (needPatch)
					{
						//emu_logf(&_logger, PLRLOG_TRACE, "Fixing missing freq p2: %03X=%02X + [%03X=%02X]\n",
						//	(port << 8) | reg, data, (port << 8) | (reg ^ 0x04), _ymFreqRegs[cacheReg ^ 0x04]);
						// complete the 2-part write by sending the command for the 2nd part
						cDev->write(dataPtr, (port << 1) | 0, reg ^ 0x04);
						cDev->write(dataPtr, (port << 1) | 1, _ymFreqRegs[cacheReg ^ 0x04]);
					}
				}
				else
				{
					if (_ymLatch[latchID] != _ymFreqRegs[cacheReg ^ 0x04])
					{
						//emu_logf(&_logger, PLRLOG_TRACE, "Fixing missing freq p1: [%03X=%02X] + %03X=%02X\n",
						//	(port << 8) | (reg ^ 0x04), _ymFreqRegs[cacheReg ^ 0x04], (port << 8) | reg, data);
						// make sure to set the latch to the correct value
						cDev->write(dataPtr, (port << 1) | 0, reg ^ 0x04);
						cDev->write(dataPtr, (port << 1) | 1, _ymFreqRegs[cacheReg ^ 0x04]);
						_ymLatch[latchID] = _ymFreqRegs[cacheReg ^ 0x04];
					}
					cDev->write(dataPtr, (port << 1) | 0, reg);
					cDev->write(dataPtr, (port << 1) | 1, data);
				}
			}
			else
			{
				cDev->write(dataPtr, (port << 1) | 0, reg);
				cDev->write(dataPtr, (port << 1) | 1, data);
			}
		}
		return;
	case 0x03:	// write to PSG
		{
			UINT8 data = _fileData[_filePos + 0x00];
			_filePos += 0x01;
			
			GYM_CHIPDEV* cDev = &_devices[1];
			DEV_DATA* dataPtr = cDev->base.defInf.dataPtr;
			if (dataPtr == NULL || cDev->write == NULL)
				return;
			
			cDev->write(dataPtr, SN76496_W_REG, data);
		}
		return;
	}
	
	return;
}

void GYMPlayer::DoFileEnd(void)
{
	UINT8 doLoop = (_loopOfs != 0);
	
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
		_filePos = _loopOfs;
		return;
	}
	
	_playState |= PLAYSTATE_END;
	_psTrigger |= PLAYSTATE_END;
	if (_eventCbFunc != NULL)
		_eventCbFunc(this, _eventCbParam, PLREVT_END, NULL);
	
	return;
}
