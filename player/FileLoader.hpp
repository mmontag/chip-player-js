#ifndef __FILELOADER_HPP__
#define __FILELOADER_HPP__

#include <stdtype.h>
#include <vector>
#include <string>

union LoaderHandles;
class FileLoader
{
public:
	enum
	{
		FLSTAT_EMPTY = 0,
		FLSTAT_LOADING = 1,
		FLSTAT_LOADED = 2
	};
	enum
	{
		// mode: sources
		FLMODE_SRC_MEMORY = 0x01,
		FLMODE_SRC_FILE = 0x02,
		
		// mode: compression
		FLMODE_CMP_RAW = 0x00,
		FLMODE_CMP_GZ = 0x10
	};
	
	FileLoader();
	~FileLoader();
	
	UINT8 LoadData(UINT32 dataSize, const void* data);	// load from RAM (copies data)
	UINT8 LoadFile(const char* fileName);			// load from file, possibly decompressing
	UINT8 LoadFile(const std::string& fileName);		// as above
	UINT8 CancelLoading(void);
	UINT8 FreeData(void);
	
	void SetPreloadBytes(UINT32 byteCount);	// only load X bytes after calling LoadFile (for header checking)
	void ReadUntil(UINT32 fileOffset);
	void ReadFullFile(void);	// call to read the rest of the file
	
	std::vector<UINT8>& GetFileData(void);	// return (writable) reference
	UINT32 GetTotalFileSize() const;	// size of completely loaded file (may be -1 when unknown)
	UINT32 GetFileSize() const;		// number of bytes already loaded, equals TotalFileSize when loading is complete
	UINT8 GetMode() const;		// see FLMODE constants
	UINT8 GetStatus() const;	// see FLSTAT constants
private:
	void ReadData(UINT32 numBytes);
	
	UINT8 _modeSrc;
	UINT8 _modeCompr;
	UINT8 _status;
	UINT32 _bytesTotal;
	UINT32 _bytesLoaded;
	std::vector<UINT8> _data;
	
	UINT32 _readStopOfs;
	LoaderHandles* _hLoad;
};

#endif	// __FILELOADER_HPP__
