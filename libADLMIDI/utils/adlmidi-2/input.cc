#include "input.hpp"

xInput::xInput()
{
#ifdef _WIN32
    inhandle = GetStdHandle(STD_INPUT_HANDLE);
#endif

#ifdef USE_TERMIO
    ioctl(0, TCGETA, &back);
    InputTermio_t term = back;
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0; // 0=no block, 1=do block
    if(ioctl(0, TCSETA, &term) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
}

xInput::~xInput()
{
#ifdef USE_TERMIO
    if(ioctl(0, TCSETA, &back) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~ O_NONBLOCK);
#endif
}

char xInput::PeekInput()
{
#ifdef __DJGPP__
    if(kbhit())
    {
        int c = getch();
        return c ? c : getch();
    }
#endif

#ifdef _WIN32
    DWORD nread = 0;
    INPUT_RECORD inbuf[1];
    while(PeekConsoleInput(inhandle, inbuf, sizeof(inbuf) / sizeof(*inbuf), &nread) && nread)
    {
        ReadConsoleInput(inhandle, inbuf, sizeof(inbuf) / sizeof(*inbuf), &nread);
        if(inbuf[0].EventType == KEY_EVENT
                && inbuf[0].Event.KeyEvent.bKeyDown)
        {
            char c = inbuf[0].Event.KeyEvent.uChar.AsciiChar;
            unsigned s = inbuf[0].Event.KeyEvent.wVirtualScanCode;
            if(c == 0) c = s;
            return c;
        }
    }
#endif

#if (!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    char c = 0;
    if(read(0, &c, 1) == 1) return c;
#endif

    return '\0';
}
