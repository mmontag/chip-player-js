#ifndef _RAR_FILE_
#define _RAR_FILE_

#ifdef _WIN_32
typedef HANDLE FileHandle;
#define BAD_HANDLE INVALID_HANDLE_VALUE
#else
typedef FILE* FileHandle;
#define BAD_HANDLE NULL
#endif

class RAROptions;

enum FILE_HANDLETYPE {FILE_HANDLENORMAL,FILE_HANDLESTD,FILE_HANDLEERR};

enum FILE_ERRORTYPE {FILE_SUCCESS,FILE_NOTFOUND};

struct FileStat
{
  uint FileAttr;
  uint FileTime;
  Int64 FileSize;
  bool IsDir;
};


class File
{
  private:
    FileHandle hFile;
    bool LastWrite;
    FILE_HANDLETYPE HandleType;
    bool SkipClose;
    bool IgnoreReadErrors;
    bool OpenShared;
    bool NewFile;
    bool AllowDelete;
  public:
    char FileName[NM];
    wchar FileNameW[NM];

    FILE_ERRORTYPE ErrorType;
  public:
    File();
    virtual ~File();
    void operator = (File &SrcFile);
    bool Open(const char *Name,const wchar *NameW=NULL,bool OpenShared=false,bool Update=false);
    void TOpen(const char *Name,const wchar *NameW=NULL);
    bool WOpen(const char *Name,const wchar *NameW=NULL);
    bool Create(const char *Name,const wchar *NameW=NULL);
    void TCreate(const char *Name,const wchar *NameW=NULL);
    bool WCreate(const char *Name,const wchar *NameW=NULL);
    bool Close();
    void Flush();
    bool Delete();
    bool Rename(const char *NewName);
    void Write(const void *Data,int Size);
    int Read(void *Data,int Size);
    int DirectRead(void *Data,int Size);
    void Seek(Int64 Offset,int Method);
    bool RawSeek(Int64 Offset,int Method);
    Int64 Tell();
    void Prealloc(Int64 Size);
    byte GetByte();
    void PutByte(byte Byte);
    bool Truncate();
    void SetOpenFileTime(uint ft);
    void SetCloseFileTime(uint ft);
    void SetOpenFileStat(uint FileTime);
    void SetCloseFileStat(uint FileTime,uint FileAttr);
    uint GetOpenFileTime();
    bool IsOpened() {return(hFile!=BAD_HANDLE);};
    Int64 FileLength();
    void SetHandleType(FILE_HANDLETYPE Type);
    FILE_HANDLETYPE GetHandleType() {return(HandleType);};
    bool IsDevice();
    void fprintf(const char *fmt,...);
    static void RemoveCreated();
    FileHandle GetHandle() {return(hFile);};
    void SetIgnoreReadErrors(bool Mode) {IgnoreReadErrors=Mode;};
    void SetOpenShared(bool Mode) {OpenShared=Mode;};
    char *GetName() {return(FileName);}
    long Copy(File &Dest,Int64 Length=INT64ERR);
    void SetAllowDelete(bool Allow) {AllowDelete=Allow;}
};

#endif
