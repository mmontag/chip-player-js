
#ifndef HAVE_USLEEP

#ifdef HAVE_SELECT

#ifdef HAVE_SELECT_H
#  include <sys/select.h>
#else
#  include <sys/time.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

void usleep(long usec)
{
        struct timeval tv;

        tv.tv_sec = usec / 1000000;
        tv.tv_usec = usec % 1000000;

        select(0, NULL, NULL, NULL, &tv);
}

#elif defined _WIN32_

#include <windows.h>

void usleep(long usec)
{
	LARGE_INTEGER lFrequency;
	LARGE_INTEGER lEndTime;
	LARGE_INTEGER lCurTime;

	QueryPerformanceFrequency(&lFrequency);
	if (lFrequency.QuadPart) {
		QueryPerformanceCounter(&lEndTime);
		lEndTime.QuadPart +=
		    (LONGLONG) usec *lFrequency.QuadPart / 1000000;
		do {
			QueryPerformanceCounter(&lCurTime);
			Sleep(0);
		} while (lCurTime.QuadPart < lEndTime.QuadPart);
	}
}

#else

#endif /* HAVE_SELECT */

#endif /* HAVE_USLEEP */
