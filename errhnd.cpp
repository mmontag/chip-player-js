#include "rar.hpp"

static bool UserBreak;

ErrorHandler::ErrorHandler()
{
  Clean();
}


void ErrorHandler::Clean()
{
  ExitCode=SUCCESS;
  ErrCount=0;
  EnableBreak=true;
  Silent=false;
  DoShutdown=false;
}


void ErrorHandler::MemoryError()
{
  MemoryErrorMsg();
  Throw(MEMORY_ERROR);
}


void ErrorHandler::OpenError(const char *FileName)
{
#ifndef SILENT
  OpenErrorMsg(FileName);
  Throw(OPEN_ERROR);
#endif
}


void ErrorHandler::CloseError(const char *FileName)
{
#ifndef SILENT
  if (!UserBreak)
  {
    ErrMsg(NULL,St(MErrFClose),FileName);
    SysErrMsg();
  }
  Throw(FATAL_ERROR);
#endif
}


void ErrorHandler::ReadError(const char *FileName)
{
#ifndef SILENT
  ReadErrorMsg(FileName);
  Throw(FATAL_ERROR);
#endif
}


bool ErrorHandler::AskRepeatRead(const char *FileName)
{
#if !defined(SILENT) && !defined(SFX_MODULE)
  if (!Silent)
  {
    mprintf("\n");
    Log(NULL,St(MErrRead),FileName);
    return(Ask(St(MRetryAbort))==1);
  }
#endif
  return(false);
}


void ErrorHandler::WriteError(const char *FileName)
{
#ifndef SILENT
  ErrMsg(NULL,St(MErrWrite),FileName);
  SysErrMsg();
  Throw(WRITE_ERROR);
#endif
}


#ifdef _WIN_32
void ErrorHandler::WriteErrorFAT(const char *FileName)
{
#if !defined(SILENT) && !defined(SFX_MODULE)
  SysErrMsg();
  ErrMsg(NULL,St(MNTFSRequired),FileName);
  Throw(WRITE_ERROR);
#endif
}
#endif


bool ErrorHandler::AskRepeatWrite(const char *FileName)
{
#ifndef SILENT
  if (!Silent)
  {
    mprintf("\n");
    Log(NULL,St(MErrWrite),FileName);
    return(Ask(St(MRetryAbort))==1);
  }
#endif
  return(false);
}


void ErrorHandler::SeekError(const char *FileName)
{
#ifndef SILENT
  if (!UserBreak)
  {
    ErrMsg(NULL,St(MErrSeek),FileName);
    SysErrMsg();
  }
  Throw(FATAL_ERROR);
#endif
}


void ErrorHandler::MemoryErrorMsg()
{
#ifndef SILENT
  ErrMsg(NULL,St(MErrOutMem));
#endif
}


void ErrorHandler::OpenErrorMsg(const char *FileName)
{
#ifndef SILENT
  Log(NULL,St(MCannotOpen),FileName);
  Alarm();
  SysErrMsg();
#endif
}


void ErrorHandler::CreateErrorMsg(const char *FileName)
{
#ifndef SILENT
  Log(NULL,St(MCannotCreate),FileName);
  Alarm();
  SysErrMsg();
#endif
}


void ErrorHandler::ReadErrorMsg(const char *FileName)
{
#ifndef SILENT
  ErrMsg(NULL,St(MErrRead),FileName);
  SysErrMsg();
#endif
}


void ErrorHandler::Exit(int ExitCode)
{
#ifndef SFX_MODULE
  Alarm();
#endif
  Throw(ExitCode);
}


#ifndef GUI
void ErrorHandler::ErrMsg(char *ArcName,const char *fmt,...)
{
  safebuf char Msg[NM+1024];
  va_list argptr;
  va_start(argptr,fmt);
  vsprintf(Msg,fmt,argptr);
  va_end(argptr);
#ifdef _WIN_32
  if (UserBreak)
    Sleep(5000);
#endif
  Alarm();
  if (*Msg)
  {
    Log(ArcName,"\n%s",Msg);
    mprintf("\n%s\n",St(MProgAborted));
  }
}
#endif


void ErrorHandler::SetErrorCode(int Code)
{
  switch(Code)
  {
    case WARNING:
    case USER_BREAK:
      if (ExitCode==SUCCESS)
        ExitCode=Code;
      break;
    case FATAL_ERROR:
      if (ExitCode==SUCCESS || ExitCode==WARNING)
        ExitCode=FATAL_ERROR;
      break;
    default:
      ExitCode=Code;
      break;
  }
  ErrCount++;
}


#if !defined(GUI) && !defined(_SFX_RTL_)
#ifdef _WIN_32
BOOL __stdcall ProcessSignal(DWORD SigType)
#else
#if defined(__sun)
extern "C"
#endif
void _stdfunction ProcessSignal(int SigType)
#endif
{
#ifdef _WIN_32
  if (SigType==CTRL_LOGOFF_EVENT)
    return(TRUE);
#endif
  UserBreak=true;
  mprintf(St(MBreak));
  File::RemoveCreated();
#if defined(USE_RC) && !defined(SFX_MODULE)
  ExtRes.UnloadDLL();
#endif
  exit(USER_BREAK);
#ifdef _WIN_32
  return(TRUE);
#endif
}
#endif


void ErrorHandler::SetSignalHandlers(bool Enable)
{
  EnableBreak=Enable;
#if !defined(GUI) && !defined(_SFX_RTL_)
#ifdef _WIN_32
  SetConsoleCtrlHandler(Enable ? ProcessSignal:NULL,TRUE);
//  signal(SIGBREAK,Enable ? ProcessSignal:SIG_IGN);
#else
  signal(SIGINT,Enable ? ProcessSignal:SIG_IGN);
  signal(SIGTERM,Enable ? ProcessSignal:SIG_IGN);
#endif
#endif
}


void ErrorHandler::Throw(int Code)
{
  if (Code==USER_BREAK && !EnableBreak)
    return;
  ErrHandler.SetErrorCode(Code);
#ifdef ALLOW_EXCEPTIONS
  throw Code;
#else
  File::RemoveCreated();
  exit(Code);
#endif
}


void ErrorHandler::SysErrMsg()
{
#if defined(_WIN_32) && !defined(SFX_MODULE) && !defined(SILENT)
  char *lpMsgBuf=NULL;
  int ErrType=GetLastError();
  if (ErrType!=0 && FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
              NULL,GetLastError(),MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
              (LPTSTR) &lpMsgBuf,0,NULL))
  {
    char *CurMsg=lpMsgBuf;
    while (CurMsg!=NULL)
    {
      while (*CurMsg=='\r' || *CurMsg=='\n')
        CurMsg++;
      if (*CurMsg==0)
        break;
      char *EndMsg=strchr(CurMsg,'\r');
      if (EndMsg==NULL)
        EndMsg=strchr(CurMsg,'\n');
      if (EndMsg!=NULL)
      {
        *EndMsg=0;
        EndMsg++;
      }
      Log(NULL,"\n%s",CurMsg);
      CurMsg=EndMsg;
    }
  }
  LocalFree( lpMsgBuf );
#endif
}
