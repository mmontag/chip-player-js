#include "rar.hpp"

static int SleepTime=0;

void InitSystemOptions(int SleepTime)
{
  ::SleepTime=SleepTime;
}


#ifndef SFX_MODULE
void SetPriority(int Priority)
{
#ifdef _WIN_32
  uint PriorityClass;
  int PriorityLevel;
  if (Priority<1 || Priority>15)
    return;
  if (Priority==1)
  {
    PriorityClass=IDLE_PRIORITY_CLASS;
    PriorityLevel=THREAD_PRIORITY_IDLE;
  }
  else
    if (Priority<7)
    {
      PriorityClass=IDLE_PRIORITY_CLASS;
      PriorityLevel=Priority-4;
    }
    else
      if (Priority<11)
      {
        PriorityClass=NORMAL_PRIORITY_CLASS;
        PriorityLevel=Priority-9;
      }
      else
      {
        PriorityClass=HIGH_PRIORITY_CLASS;
        PriorityLevel=Priority-13;
      }
  SetPriorityClass(GetCurrentProcess(),PriorityClass);
  SetThreadPriority(GetCurrentThread(),PriorityLevel);
#endif
}
#endif


void Wait()
{
}




