#include "rar.hpp"

static File *CreatedFiles[16];

File::File()
{
  hFile=BAD_HANDLE;
  *FileName=0;
  *FileNameW=0;
  NewFile=false;
  LastWrite=false;
  HandleType=FILE_HANDLENORMAL;
  SkipClose=false;
  IgnoreReadErrors=false;
  ErrorType=FILE_SUCCESS;
  OpenShared=false;
  AllowDelete=true;
}


File::~File()
{
  if (hFile!=BAD_HANDLE && !SkipClose)
    if (NewFile)
      Delete();
    else
      Close();
}


void File::operator = (File &SrcFile)
{
  hFile=SrcFile.hFile;
  strcpy(FileName,SrcFile.FileName);
  NewFile=SrcFile.NewFile;
  LastWrite=SrcFile.LastWrite;
  HandleType=SrcFile.HandleType;
  SrcFile.SkipClose=true;
}


bool File::Open(const char *Name,const wchar *NameW,bool OpenShared,bool Update)
{
  ErrorType=FILE_SUCCESS;
  FileHandle hNewFile;
#ifdef _WIN_32
  uint Access=GENERIC_READ;
  if (Update)
    Access|=GENERIC_WRITE;
  uint ShareMode=FILE_SHARE_READ;
  if (File::OpenShared || OpenShared)
    ShareMode|=FILE_SHARE_WRITE;
  if (WinNT() && NameW!=NULL && *NameW!=0)
    hNewFile=CreateFileW(NameW,Access,ShareMode,NULL,OPEN_EXISTING,
                         FILE_FLAG_SEQUENTIAL_SCAN,NULL);
  else
    hNewFile=CreateFile(Name,Access,ShareMode,NULL,OPEN_EXISTING,
                        FILE_FLAG_SEQUENTIAL_SCAN,NULL);

  if (hNewFile==BAD_HANDLE && GetLastError()==ERROR_FILE_NOT_FOUND)
    ErrorType=FILE_NOTFOUND;
#else
  int flags=Update ? O_RDWR:O_RDONLY;
#ifdef O_BINARY
  flags|=O_BINARY;
#endif
#if defined(_EMX) && !defined(_DJGPP)
  int sflags=OpenShared ? SH_DENYNO:SH_DENYRW;
  int handle=sopen(Name,flags,sflags);
#else
  int handle=open(Name,flags);
#ifdef LOCK_EX
  if (!OpenShared && handle>=0 && flock(handle,LOCK_EX|LOCK_NB)==-1)
  {
    close(handle);
    return(false);
  }
#endif
#endif
  hNewFile=handle==-1 ? BAD_HANDLE:fdopen(handle,Update ? UPDATEBINARY:READBINARY);
  if (hNewFile==BAD_HANDLE && errno==ENOENT)
    ErrorType=FILE_NOTFOUND;
#endif
  NewFile=false;
  HandleType=FILE_HANDLENORMAL;
  SkipClose=false;
  bool Success=hNewFile!=BAD_HANDLE;
  if (Success)
  {
    hFile=hNewFile;
    if (NameW!=NULL)
      strcpyw(FileNameW,NameW);
    strcpy(FileName,Name);
  }
  return(Success);
}


#if !defined(SHELL_EXT) && !defined(SFX_MODULE)
void File::TOpen(const char *Name,const wchar *NameW)
{
  if (!WOpen(Name,NameW))
    ErrHandler.Exit(OPEN_ERROR);
}
#endif


bool File::WOpen(const char *Name,const wchar *NameW)
{
  if (Open(Name,NameW))
    return(true);
  ErrHandler.OpenErrorMsg(Name);
  return(false);
}


