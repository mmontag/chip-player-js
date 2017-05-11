#include "rar.hpp"

static bool match(char *pattern,char *string,bool ForceCase);
static bool match(wchar *pattern,wchar *string,bool ForceCase);

static int mstricompc(const char *Str1,const char *Str2,bool ForceCase);
static int mstricompcw(const wchar *Str1,const wchar *Str2,bool ForceCase);
static int mstrnicompc(const char *Str1,const char *Str2,size_t N,bool ForceCase);
static int mstrnicompcw(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase);

inline uint toupperc(byte ch,bool ForceCase)
{
  if (ForceCase)
    return(ch);
#ifdef _WIN_32
  return((uint)(LPARAM)CharUpper((LPTSTR)(ch)));
#elif defined(_UNIX)
  return(ch);
#else
  return(toupper(ch));
#endif
}


inline uint touppercw(uint ch,bool ForceCase)
{
  if (ForceCase)
    return(ch);
#if defined(_UNIX)
  return(ch);
#else
  return(toupperw(ch));
#endif
}


bool CmpName(char *Wildcard,char *Name,int CmpMode)
{
  bool ForceCase=(CmpMode&MATCH_FORCECASESENSITIVE)!=0;

  CmpMode&=MATCH_MODEMASK;
  
  if (CmpMode!=MATCH_NAMES)
  {
    size_t WildLength=strlen(Wildcard);
    if (CmpMode!=MATCH_EXACT && CmpMode!=MATCH_EXACTPATH && 
        mstrnicompc(Wildcard,Name,WildLength,ForceCase)==0)
    {
      // For all modes except MATCH_NAMES, MATCH_EXACT and MATCH_EXACTPATH
      // "path1" mask must match "path1\path2\filename.ext" and "path1" names.
      char NextCh=Name[WildLength];
      if (NextCh=='\\' || NextCh=='/' || NextCh==0)
        return(true);

      // Nothing more to compare for MATCH_SUBPATHONLY.
      if (CmpMode==MATCH_SUBPATHONLY)
        return(false);
    }
    char Path1[NM],Path2[NM];
    GetFilePath(Wildcard,Path1,ASIZE(Path1));
    GetFilePath(Name,Path2,ASIZE(Path1));

    if ((CmpMode==MATCH_EXACT || CmpMode==MATCH_EXACTPATH) &&
        mstricompc(Path1,Path2,ForceCase)!=0)
      return(false);
    if (CmpMode==MATCH_SUBPATH || CmpMode==MATCH_WILDSUBPATH)
      if (IsWildcard(Path1))
        return(match(Wildcard,Name,ForceCase));
      else
        if (CmpMode==MATCH_SUBPATH || IsWildcard(Wildcard))
        {
          if (*Path1 && mstrnicompc(Path1,Path2,strlen(Path1),ForceCase)!=0)
            return(false);
        }
        else
          if (mstricompc(Path1,Path2,ForceCase)!=0)
            return(false);
  }
  char *Name1=PointToName(Wildcard);
  char *Name2=PointToName(Name);

  // Always return false for RAR temporary files to exclude them
  // from archiving operations.
  if (mstrnicompc("__rar_",Name2,6,false)==0)
    return(false);

  if (CmpMode==MATCH_EXACT)
    return(mstricompc(Name1,Name2,ForceCase)==0);
  
  return(match(Name1,Name2,ForceCase));
}


#ifndef SFX_MODULE
bool CmpName(wchar *Wildcard,wchar *Name,int CmpMode)
{
  bool ForceCase=(CmpMode&MATCH_FORCECASESENSITIVE)!=0;

  CmpMode&=MATCH_MODEMASK;

  if (CmpMode!=MATCH_NAMES)
  {
    size_t WildLength=strlenw(Wildcard);
    if (CmpMode!=MATCH_EXACT && CmpMode!=MATCH_EXACTPATH &&
        mstrnicompcw(Wildcard,Name,WildLength,ForceCase)==0)
    {
      // For all modes except MATCH_NAMES, MATCH_EXACT and MATCH_EXACTPATH
      // "path1" mask must match "path1\path2\filename.ext" and "path1" names.
      wchar NextCh=Name[WildLength];
      if (NextCh==L'\\' || NextCh==L'/' || NextCh==0)
        return(true);

      // Nothing more to compare for MATCH_SUBPATHONLY.
      if (CmpMode==MATCH_SUBPATHONLY)
        return(false);
    }
    wchar Path1[NM],Path2[NM];
    GetFilePath(Wildcard,Path1,ASIZE(Path1));
    GetFilePath(Name,Path2,ASIZE(Path2));

    if ((CmpMode==MATCH_EXACT || CmpMode==MATCH_EXACTPATH) &&
        mstricompcw(Path1,Path2,ForceCase)!=0)
      return(false);
    if (CmpMode==MATCH_SUBPATH || CmpMode==MATCH_WILDSUBPATH)
      if (IsWildcard(NULL,Path1))
        return(match(Wildcard,Name,ForceCase));
      else
        if (CmpMode==MATCH_SUBPATH || IsWildcard(NULL,Wildcard))
        {
          if (*Path1 && mstrnicompcw(Path1,Path2,strlenw(Path1),ForceCase)!=0)
            return(false);
        }
        else
          if (mstricompcw(Path1,Path2,ForceCase)!=0)
            return(false);
  }
  wchar *Name1=PointToName(Wildcard);
  wchar *Name2=PointToName(Name);

  // Always return false for RAR temporary files to exclude them
  // from archiving operations.
  if (mstrnicompcw(L"__rar_",Name2,6,false)==0)
    return(false);

  if (CmpMode==MATCH_EXACT)
    return(mstricompcw(Name1,Name2,ForceCase)==0);

  return(match(Name1,Name2,ForceCase));
}
#endif


