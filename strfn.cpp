#include "rar.hpp"

const char *NullToEmpty(const char *Str)
{
  return(Str==NULL ? "":Str);
}


const wchar *NullToEmpty(const wchar *Str)
{
  return(Str==NULL ? L"":Str);
}


char *IntNameToExt(const char *Name)
{
  static char OutName[NM];
  IntToExt(Name,OutName);
  return(OutName);
}


void ExtToInt(const char *Src,char *Dest)
{
#ifdef _WIN_32
  CharToOem(Src,Dest);
#else
  if (Dest!=Src)
    strcpy(Dest,Src);
#endif
}


void IntToExt(const char *Src,char *Dest)
{
#ifdef _WIN_32
  OemToChar(Src,Dest);
#else
  if (Dest!=Src)
    strcpy(Dest,Src);
#endif
}


char* strlower(char *Str)
{
#ifdef _WIN_32
  CharLower((LPTSTR)Str);
#else
  for (char *ChPtr=Str;*ChPtr;ChPtr++)
    *ChPtr=(char)loctolower(*ChPtr);
#endif
  return(Str);
}


char* strupper(char *Str)
{
#ifdef _WIN_32
  CharUpper((LPTSTR)Str);
#else
  for (char *ChPtr=Str;*ChPtr;ChPtr++)
    *ChPtr=(char)loctoupper(*ChPtr);
#endif
  return(Str);
}


int stricomp(const char *Str1,const char *Str2)
{
#ifdef _WIN_32
  return(CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,Str1,-1,Str2,-1)-2);
#else
  char S1[NM*2],S2[NM*2];
  strncpy(S1,Str1,sizeof(S1));
  strncpy(S2,Str2,sizeof(S2));
  return(strcmp(strupper(S1),strupper(S2)));
#endif
}


int strnicomp(const char *Str1,const char *Str2,int N)
{
#ifdef _WIN_32
  return(CompareString(LOCALE_USER_DEFAULT,NORM_IGNORECASE|SORT_STRINGSORT,Str1,N,Str2,N)-2);
#else
  char S1[512],S2[512];
  strncpy(S1,Str1,sizeof(S1));
  strncpy(S2,Str2,sizeof(S2));
  return(strncmp(strupper(S1),strupper(S2),N));
#endif
}


char* RemoveEOL(char *Str)
{
  for (int I=strlen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n' || Str[I]==' ' || Str[I]=='\t');I--)
    Str[I]=0;
  return(Str);
}


char* RemoveLF(char *Str)
{
  for (int I=strlen(Str)-1;I>=0 && (Str[I]=='\r' || Str[I]=='\n');I--)
    Str[I]=0;
  return(Str);
}


unsigned int loctolower(unsigned int ch)
{
#ifdef _WIN_32
  return((int)CharLower((LPTSTR)ch));
#else
  return(tolower(ch));
#endif
}


unsigned int loctoupper(unsigned int ch)
{
#ifdef _WIN_32
  return((int)CharUpper((LPTSTR)ch));
#else
  return(toupper(ch));
#endif
}





bool LowAscii(const char *Str)
{
  for (int I=0;Str[I]!=0;I++)
    if ((byte)Str[I]<32 || (byte)Str[I]>127)
      return(false);
  return(true);
}


bool LowAscii(const wchar *Str)
{
  for (int I=0;Str[I]!=0;I++)
    if (Str[I]<32 || Str[I]>127)
      return(false);
  return(true);
}