bool File::Create(const char *Name,const wchar *NameW)
{
#ifdef _WIN_32
  if (WinNT() && NameW!=NULL && *NameW!=0)
    hFile=CreateFileW(NameW,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,
                      CREATE_ALWAYS,0,NULL);
  else
    hFile=CreateFile(Name,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,
                     CREATE_ALWAYS,0,NULL);
#else
  hFile=fopen(Name,CREATEBINARY);
#endif
  NewFile=true;
  HandleType=FILE_HANDLENORMAL;
  SkipClose=false;
  if (NameW!=NULL)
    strcpyw(FileNameW,NameW);
  strcpy(FileName,Name);
  if (hFile!=BAD_HANDLE)
    for (int I=0;I<sizeof(CreatedFiles)/sizeof(CreatedFiles[0]);I++)
      if (CreatedFiles[I]==NULL)
      {
        CreatedFiles[I]=this;
        break;
      }
  return(hFile!=BAD_HANDLE);
}


#if !defined(SHELL_EXT) && !defined(SFX_MODULE)
void File::TCreate(const char *Name,const wchar *NameW)
{
  if (!WCreate(Name,NameW))
    ErrHandler.Exit(FATAL_ERROR);
}
#endif


bool File::WCreate(const char *Name,const wchar *NameW)
{
  if (Create(Name,NameW))
    return(true);
  ErrHandler.CreateErrorMsg(Name);
  return(false);
}


bool File::Close()
{
  bool Success=true;
  if (HandleType!=FILE_HANDLENORMAL)
    HandleType=FILE_HANDLENORMAL;
  else
    if (hFile!=BAD_HANDLE)
    {
      if (!SkipClose)
      {
#ifdef _WIN_32
        Success=CloseHandle(hFile);
#else
        Success=fclose(hFile)!=EOF;
#endif
        for (int I=0;I<sizeof(CreatedFiles)/sizeof(CreatedFiles[0]);I++)
          if (CreatedFiles[I]==this)
          {
            CreatedFiles[I]=NULL;
            break;
          }
      }
      hFile=BAD_HANDLE;
      if (!Success)
        ErrHandler.CloseError(FileName);
    }
  return(Success);
}


void File::Flush()
{
#ifdef _WIN_32
  FlushFileBuffers(hFile);
#else
  fflush(hFile);
#endif
}


bool File::Delete()
{
  if (HandleType!=FILE_HANDLENORMAL || !AllowDelete)
    return(false);
  if (hFile!=BAD_HANDLE)
    Close();
  bool RetCode;
#ifdef _WIN_32
  if (WinNT() && *FileNameW)
    RetCode=DeleteFileW(FileNameW);
  else
    RetCode=DeleteFile(FileName);
#else
  RetCode=(remove(FileName)==0);
#endif
  return(RetCode);
}


bool File::Rename(const char *NewName)
{
  bool Success=strcmp(FileName,NewName)==0 || rename(FileName,NewName)==0;
  if (Success)
  {
    strcpy(FileName,NewName);
    *FileNameW=0;
  }
  return(Success);
}


void File::Write(const void *Data,int Size)
{
  if (Size==0)
    return;
  if (HandleType!=FILE_HANDLENORMAL)
    switch(HandleType)
    {
      case FILE_HANDLESTD:
#ifdef _WIN_32
        hFile=GetStdHandle(STD_OUTPUT_HANDLE);
#else
        hFile=stdout;
#endif
        break;
      case FILE_HANDLEERR:
#ifdef _WIN_32
        hFile=GetStdHandle(STD_ERROR_HANDLE);
#else
        hFile=stderr;
#endif
        break;
    }
  while (1)
  {
    bool Success;
#ifdef _WIN_32
    DWORD Written;
    if (HandleType!=FILE_HANDLENORMAL)
    {
      const int MaxSize=0x4000;
      for (int I=0;I<Size;I+=MaxSize)
        if (!(Success=WriteFile(hFile,(byte *)Data+I,Min(Size-I,MaxSize),&Written,NULL)))
          break;
    }
    else
      Success=WriteFile(hFile,Data,Size,&Written,NULL);
#else
    Success=(fwrite(Data,1,Size,hFile)==Size && !ferror(hFile));
#endif
    if (!Success && HandleType==FILE_HANDLENORMAL)
    {
#if defined(_WIN_32) && !defined(SFX_MODULE) && !defined(RARDLL)
      Int64 FileSize=Tell()+Size;
      if (GetFreeDisk(FileName)>Size && FileSize>0xffffffff)
        ErrHandler.WriteErrorFAT(FileName);
#endif
      if (ErrHandler.AskRepeatWrite(FileName))
        continue;
      ErrHandler.WriteError(FileName);
    }
    break;
  }
  LastWrite=true;
}


