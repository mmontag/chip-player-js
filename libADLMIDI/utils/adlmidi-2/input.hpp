#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
# include <cctype>
# define WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX //To don't damage std::min and std::max
# endif
# include <windows.h>
#endif

#ifdef __DJGPP__
#include <conio.h>
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/exceptn.h>
#include <dos.h>
#include <stdlib.h>
#define BIOStimer _farpeekl(_dos_ds, 0x46C)
static const unsigned NewTimerFreq = 209;
#elif !defined(_WIN32) || defined(__CYGWIN__)
# include <termios.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <csignal>
#endif

class xInput
{
    #ifdef _WIN32
    void *inhandle;
    #endif
    #if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__) && !defined(__APPLE__)
    struct termio back;
    #endif
public:
    xInput();
    ~xInput();

    char PeekInput();
};

extern xInput Input;