bool match(char *pattern,char *string,bool ForceCase)
{
  for (;; ++string)
  {
    char stringc=toupperc(*string,ForceCase);
    char patternc=toupperc(*pattern++,ForceCase);
    switch (patternc)
    {
      case 0:
        return(stringc==0);
      case '?':
        if (stringc == 0)
          return(false);
        break;
      case '*':
        if (*pattern==0)
          return(true);
        if (*pattern=='.')
        {
          if (pattern[1]=='*' && pattern[2]==0)
            return(true);
          char *dot=strchr(string,'.');
          if (pattern[1]==0)
            return (dot==NULL || dot[1]==0);
          if (dot!=NULL)
          {
            string=dot;
            if (strpbrk(pattern,"*?")==NULL && strchr(string+1,'.')==NULL)
              return(mstricompc(pattern+1,string+1,ForceCase)==0);
          }
        }

        while (*string)
          if (match(pattern,string++,ForceCase))
            return(true);
        return(false);
      default:
        if (patternc != stringc)
        {
          // Allow "name." mask match "name" and "name.\" match "name\".
          if (patternc=='.' && (stringc==0 || stringc=='\\' || stringc=='.'))
            return(match(pattern,string,ForceCase));
          else
            return(false);
        }
        break;
    }
  }
}


#ifndef SFX_MODULE
bool match(wchar *pattern,wchar *string,bool ForceCase)
{
  for (;; ++string)
  {
    wchar stringc=touppercw(*string,ForceCase);
    wchar patternc=touppercw(*pattern++,ForceCase);
    switch (patternc)
    {
      case 0:
        return(stringc==0);
      case '?':
        if (stringc == 0)
          return(false);
        break;
      case '*':
        if (*pattern==0)
          return(true);
        if (*pattern=='.')
        {
          if (pattern[1]=='*' && pattern[2]==0)
            return(true);
          wchar *dot=strchrw(string,'.');
          if (pattern[1]==0)
            return (dot==NULL || dot[1]==0);
          if (dot!=NULL)
          {
            string=dot;
            if (strpbrkw(pattern,L"*?")==NULL && strchrw(string+1,'.')==NULL)
              return(mstricompcw(pattern+1,string+1,ForceCase)==0);
          }
        }

        while (*string)
          if (match(pattern,string++,ForceCase))
            return(true);
        return(false);
      default:
        if (patternc != stringc)
        {
          // Allow "name." mask match "name" and "name.\" match "name\".
          if (patternc=='.' && (stringc==0 || stringc=='\\' || stringc=='.'))
            return(match(pattern,string,ForceCase));
          else
            return(false);
        }
        break;
    }
  }
}
#endif


int mstricompc(const char *Str1,const char *Str2,bool ForceCase)
{
  if (ForceCase)
    return(strcmp(Str1,Str2));
  return(stricompc(Str1,Str2));
}


#ifndef SFX_MODULE
int mstricompcw(const wchar *Str1,const wchar *Str2,bool ForceCase)
{
  if (ForceCase)
    return(strcmpw(Str1,Str2));
  return(stricompcw(Str1,Str2));
}
#endif


int mstrnicompc(const char *Str1,const char *Str2,size_t N,bool ForceCase)
{
  if (ForceCase)
    return(strncmp(Str1,Str2,N));
#if defined(_UNIX)
  return(strncmp(Str1,Str2,N));
#else
  return(strnicomp(Str1,Str2,N));
#endif
}


#ifndef SFX_MODULE
int mstrnicompcw(const wchar *Str1,const wchar *Str2,size_t N,bool ForceCase)
{
  if (ForceCase)
    return(strncmpw(Str1,Str2,N));
#if defined(_UNIX)
  return(strncmpw(Str1,Str2,N));
#else
  return(strnicmpw(Str1,Str2,N));
#endif
}
#endif