int File::Read(void *Data,int Size)
{
  Int64 FilePos;
  if (IgnoreReadErrors)
    FilePos=Tell();
  int ReadSize;
  while (true)
  {
    ReadSize=DirectRead(Data,Size);
    if (ReadSize==-1)
      if (IgnoreReadErrors)
      {
        ReadSize=0;
        for (int I=0;I<Size;I+=512)
        {
          Seek(FilePos+I,SEEK_SET);
          int SizeToRead=Min(Size-I,512);
          int ReadCode=DirectRead(Data,SizeToRead);
          ReadSize+=(ReadCode==-1) ? 512:ReadCode;
        }
      }
      else
      {
        if (HandleType==FILE_HANDLENORMAL && ErrHandler.AskRepeatRead(FileName))
          continue;
        ErrHandler.ReadError(FileName);
      }
    break;
  }
  return(ReadSize);
}


int File::DirectRead(void *Data,int Size)
{
#ifdef _WIN_32
  const int MaxDeviceRead=20000;
#endif
  if (HandleType==FILE_HANDLESTD)
  {
#ifdef _WIN_32
    if (Size>MaxDeviceRead)
      Size=MaxDeviceRead;
    hFile=GetStdHandle(STD_INPUT_HANDLE);
#else
    hFile=stdin;
#endif
  }
#ifdef _WIN_32
  DWORD Read;
  if (!ReadFile(hFile,Data,Size,&Read,NULL))
  {
    if (IsDevice() && Size>MaxDeviceRead)
      return(DirectRead(Data,MaxDeviceRead));
    if (HandleType==FILE_HANDLESTD && GetLastError()==ERROR_BROKEN_PIPE)
      return(0);
    return(-1);
  }
  return(Read);
#else
  if (LastWrite)
  {
    fflush(hFile);
    LastWrite=false;
  }
  clearerr(hFile);
  int ReadSize=fread(Data,1,Size,hFile);
  if (ferror(hFile))
    return(-1);
  return(ReadSize);
#endif
}


void File::Seek(Int64 Offset,int Method)
{
  if (!RawSeek(Offset,Method))
    ErrHandler.SeekError(FileName);
}


bool File::RawSeek(Int64 Offset,int Method)
{
  if (hFile==BAD_HANDLE)
    return(true);
#ifdef _WIN_32
  LONG HighDist=int64to32(Offset>>32);
  if (SetFilePointer(hFile,int64to32(Offset),&HighDist,Method)==0xffffffff &&
      GetLastError()!=NO_ERROR)
    return(false);
#else
  LastWrite=false;
  if (fseek(hFile,int64to32(Offset),Method)!=0)
    return(false);
#endif
  return(true);
}


Int64 File::Tell()
{
#ifdef _WIN_32
  LONG HighDist=0;
  uint LowDist=SetFilePointer(hFile,0,&HighDist,FILE_CURRENT);
  if (LowDist==0xffffffff && GetLastError()!=NO_ERROR)
    ErrHandler.SeekError(FileName);
  return(int32to64(HighDist,LowDist));
#else
  return(ftell(hFile));
#endif
}


void File::Prealloc(Int64 Size)
{
#ifdef _WIN_32
  if (RawSeek(Size,SEEK_SET))
  {
    Truncate();
    Seek(0,SEEK_SET);
  }
#endif
}


byte File::GetByte()
{
  byte Byte=0;
  Read(&Byte,1);
  return(Byte);
}


void File::PutByte(byte Byte)
{
  Write(&Byte,1);
}


bool File::Truncate()
{
#ifdef _WIN_32
  return(SetEndOfFile(hFile));
#else
  return(false);
#endif
}


