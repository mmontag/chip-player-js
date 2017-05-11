#include "rar.hpp"

MKDIR_CODE MakeDir(const char *Name,const wchar *NameW,uint Attr)
{
#ifdef _WIN_32
  int Success;
  if (WinNT() && NameW!=NULL && *NameW!=0)
    Success=CreateDirectoryW(NameW,NULL);
  else
    Success=CreateDirectory(Name,NULL);
  if (Success)
  {
    SetFileAttr(Name,NameW,Attr);
    return(MKDIR_SUCCESS);
  }
  int ErrCode=GetLastError();
  if (ErrCode==ERROR_FILE_NOT_FOUND || ErrCode==ERROR_PATH_NOT_FOUND)
    return(MKDIR_BADPATH);
  return(MKDIR_ERROR);
#endif
#ifdef _EMX
#ifdef _DJGPP
  if (mkdir(Name,(Attr & FA_RDONLY) ? 0:S_IWUSR)==0)
#else
  if (__mkdir(Name)==0)
#endif
  {
    SetFileAttr(Name,NameW,Attr);
    return(MKDIR_SUCCESS);
  }
  return(errno==ENOENT ? MKDIR_BADPATH:MKDIR_ERROR);
#endif
#ifdef _UNIX
  int prevmask=umask(0);
  int ErrCode=mkdir(Name,(mode_t)Attr);
  umask(prevmask);
  if (ErrCode==-1)
    return(errno==ENOENT ? MKDIR_BADPATH:MKDIR_ERROR);
  return(MKDIR_SUCCESS);
#endif
}


void CreatePath(const char *Path,const wchar *PathW,bool SkipLastName)
{
#ifdef _WIN_32
  uint DirAttr=0;
#else
  uint DirAttr=0777;
#endif
  int PosW=0;
  for (const char *s=Path;*s!=0 && PosW<NM;s=charnext(s),PosW++)
  {
    bool Wide=PathW!=NULL && *PathW!=0;
    if (Wide && PathW[PosW]==CPATHDIVIDER || !Wide && *s==CPATHDIVIDER)
    {
      wchar *DirPtrW=NULL;
      if (Wide)
      {
        wchar DirNameW[NM];
        strncpyw(DirNameW,PathW,PosW);
        DirNameW[PosW]=0;
        DirPtrW=DirNameW;
      }
      char DirName[NM];
      strncpy(DirName,Path,s-Path);
      DirName[s-Path]=0;
      if (MakeDir(DirName,DirPtrW,DirAttr)==MKDIR_SUCCESS)
      {
#ifndef GUI
        mprintf(St(MCreatDir),DirName);
        mprintf(" %s",St(MOk));
#endif
      }
    }
  }
  if (!SkipLastName)
    MakeDir(Path,PathW,DirAttr);
}


