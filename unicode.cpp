#include "rar.hpp"

void WideToChar(const wchar_t *Src,char *Dest,int DestSize)
{
#ifdef _WIN_32
  WideCharToMultiByte(CP_ACP,0,Src,-1,Dest,DestSize,NULL,NULL);
#else
#ifdef MBFUNCTIONS
  if (wcstombs(Dest,Src,DestSize)==-1)
    *Dest=0;
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(char)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
#endif
}


void CharToWide(const char *Src,wchar_t *Dest,int DestSize)
{
#ifdef _WIN_32
  MultiByteToWideChar(CP_ACP,0,Src,-1,Dest,DestSize);
#else
#ifdef MBFUNCTIONS
  mbstowcs(Dest,Src,DestSize);
#else
  for (int I=0;I<DestSize;I++)
  {
    Dest[I]=(wchar_t)Src[I];
    if (Src[I]==0)
      break;
  }
#endif
#endif
}


byte* WideToRaw(const wchar *Src,byte *Dest,int DestSize)
{
  for (int I=0;I<DestSize;I++,Src++)
  {
    Dest[I*2]=(byte)*Src;
    Dest[I*2+1]=(byte)(*Src>>8);
    if (*Src==0)
      break;
  }
  return(Dest);
}


wchar* RawToWide(const byte *Src,wchar *Dest,int DestSize)
{
  for (int I=0;I<DestSize;I++)
    if ((Dest[I]=Src[I*2]+(Src[I*2+1]<<8))==0)
      break;
  return(Dest);
}


bool LowAscii(const wchar *Str)
{
  for (int I=0;Str[I]!=0;I++)
    if (Str[I]<32 || Str[I]>127)
      return(false);
  return(true);
}


int strlenw(const wchar *str)
{
  int length=0;
  while (*(str++)!=0)
    length++;
  return(length);
}


wchar* strcpyw(wchar *dest,const wchar *src)
{
  do {
    *(dest++)=*src;
  } while (*(src++)!=0);
  return(dest);
}


wchar* strncpyw(wchar *dest,const wchar *src,int n)
{
  do {
    *(dest++)=*src;
  } while (*(src++)!=0 && --n > 0);
  return(dest);
}


wchar* strcatw(wchar *dest,const wchar *src)
{
  return(strcpyw(dest+strlenw(dest),src));
}


#ifndef SFX_MODULE
wchar* strncatw(wchar *dest,const wchar *src,int n)
{
  dest+=strlenw(dest);
  while (true)
    if (--n<0)
    {
      *dest=0;
      break;
    }
    else
      if ((*(dest++)=*(src++))==0)
        break;
  return(dest);
}
#endif


int strcmpw(const wchar *s1,const wchar *s2)
{
  while (*s1==*s2)
  {
    if (*s1==0)
      return(0);
    s1++;
    s2++;
  }
  return(*s1<*s2 ? -1:1);
}


int strncmpw(const wchar *s1,const wchar *s2,int n)
{
  while (n-->0)
  {
    if (*s1<*s2)
      return(-1);
    if (*s1>*s2)
      return(-1);
    if (*s1==0)
      break;
    s1++;
    s2++;
  }
  return(0);
}


#ifndef SFX_MODULE
int stricmpw(const wchar *s1,const wchar *s2)
{
#ifdef _WIN_32
  if (WinNT())
    return(CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,-1,s2,-1)-2);
  else
  {
    char Ansi1[NM*sizeof(wchar)],Ansi2[NM*sizeof(wchar)];
    WideToChar(s1,Ansi1,sizeof(Ansi1));
    WideToChar(s2,Ansi2,sizeof(Ansi2));
    return(stricomp(Ansi1,Ansi2));
  }
#else
  char Ansi1[NM*sizeof(wchar)],Ansi2[NM*sizeof(wchar)];
  WideToChar(s1,Ansi1,sizeof(Ansi1));
  WideToChar(s2,Ansi2,sizeof(Ansi2));
  return(stricomp(Ansi1,Ansi2));
#endif
}
#endif


#ifndef SFX_MODULE
inline int strnicmpw_w2c(const wchar *s1,const wchar *s2,int n)
{
  wchar Wide1[NM*2],Wide2[NM*2];
  strncpyw(Wide1,s1,sizeof(Wide1)/sizeof(Wide1[0])-1);
  strncpyw(Wide2,s2,sizeof(Wide2)/sizeof(Wide2[0])-1);
  Wide1[Min(sizeof(Wide1)/sizeof(Wide1[0])-1,n)]=0;
  Wide2[Min(sizeof(Wide2)/sizeof(Wide2[0])-1,n)]=0;
  char Ansi1[NM*2],Ansi2[NM*2];
  WideToChar(Wide1,Ansi1,sizeof(Ansi1));
  WideToChar(Wide2,Ansi2,sizeof(Ansi2));
  return(stricomp(Ansi1,Ansi2));
}


