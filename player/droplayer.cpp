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
#include "droplayer.hpp"
#include <emu/EmuStructs.h>
#include <emu/SoundEmu.h>
#include <emu/Resampler.h>
#include <emu/SoundDevs.h>
#include <emu/EmuCores.h>
#include <emu/cores/sn764intf.h>	// for SN76496_CFG
#include <emu/cores/ayintf.h>		// for AY8910_CFG
#include "helper.h"




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
	_curLoop(0),
	_playState(0x00),
	_psTrigger(0x00)
{
}

DROPlayer::~DROPlayer()
{
}

UINT32 DROPlayer::GetPlayerType(void) const
{
	return FCC_DRO;
}

const char* DROPlayer::GetPlayerName(void) const
{
	return "DRO";
}

UINT8 DROPlayer::LoadFile(const char* fileName)
{
	FILE* hFile;
	UINT8 fileSig[0x10];
	size_t readBytes;
	size_t fileSize;
	UINT32 tempLng;
	
	hFile = fopen(fileName, "rb");
	if (hFile == NULL)
		return 0xFF;
	
	fseek(hFile, 0, SEEK_END);
	fileSize = ftell(hFile);
	rewind(hFile);
	
	readBytes = fread(fileSig, 1, 0x10, hFile);
	if (readBytes < 0x10 || memcmp(fileSig, "DBRAWOPL", 8))
	{
		fclose(hFile);
		return 0xF0;	// invalid file
	}
	
	// --- try to detect the DRO version ---
	tempLng = ReadLE32(&fileSig[0x08]);
	if (tempLng & 0xFF00FF00)
	{
		// DOSBox 0.61 - DRO v0
		// This version didn't write version bytes.
		_fileHdr.verMajor = 0x00;
		_fileHdr.verMinor = 0x00;
	}
	else if (! (tempLng & 0x0000FFFF))
	{
		// DOSBox 0.63 - DRO v1
		// order is: minor, major
		_fileHdr.verMinor = ReadLE16(&fileSig[0x08]);
		_fileHdr.verMajor = ReadLE16(&fileSig[0x0A]);
	}
	else
	{
		// DOSBox 0.73 - DRO v2
		_fileHdr.verMajor = ReadLE16(&fileSig[0x08]);
		_fileHdr.verMinor = ReadLE16(&fileSig[0x0A]);
	}
	if (_fileHdr.verMajor > 2)
	{
		fclose(hFile);
		return 0xF1;	// unsupported version
	}
	
	// --- read the file data ---
	_fileData.resize(fileSize);
	memcpy(&_fileData[0], fileSig, 0x10);
	
	readBytes = 0x10 + fread(&_fileData[0x10], 1, fileSize - 0x10, hFile);
	if (readBytes < _fileData.size())
		_fileData.resize(readBytes);	// file was not completely read
	
	fclose(hFile);
	
	switch(_fileHdr.verMajor)
	{
	case 0:	// version 0 (DOSBox 0.61)
	case 1:	// version 1 (DOSBox 0.63)
		switch(_fileHdr.verMajor)
		{
		case 0:
			_fileHdr.lengthMS = ReadLE32(&_fileData[0x08]);
			_fileHdr.dataSize = ReadLE32(&_fileData[0x0C]);
			_fileHdr.hwType = _fileData[0x10];
			_dataOfs = 0x11;
			break;
		case 1:
			_fileHdr.lengthMS = ReadLE32(&_fileData[0x0C]);
			_fileHdr.dataSize = ReadLE32(&_fileData[0x10]);
			tempLng = ReadLE32(&_fileData[0x14]);
			_fileHdr.hwType = (tempLng <= 0xFF) ? (UINT8)tempLng : 0xFF;
			_dataOfs = 0x18;
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
		_dataOfs = 0x1A + _fileHdr.regCmdCnt;
		
		if (_fileHdr.regCmdCnt > 0x80)
			_fileHdr.regCmdCnt = 0x80;	// only 0x80 values are possible
		memcpy(_fileHdr.regCmdMap, &_fileData[0x1A], _fileHdr.regCmdCnt);
		
		break;
	}
	
	_realHwType = _fileHdr.hwType;
	if (true)
	{
		// DOSBox puts "DualOPL2" into the header of DROs that log OPL3 data ...
		// ... unless 4op mode is enabled.
		if (_realHwType == 1)
			_realHwType = 2;	// DualOPL2 -> OPL3
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
		_devTypes.push_back(DEVID_YMF262);	_devPanning.push_back(0x00);
		_portShift = 1;
		break;
	}
	_portMask = (1 << _portShift) - 1;
	
	_totalTicks = _fileHdr.lengthMS;
	_initBlkEndOfs = 0x20;
	
	return 0x00;
}

UINT8 DROPlayer::UnloadFile(void)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0xFF;
	
	_playState = 0x00;
	_fileData.clear();
	_fileHdr.verMajor = 0xFF;
	_dataOfs = 0x00;
	_devTypes.clear();
	_devPanning.clear();
	_devices.clear();
	
	return 0x00;
}