void SetDirTime(const char *Name,RarTime *ftm,RarTime *ftc,RarTime *fta)
{
  bool sm=ftm!=NULL && ftm->IsSet();
  bool sc=ftc!=NULL && ftc->IsSet();
  bool sa=ftc!=NULL && fta->IsSet();
#ifdef _WIN_32
  if (!WinNT())
    return;
  HANDLE hFile=CreateFile(Name,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                          NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
  if (hFile==INVALID_HANDLE_VALUE)
    return;
  FILETIME fm,fc,fa;
  if (sm)
    ftm->GetWin32(&fm);
  if (sc)
    ftc->GetWin32(&fc);
  if (sa)
    fta->GetWin32(&fa);
  SetFileTime(hFile,sc ? &fc:NULL,sa ? &fa:NULL,sm ? &fm:NULL);
  CloseHandle(hFile);
#endif
#if defined(_UNIX) || defined(_EMX)
  File::SetCloseFileTimeByName(Name,ftm,fta);
#endif
}


bool IsRemovable(const char *FileName)
{
#ifdef _WIN_32
  char Root[NM];
  GetPathRoot(FileName,Root);
  int Type=GetDriveType(*Root ? Root:NULL);
  return(Type==DRIVE_REMOVABLE || Type==DRIVE_CDROM);
#elif defined(_EMX)
  char Drive=toupper(FileName[0]);
  return((Drive=='A' || Drive=='B') && FileName[1]==':');
#else
  return(false);
#endif
}


#ifndef SFX_MODULE
Int64 GetFreeDisk(const char *FileName)
{
#ifdef _WIN_32
  char Root[NM];
  GetPathRoot(FileName,Root);

  typedef BOOL (WINAPI *GETDISKFREESPACEEX)(
    LPCTSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER
   );
  static GETDISKFREESPACEEX pGetDiskFreeSpaceEx=NULL;

  if (pGetDiskFreeSpaceEx==NULL)
  {
    HMODULE hKernel=GetModuleHandle("kernel32.dll");
    if (hKernel!=NULL)
      pGetDiskFreeSpaceEx=(GETDISKFREESPACEEX)GetProcAddress(hKernel,"GetDiskFreeSpaceExA");
  }
  if (pGetDiskFreeSpaceEx!=NULL)
  {
    GetFilePath(FileName,Root);
    ULARGE_INTEGER uiTotalSize,uiTotalFree,uiUserFree;
    uiUserFree.u.LowPart=uiUserFree.u.HighPart=0;
    if (pGetDiskFreeSpaceEx(*Root ? Root:NULL,&uiUserFree,&uiTotalSize,&uiTotalFree) &&
        uiUserFree.u.HighPart<=uiTotalFree.u.HighPart)
      return(int32to64(uiUserFree.u.HighPart,uiUserFree.u.LowPart));
  }

  DWORD SectorsPerCluster,BytesPerSector,FreeClusters,TotalClusters;
  if (!GetDiskFreeSpace(*Root ? Root:NULL,&SectorsPerCluster,&BytesPerSector,&FreeClusters,&TotalClusters))
    return(1457664);
  Int64 FreeSize=SectorsPerCluster*BytesPerSector;
  FreeSize=FreeSize*FreeClusters;
  return(FreeSize);
#elif defined(_BEOS)
  char Root[NM];
  GetFilePath(FileName,Root);
  dev_t Dev=dev_for_path(*Root ? Root:".");
  if (Dev<0)
    return(1457664);
  fs_info Info;
  if (fs_stat_dev(Dev,&Info)!=0)
    return(1457664);
  Int64 FreeSize=Info.block_size;
  FreeSize=FreeSize*Info.free_blocks;
  return(FreeSize);
#elif defined(_UNIX)
  return(1457664);
#elif defined(_EMX)
  int Drive=(!isalpha(FileName[0]) || FileName[1]!=':') ? 0:toupper(FileName[0])-'A'+1;
  if (_osmode == OS2_MODE)
  {
    FSALLOCATE fsa;
    if (DosQueryFSInfo(Drive,1,&fsa,sizeof(fsa))!=0)
      return(1457664);
    Int64 FreeSize=fsa.cSectorUnit*fsa.cbSector;
    FreeSize=FreeSize*fsa.cUnitAvail;
    return(FreeSize);
  }
  else
  {
    union REGS regs,outregs;
    memset(&regs,0,sizeof(regs));
    regs.h.ah=0x36;
    regs.h.dl=Drive;
    _int86 (0x21,&regs,&outregs);
    if (outregs.x.ax==0xffff)
      return(1457664);
    Int64 FreeSize=outregs.x.ax*outregs.x.cx;
    FreeSize=FreeSize*outregs.x.bx;
    return(FreeSize);
  }
#else
  #define DISABLEAUTODETECT
  return(1457664);
#endif
}
#endif


bool FileExist(const char *FileName,const wchar *FileNameW)
{
#ifdef _WIN_32
  if (WinNT() && FileNameW!=NULL && *FileNameW!=0)
    return(GetFileAttributesW(FileNameW)!=0xffffffff);
  else
    return(GetFileAttributes(FileName)!=0xffffffff);
#elif defined(ENABLE_ACCESS)
  return(access(FileName,0)==0);
#else
  struct FindData FD;
  return(FindFile::FastFind(FileName,FileNameW,&FD));
#endif
}


bool WildFileExist(const char *FileName,const wchar *FileNameW)
{
  if (IsWildcard(FileName,FileNameW))
  {
    FindFile Find;
    Find.SetMask(FileName);
    Find.SetMaskW(FileNameW);
    struct FindData fd;
    return(Find.Next(&fd));
  }
  return(FileExist(FileName,FileNameW));
}


bool IsDir(uint Attr)
{
#if defined (_WIN_32) || defined(_EMX)
  return(Attr!=0xffffffff && (Attr & 0x10)!=0);
#endif
#if defined(_UNIX)
  return((Attr & 0xF000)==0x4000);
#endif
}


bool IsUnreadable(uint Attr)
{
#if defined(_UNIX) && defined(S_ISFIFO) && defined(S_ISSOCK) && defined(S_ISCHR)
  return(S_ISFIFO(Attr) || S_ISSOCK(Attr) || S_ISCHR(Attr));
#endif
  return(false);
}


bool IsLabel(uint Attr)
{
#if defined (_WIN_32) || defined(_EMX)
  return((Attr & 8)!=0);
#else
  return(false);
#endif
}


bool IsLink(uint Attr)
{
#ifdef _UNIX
  return((Attr & 0xF000)==0xA000);
#endif
  return(false);
}






bool IsDeleteAllowed(uint FileAttr)
{
#if defined(_WIN_32) || defined(_EMX)
  return((FileAttr & (FA_RDONLY|FA_SYSTEM|FA_HIDDEN))==0);
#else
  return(false);
#endif
}


void PrepareToDelete(const char *Name,const wchar *NameW)
{
#if defined(_WIN_32) || defined(_EMX)
  SetFileAttr(Name,NameW,0);
#endif
#ifdef _UNIX
  chmod(Name,S_IRUSR|S_IWUSR);
#endif
}


uint GetFileAttr(const char *Name,const wchar *NameW)
{
#ifdef _WIN_32
  if (WinNT() && NameW!=NULL && *NameW!=0)
    return(GetFileAttributesW(NameW));
  else
    return(GetFileAttributes(Name));
#elif defined(_DJGPP)
  return(_chmod(Name,0));
#else
  struct stat st;
  if (stat(Name,&st)!=0)
    return(0);
#ifdef _EMX
  return(st.st_attr);
#else
  return(st.st_mode);
#endif
#endif
}


bool SetFileAttr(const char *Name,const wchar *NameW,uint Attr)
{
  bool Success;
#ifdef _WIN_32
  if (WinNT() && NameW!=NULL && *NameW!=0)
    Success=SetFileAttributesW(NameW,Attr)!=0;
  else
    Success=SetFileAttributes(Name,Attr)!=0;
#elif defined(_DJGPP)
  Success=_chmod(Name,1,Attr)!=-1;
#elif defined(_EMX)
  Success=__chmod(Name,1,Attr)!=-1;
#elif defined(_UNIX)
  Success=chmod(Name,(mode_t)Attr)==0;
#else
  Success=false;
#endif
  return(Success);
}


void ConvertNameToFull(const char *Src,char *Dest)
{
#ifdef _WIN_32
  char FullName[NM],*NamePtr;
  if (GetFullPathName(Src,sizeof(FullName),FullName,&NamePtr))
    strcpy(Dest,FullName);
  else
    if (Src!=Dest)
      strcpy(Dest,Src);
#else
  char FullName[NM];
  if (IsPathDiv(*Src) || IsDiskLetter(Src))
    strcpy(FullName,Src);
  else
  {
    getcwd(FullName,sizeof(FullName));
    AddEndSlash(FullName);
    strcat(FullName,Src);
  }
  strcpy(Dest,FullName);
#endif
}


#ifndef SFX_MODULE
void ConvertNameToFull(const wchar *Src,wchar *Dest)
{
  if (Src==NULL || *Src==0)
  {
    *Dest=0;
    return;
  }
#ifdef _WIN_32
  if (WinNT())
  {
    wchar FullName[NM],*NamePtr;
    if (GetFullPathNameW(Src,sizeof(FullName)/sizeof(FullName[0]),FullName,&NamePtr))
      strcpyw(Dest,FullName);
    else
      if (Src!=Dest)
        strcpyw(Dest,Src);
  }
  else
  {
    char AnsiName[NM];
    WideToChar(Src,AnsiName);
    ConvertNameToFull(AnsiName,AnsiName);
    CharToWide(AnsiName,Dest);
  }
#else
  char AnsiName[NM];
  WideToChar(Src,AnsiName);
  ConvertNameToFull(AnsiName,AnsiName);
  CharToWide(AnsiName,Dest);
#endif
}
#endif


#ifndef SFX_MODULE
char *MkTemp(char *Name)
{
  int Length=strlen(Name);
  if (Length<=6)
    return(NULL);
  for (int Random=clock(),Attempt=0;;Attempt++)
  {
    sprintf(Name+Length-6,"%06u",Random+Attempt);
    Name[Length-4]='.';
    if (!FileExist(Name))
      break;
    if (Attempt==1000)
      return(NULL);
  }
  return(Name);
}
#endif


#ifndef SFX_MODULE
uint CalcFileCRC(File *SrcFile,Int64 Size)
{
  SaveFilePos SavePos(*SrcFile);
  const int BufSize=0x10000;
  Array<byte> Data(BufSize);
  int ReadSize,BlockCount=0;
  uint DataCRC=0xffffffff;


  SrcFile->Seek(0,SEEK_SET);
  while ((ReadSize=SrcFile->Read(&Data[0],int64to32(Size==INT64ERR ? Int64(BufSize):Min(Int64(BufSize),Size))))!=0)
  {
    if ((++BlockCount & 15)==0)
    {
      Wait();
    }
    DataCRC=CRC(DataCRC,&Data[0],ReadSize);
    if (Size!=INT64ERR)
      Size-=ReadSize;
  }
  return(DataCRC^0xffffffff);
}
#endif