int strnicmpw(const wchar *s1,const wchar *s2,int n)
{
#ifdef _WIN_32
  if (WinNT())
  {
    int Length1=Min(strlenw(s1),n);
    int Length2=Min(strlenw(s2),n);
    return(CompareStringW(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,s1,Length1,s2,Length2)-2);
  }
  else
    return(strnicmpw_w2c(s1,s2,n));
#else
  return(strnicmpw_w2c(s1,s2,n));
#endif
}
#endif


wchar* strchrw(const wchar *s,int c)
{
  while (*s)
  {
    if (*s==c)
      return((wchar *)s);
    s++;
  }
  return(NULL);
}


wchar* strrchrw(const wchar *s,int c)
{
  for (int I=strlenw(s)-1;I>=0;I--)
    if (s[I]==c)
      return((wchar *)(s+I));
  return(NULL);
}


wchar* strpbrkw(const wchar *s1,const wchar *s2)
{
  while (*s1)
  {
    if (strchrw(s2,*s1)!=NULL)
      return((wchar *)s1);
    s1++;
  }
  return(NULL);
}


#ifndef SFX_MODULE
wchar* strlowerw(wchar *Str)
{
#ifdef _WIN_32
  if (WinNT())
    CharLowerW(Str);
  else
  {
    char AnsiStr[NM*sizeof(wchar)];
    WideToChar(Str,AnsiStr,sizeof(AnsiStr));
    strlower(AnsiStr);
    CharToWide(AnsiStr,Str);
  }
#else
  for (wchar *ChPtr=Str;*ChPtr;ChPtr++)
    if (*ChPtr<128)
      *ChPtr=loctolower(*ChPtr);
#endif
  return(Str);
}
#endif


#ifndef SFX_MODULE
wchar* strupperw(wchar *Str)
{
#ifdef _WIN_32
  if (WinNT())
    CharUpperW(Str);
  else
  {
    char AnsiStr[NM*sizeof(wchar)];
    WideToChar(Str,AnsiStr,sizeof(AnsiStr));
    strupper(AnsiStr);
    CharToWide(AnsiStr,Str);
  }
#else
  for (wchar *ChPtr=Str;*ChPtr;ChPtr++)
    if (*ChPtr<128)
      *ChPtr=loctoupper(*ChPtr);
#endif
  return(Str);
}
#endif


#ifndef SFX_MODULE
int toupperw(int ch)
{
#ifdef _WIN_32
  if (WinNT())
    return((int)CharUpperW((wchar *)ch));
  else
  {
    char AnsiStr[10];
    wchar WideStr[10];
    WideStr[0]=ch;
    WideStr[1]=0;
    WideToChar(WideStr,AnsiStr,sizeof(AnsiStr));
    if (*AnsiStr=='?')
      return(ch);
    *AnsiStr=loctoupper(*AnsiStr);
    CharToWide(AnsiStr,WideStr);
    return(*WideStr);
  }
#else
  return((ch<128) ? loctoupper(ch):ch);
#endif
}
#endif


int atoiw(const wchar *s)
{
  int n=0;
  while (*s>='0' && *s<='9')
  {
    n=n*10+(*s-'0');
    s++;
  }
  return(n);
}


#ifdef DBCS_SUPPORTED
SupportDBCS gdbcs;

SupportDBCS::SupportDBCS()
{
  CPINFO CPInfo;
  GetCPInfo(CP_ACP,&CPInfo);
  DBCSMode=CPInfo.MaxCharSize > 1;
  for (int I=0;I<sizeof(IsLeadByte)/sizeof(IsLeadByte[0]);I++)
    IsLeadByte[I]=IsDBCSLeadByte(I);
}


char* SupportDBCS::charnext(const char *s)
{
  return (char *)(IsLeadByte[*s] ? s+2:s+1);
}


char* SupportDBCS::strchrd(const char *s, int c)
{
  while (*s!=0)
    if (IsLeadByte[*s])
      s+=2;
    else
      if (*s==c)
        return((char *)s);
      else
        s++;
  return(NULL);
}


char* SupportDBCS::strrchrd(const char *s, int c)
{
  const char *found=NULL;
  while (*s!=0)
    if (IsLeadByte[*s])
      s+=2;
    else
    {
      if (*s==c)
        found=s;
      s++;
    }
  return((char *)found);
}
#endif