const DRO_HEADER* DROPlayer::GetFileHeader(void) const
{
	return &_fileHdr;
}

const char* DROPlayer::GetSongTitle(void)
{
	return NULL;
}

UINT8 DROPlayer::SetSampleRate(UINT32 sampleRate)
{
	if (_playState & PLAYSTATE_PLAY)
		return 0x01;	// can't set during playback
	
	_outSmplRate = sampleRate;
	return 0x00;
}

/*UINT8 DROPlayer::SetPlaybackSpeed(double speed)
{
	return 0xFF;	// not yet supported
}*/


UINT32 DROPlayer::Tick2Sample(UINT32 ticks) const
{
	return (UINT32)(ticks * _tsMult / _tsDiv);
}

UINT32 DROPlayer::Sample2Tick(UINT32 samples) const
{
	return (UINT32)(samples * _tsDiv / _tsMult);
}

double DROPlayer::Tick2Second(UINT32 ticks) const
{
	return ticks / (double)_tickFreq;
}

UINT8 DROPlayer::GetState(void) const
{
	return _playState;
}

UINT32 DROPlayer::GetCurFileOfs(void) const
{
	return _filePos;
}

UINT32 DROPlayer::GetCurTick(void) const
{
	return _playTick;
}

UINT32 DROPlayer::GetCurSample(void) const
{
	return _playSmpl;
}

UINT32 DROPlayer::GetTotalTicks(void) const
{
	return _totalTicks;
}

UINT32 DROPlayer::GetLoopTicks(void) const
{
	return 0;
}

UINT32 DROPlayer::GetCurrentLoop(void) const
{
	return _curLoop;
}