void File::SetOpenFileTime(uint ft)
{
#ifdef _WIN_32
  FILETIME LocalTime,FileTime;
  DosDateTimeToFileTime(HIWORD(ft),LOWORD(ft),&LocalTime);
  LocalFileTimeToFileTime(&LocalTime,&FileTime);
  SetFileTime(hFile,NULL,NULL,&FileTime);
#endif
}


void File::SetCloseFileTime(uint ft)
{
#if defined(_UNIX) || defined(_EMX)
  struct utimbuf ut;
  ut.actime=ut.modtime=DosTimeToUnix(ft);
  utime(FileName,&ut);
#endif
}


uint File::GetOpenFileTime()
{
#ifdef _WIN_32
  FILETIME LocalTime,FileTime;
  GetFileTime(hFile,NULL,NULL,&FileTime);
  FileTimeToLocalFileTime(&FileTime,&LocalTime);
  WORD FatDate,FatTime;
  FileTimeToDosDateTime(&LocalTime,&FatDate,&FatTime);
  return((FatDate<<16)|FatTime);
#endif
#if defined(_UNIX) || defined(_EMX)
  struct stat st;
  fstat(fileno(hFile),&st);
  return(UnixTimeToDos(st.st_mtime));
#endif
}


void File::SetOpenFileStat(uint FileTime)
{
#ifdef _WIN_32
  SetOpenFileTime(FileTime);
#endif
}


void File::SetCloseFileStat(uint FileTime,uint FileAttr)
{
#ifdef _WIN_32
  SetFileAttr(FileName,FileNameW,FileAttr);
#endif
#ifdef _EMX
  SetCloseFileTime(FileTime);
  SetFileAttr(FileName,FileNameW,FileAttr);
#endif
#ifdef _UNIX
  SetCloseFileTime(FileTime);
  chmod(FileName,(mode_t)FileAttr);
#endif
}


Int64 File::FileLength()
{
  SaveFilePos SavePos(*this);
  Seek(0,SEEK_END);
  return(Tell());
}


void File::SetHandleType(FILE_HANDLETYPE Type)
{
  HandleType=Type;
}


bool File::IsDevice()
{
  if (hFile==BAD_HANDLE)
    return(false);
#ifdef _WIN_32
  uint Type=GetFileType(hFile);
  return(Type==FILE_TYPE_CHAR || Type==FILE_TYPE_PIPE);
#else
  return(isatty(fileno(hFile)));
#endif
}


#ifndef SFX_MODULE
void File::fprintf(const char *fmt,...)
{
  va_list argptr;
  va_start(argptr,fmt);
  char Msg[8192],OutMsg[8192];
  vsprintf(Msg,fmt,argptr);
#ifdef _WIN_32
  for (int Src=0,Dest=0;;Src++)
  {
    char CurChar=Msg[Src];
    if (CurChar=='\n')
      OutMsg[Dest++]='\r';
    OutMsg[Dest++]=CurChar;
    if (CurChar==0)
      break;
  }
#else
  strcpy(OutMsg,Msg);
#endif
  Write(OutMsg,strlen(OutMsg));
  va_end(argptr);
}
#endif


void File::RemoveCreated()
{
  for (int I=0;I<sizeof(CreatedFiles)/sizeof(CreatedFiles[0]);I++)
    if (CreatedFiles[I]!=NULL)
      CreatedFiles[I]->Delete();
}


#ifndef SFX_MODULE
long File::Copy(File &Dest,Int64 Length)
{
  Array<char> Buffer(0x10000);
  long CopySize=0;
  bool CopyAll=(Length==INT64ERR);

  while (CopyAll || Length>0)
  {
    Wait();
    int SizeToRead=(!CopyAll && Length<Buffer.Size()) ? int64to32(Length):Buffer.Size();
    int ReadSize=Read(&Buffer[0],SizeToRead);
    if (ReadSize==0)
      break;
    Dest.Write(&Buffer[0],ReadSize);
    CopySize+=ReadSize;
    if (!CopyAll)
      Length-=ReadSize;
  }
  return(CopySize);
}
#endif
