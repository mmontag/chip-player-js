#include "rar.hpp"

#ifdef _WIN_32
static FILETIME SystemTime;
#else
static time_t SystemTime;
#endif


void InitTime()
{
#ifdef _WIN_32
  GetSystemTimeAsFileTime(&SystemTime);
#else
  time(&SystemTime);
#endif
}


#ifndef SFX_MODULE
uint SecondsToDosTime(uint Seconds)
{
#ifdef _WIN_32
  FILETIME ft=SystemTime;
  Int64 f=int32to64(ft.dwHighDateTime,ft.dwLowDateTime),s=Seconds;
  f=f-s*10000000;
  ft.dwHighDateTime=int64to32(f>>32);
  ft.dwLowDateTime=int64to32(f);
  return(NTTimeToDos(&ft));
#endif
#if defined(_UNIX) || defined(_EMX)
  return(UnixTimeToDos(SystemTime-Seconds));
#endif
}
#endif


void ConvertDate(uint ft,char *DateStr,bool FullYear)
{
  int Day=(ft>>16)&0x1f;
  int Month=(ft>>21)&0xf;
  int Year=(ft>>25)+1980;
  int Hour=(ft>>11)&0x1f;
  int Minute=(ft>>5)&0x3f;
  if (FullYear)
    sprintf(DateStr,"%02u-%02u-%u %02u:%02u",Day,Month,Year,Hour,Minute);
  else
    sprintf(DateStr,"%02u-%02u-%02u %02u:%02u",Day,Month,Year%100,Hour,Minute);
}


#ifndef SFX_MODULE
const char *GetMonthName(int Month)
{
#ifdef SILENT
  return("");
#else
  static MSGID MonthID[]={
         MMonthJan,MMonthFeb,MMonthMar,MMonthApr,MMonthMay,MMonthJun,
         MMonthJul,MMonthAug,MMonthSep,MMonthOct,MMonthNov,MMonthDec
  };
  return(St(MonthID[Month]));
#endif
}
#endif


#ifndef SFX_MODULE
uint TextAgeToSeconds(char *TimeText)
{
  uint Seconds=0,Value=0;
  for (int I=0;TimeText[I]!=0;I++)
  {
    int Ch=TimeText[I];
    if (isdigit(Ch))
      Value=Value*10+Ch-'0';
    else
    {
      switch(toupper(Ch))
      {
        case 'D':
          Seconds+=Value*24*3600;
          break;
        case 'H':
          Seconds+=Value*3600;
          break;
        case 'M':
          Seconds+=Value*60;
          break;
        case 'S':
          Seconds+=Value;
          break;
      }
      Value=0;
    }
  }
  return(Seconds);
}
#endif


#ifndef SFX_MODULE
uint IsoTextToDosTime(char *TimeText)
{
  int Field[6];
  memset(Field,0,sizeof(Field));
  for (int DigitCount=0;*TimeText!=0;TimeText++)
    if (isdigit(*TimeText))
    {
      int FieldPos=DigitCount<4 ? 0:(DigitCount-4)/2+1;
      if (FieldPos<sizeof(Field)/sizeof(Field[0]))
        Field[FieldPos]=Field[FieldPos]*10+*TimeText-'0';
      DigitCount++;
    }
  if (Field[1]==0)
    Field[1]=1;
  if (Field[2]==0)
    Field[2]=1;
  uint DosTime=((Field[0]-1980)<<25)|(Field[1]<<21)|(Field[2]<<16)|
               (Field[3]<<11)|(Field[4]<<5)|(Field[5]/2);
  return(DosTime);
}
#endif


#if defined(_UNIX) || defined(_EMX)
uint UnixTimeToDos(time_t UnixTime)
{
  struct tm *t;
  uint DosTime;
  t=localtime(&UnixTime);
  DosTime=(t->tm_sec/2)|(t->tm_min<<5)|(t->tm_hour<<11)|
          (t->tm_mday<<16)|((t->tm_mon+1)<<21)|((t->tm_year-80)<<25);
  return(DosTime);
}


time_t DosTimeToUnix(uint DosTime)
{
  struct tm t;

  t.tm_sec=(DosTime & 0x1f)*2;
  t.tm_min=(DosTime>>5) & 0x3f;
  t.tm_hour=(DosTime>>11) & 0x1f;
  t.tm_mday=(DosTime>>16) & 0x1f;
  t.tm_mon=((DosTime>>21)-1) & 0x0f;
  t.tm_year=(DosTime>>25)+80;
  t.tm_isdst=-1;
  return(mktime(&t));
}
#endif


#ifdef _WIN_32
uint NTTimeToDos(FILETIME *ft)
{
  WORD DosDate,DosTime;
  FILETIME ct;
  FileTimeToLocalFileTime(ft,&ct);
  FileTimeToDosDateTime(&ct,&DosDate,&DosTime);
  return(((uint)DosDate<<16)|DosTime);
}
#endif




