#ifndef _RAR_TIMEFN_
#define _RAR_TIMEFN_

void InitTime();
uint SecondsToDosTime(uint Seconds);
void ConvertDate(uint ft,char *DateStr,bool FullYear);
const char *GetMonthName(int Month);
uint TextAgeToSeconds(char *TimeText);
uint IsoTextToDosTime(char *TimeText);
uint UnixTimeToDos(time_t UnixTime);
time_t DosTimeToUnix(uint DosTime);

#ifdef _WIN_32
uint NTTimeToDos(FILETIME *ft);
#endif

void GetCurSysTime(struct tm *T);
bool IsLeapYear(int Year);

#endif