UINT8 DROPlayer::Start(void)
{
	size_t curDev;
	UINT8 retVal;
	
	_devices.clear();
	_devices.resize(_devTypes.size());
	for (curDev = 0; curDev < _devTypes.size(); curDev ++)
	{
		DRO_CHIPDEV* cDev = &_devices[curDev];
		DEV_GEN_CFG devCfg;
		VGM_BASEDEV* clDev;
		UINT8 deviceID;
		
		cDev->base.defInf.dataPtr = NULL;
		cDev->base.linkDev = NULL;
		devCfg.emuCore = 0x00;
		devCfg.srMode = DEVRI_SRMODE_NATIVE;
		devCfg.flags = 0x00;
		devCfg.clock = 3579545;
		devCfg.smplRate = _outSmplRate;
		
		deviceID = _devTypes[curDev];
		if (deviceID == DEVID_YMF262)
			devCfg.clock *= 4;	// OPL3 uses a 14 MHz clock
		retVal = SndEmu_Start(deviceID, &devCfg, &cDev->base.defInf);
		if (retVal)
		{
			cDev->base.defInf.dataPtr = NULL;
			cDev->base.defInf.devDef = NULL;
			continue;
		}
		SndEmu_GetDeviceFunc(cDev->base.defInf.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8, 0, (void**)&cDev->write);
		
		SetupLinkedDevices(&cDev->base, NULL, NULL);
		
		clDev = &cDev->base;
		while(clDev != NULL)
		{
			Resmpl_SetVals(&clDev->resmpl, 0xFF, 0x100, _outSmplRate);
			// do DualOPL2 hard panning by muting either the left or right speaker
			if (_devPanning[curDev] & 0x02)
				clDev->resmpl.volumeL = 0x00;
			if (_devPanning[curDev] & 0x01)
				clDev->resmpl.volumeR = 0x00;
			Resmpl_DevConnect(&clDev->resmpl, &clDev->defInf);
			Resmpl_Init(&clDev->resmpl);
			clDev = clDev->linkDev;
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
	
	_filePos = _dataOfs;
	_fileTick = 0;
	_playTick = 0;
	_playSmpl = 0;
	_playState &= ~PLAYSTATE_END;
	_psTrigger = 0x00;
	_selPort = 0;
	_curLoop = 0;
	
	_tsMult = _outSmplRate;
	_tsDiv = _tickFreq;
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		DRO_CHIPDEV* cDev = &_devices[curDev];
		VGM_BASEDEV* clDev;
		
		cDev->base.defInf.devDef->Reset(cDev->base.defInf.dataPtr);
		clDev = &cDev->base;
		while(clDev != NULL)
		{
			// TODO: Resmpl_Reset(&clDev->resmpl);
			clDev = clDev->linkDev;
		}
	}
	
	for (curDev = 0; curDev < _devices.size(); curDev ++)
	{
		for (curPort = 0; curPort <= _portMask; curPort ++)
		{
			devport = (curDev << _portShift) | curPort;
			for (curReg = 0xFF; curReg >= 0x20; curReg --)
				WriteReg(devport, curReg, 0x00);
		}
		devport = (curDev << _portShift);
		WriteReg(devport | 0, 0x08, 0x00);
		WriteReg(devport | 0, 0x01, 0x00);
		
		if (_devTypes[curDev] == DEVID_YMF262)
		{
			// enable OPL3 mode (DOSBox dumps the registers in the wrong order)
			WriteReg(devport | 1, 0x05, 0x01);
			WriteReg(devport | 1, 0x04, 0x00);	// disable 4op mode
		}
	}
	
	return 0x00;
}

UINT32 DROPlayer::Render(UINT32 smplCnt, WAVE_32BS* data)
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
			DRO_CHIPDEV* cDev = &_devices[curDev];
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
	if (_filePos >= _fileData.size())
	{
		DoFileEnd();
		return;
	}
	
	UINT8 curCmd;
	
	//printf("DRO v1: Ofs %04X, Command %02X data %02X\n", _filePos, _fileData[_filePos], _fileData[_filePos+1]);
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
		if (_fileData[_filePos + 0x01] == 0xBD)
			break;	// This is an unescaped register write. (sequence 01 xx BD xx)
		if (_fileData[_filePos + 0x00] == 0x20 && _fileData[_filePos + 0x01] == 0x08)
			break;	// This is an unescaped register write. (sequence 01 20 08 xx)
		if (_filePos < _initBlkEndOfs)
			break;	// assume missing escape command during initialization block
		
		_fileTick += 1 + ReadLE16(&_fileData[_filePos]);
		_filePos += 0x02;
		return;
	case 0x02:	// use 1st OPL2 chip / 1st OPL3 port
	case 0x03:	// use 2nd OPL2 chip / 2nd OPL3 port
		_selPort = curCmd & 0x01;
		if (_selPort >= (_devTypes.size() << _portShift))
		{
			//printf("More chips used than defined in header!\n");
			//_shownMsgs[2] = true;
		}
		return;
	case 0x04:	// escape command
		// Note: This command is used by various tools that edit DRO files, but DOSBox itself doesn't write it.
		if (_fileData[_filePos] >= 0x20)
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
	if (_filePos >= _fileData.size())
	{
		DoFileEnd();
		return;
	}
	
	UINT8 port;
	UINT8 reg;
	UINT8 data;
	
	//printf("DRO v2: Ofs %04X, Command %02X data %02X\n", _filePos, _fileData[_filePos], _fileData[_filePos+1]);
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
	
	port &= _portMask;
	cDev->write(dataPtr, (port << 1) | 0, reg);
	cDev->write(dataPtr, (port << 1) | 1, data);
	
	return;
}
