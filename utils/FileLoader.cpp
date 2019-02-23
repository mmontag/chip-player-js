#include <stdio.h>
#include <zlib.h>
#include <stdtype.h>
#include "FileLoader.hpp"

union LoaderHandles
{
	const void* dataPtr;
	FILE* hFileRaw;
	gzFile hFileGZ;
};

FileLoader::FileLoader() :
	_readStopOfs((UINT32)-1),
	_status(FLSTAT_EMPTY)
{
	_hLoad = new LoaderHandles;
	
	return;
}

FileLoader::~FileLoader()
{
	FreeData();
	delete _hLoad;
	
	return;
}

std::vector<UINT8>& FileLoader::GetFileData(void)
{
	return _data;
}

UINT32 FileLoader::GetTotalFileSize() const
{
	return _bytesTotal;
}

UINT32 FileLoader::GetFileSize() const
{
	return _bytesLoaded;
}

UINT8 FileLoader::GetMode() const
{
	return _modeSrc | _modeCompr;
}

UINT8 FileLoader::GetStatus() const
{
	return _status;
}

UINT8 FileLoader::LoadData(UINT32 dataSize, const void* data)
{
	CancelLoading();
	
	_modeSrc = FLMODE_SRC_MEMORY;
	_modeCompr = FLMODE_CMP_RAW;
	
	_hLoad->dataPtr = data;
	_bytesTotal = dataSize;
	_data.clear();
	_bytesLoaded = 0x00;
	_status = FLSTAT_LOADING;
	
	if (_readStopOfs > 0)
		ReadData(_readStopOfs);
	
	return 0x00;
}

UINT8 FileLoader::LoadFile(const std::string& fileName)
{
	return LoadFile(fileName.c_str());
}

UINT8 FileLoader::LoadFile(const char* fileName)
{
	if (_status == FLSTAT_LOADING)
		return 0x01;
	
	UINT8 fileHdr[4];
	size_t readBytes;
	
	FreeData();
	
	_hLoad->hFileRaw = fopen(fileName, "rb");
	if (_hLoad->hFileRaw == NULL)
		return 0xFF;
	
	readBytes = fread(fileHdr, 0x01, 4, _hLoad->hFileRaw);
	if (readBytes < 4)
	{
		fclose(_hLoad->hFileRaw);
		return 0xFF;
	}
	_modeSrc = FLMODE_SRC_FILE;
	
	if (fileHdr[0] == 31 && fileHdr[1] == 139)
	{
		fseek(_hLoad->hFileRaw, -4, SEEK_END);
		fread(&_bytesTotal, 0x04, 0x01, _hLoad->hFileRaw);
		if (_bytesTotal < ftell(_hLoad->hFileRaw) / 2)
			_bytesTotal = (UINT32)-1;
		fclose(_hLoad->hFileRaw);	_hLoad->hFileRaw = NULL;
		
		_hLoad->hFileGZ = gzopen(fileName, "rb");
		if (_hLoad->hFileGZ == NULL)
			return 0xFF;
		_modeCompr = FLMODE_CMP_GZ;
	}
	else
	{
		fseek(_hLoad->hFileRaw, 0, SEEK_END);
		_bytesTotal = ftell(_hLoad->hFileRaw);
		rewind(_hLoad->hFileRaw);
		_modeCompr = FLMODE_CMP_RAW;
	}
	_data.clear();
	_bytesLoaded = 0x00;
	_status = FLSTAT_LOADING;
	
	if (_readStopOfs > 0)
		ReadData(_readStopOfs);
	
	return 0x00;
}

UINT8 FileLoader::CancelLoading(void)
{
	if (_status != FLSTAT_LOADING)
		return 0x01;
	
	switch(_modeSrc)
	{
	case FLMODE_SRC_MEMORY:
		if (_hLoad->dataPtr == NULL)
			return 0x01;
		_hLoad->dataPtr = NULL;
		break;
	case FLMODE_SRC_FILE:
		switch(_modeCompr)
		{
		case FLMODE_CMP_RAW:
			if (_hLoad->hFileRaw == NULL)
				return 0x01;
			fclose(_hLoad->hFileRaw);
			_hLoad->hFileRaw = NULL;
			break;
		case FLMODE_CMP_GZ:
			if (_hLoad->hFileGZ == NULL)
				return 0x01;
			gzclose(_hLoad->hFileGZ);
			_hLoad->hFileGZ = NULL;
			break;
		}
		break;
	}
	_status = FLSTAT_LOADED;
	
	return 0x00;
}

UINT8 FileLoader::FreeData(void)
{
	if (_status == FLSTAT_EMPTY)
		return 0x01;
	
	CancelLoading();
	_data.swap(std::vector<UINT8>());	// release all memory (.clear() doesn't always do that)
	_status = FLSTAT_EMPTY;
	
	return 0x00;
}

void FileLoader::SetPreloadBytes(UINT32 byteCount)
{
	_readStopOfs = byteCount;
	return;
}

void FileLoader::ReadUntil(UINT32 fileOffset)
{
	if (fileOffset > _bytesLoaded)
		ReadData(fileOffset);
	return;
}

void FileLoader::ReadFullFile(void)
{
	ReadData((UINT32)-1);
	return;
}

void FileLoader::ReadData(UINT32 numBytes)
{
	if (_status != FLSTAT_LOADING)
		return;
	
	UINT32 endOfs;
	
	endOfs = _bytesLoaded + numBytes;
	if (endOfs < _bytesLoaded)
		endOfs = (UINT32)-1;
	if (endOfs > _bytesTotal)
		endOfs = _bytesTotal;
	
	numBytes = endOfs - _bytesLoaded;
	if (_modeSrc == FLMODE_SRC_MEMORY)
	{
		const UINT8* dataPtr = (const UINT8*)_hLoad->dataPtr;
		_data.resize(endOfs);
		memcpy(&_data[_bytesLoaded], &dataPtr[_bytesLoaded], numBytes);
		_bytesLoaded = endOfs;
	}
	else if (_modeSrc == FLMODE_SRC_FILE)
	{
		if (_modeCompr == FLMODE_CMP_RAW)
		{
			UINT32 readBytes;
			
			_data.resize(endOfs);
			readBytes = fread(&_data[_bytesLoaded], 0x01, numBytes, _hLoad->hFileRaw);
			_bytesLoaded += readBytes;
		}
		else if (_modeCompr == FLMODE_CMP_GZ)
		{
			UINT32 readBytes;
			
			if (endOfs != (UINT32)-1)
			{
				_data.resize(endOfs);
				readBytes = gzread(_hLoad->hFileGZ, &_data[_bytesLoaded], numBytes);
				_bytesLoaded += readBytes;
			}
			else
			{
				UINT32 blkSize = 0x4000;
				if (blkSize > numBytes)
					blkSize = numBytes;
				do
				{
					_data.resize(_bytesLoaded + blkSize);
					readBytes = gzread(_hLoad->hFileGZ, &_data[_bytesLoaded], blkSize);
					_bytesLoaded += readBytes;
				} while(readBytes >= blkSize && _bytesLoaded < endOfs);
				if (readBytes < blkSize)	// reached EOF
					_bytesTotal = _bytesLoaded;
			}
		}
	}
	if (_bytesLoaded >= _bytesTotal)
	{
		CancelLoading();
		_status = FLSTAT_LOADED;
	}
	
	return;
}
