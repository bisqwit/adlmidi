
#ifdef __DJGPP__
# include <conio.h>
#elif !defined(__WIN32__) || defined(__CYGWIN__)
# include <unistd.h>
# include <fcntl.h>
#endif

#ifdef __WIN32__
# include <cctype>
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#include "../include/adlinput.h"
#include "adlpriv.hh"

ADLMIDI_EXPORT char ADL_Input::PeekInput()
{
#ifdef __DJGPP__
    if(kbhit()) { int c = getch(); return c ? c : getch(); }
#endif
#ifdef __WIN32__
    DWORD nread=0;
    INPUT_RECORD inbuf[1];
    while(PeekConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread)&&nread)
    {
        ReadConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread);
        if(inbuf[0].EventType==KEY_EVENT
                && inbuf[0].Event.KeyEvent.bKeyDown)
        {
            char c = inbuf[0].Event.KeyEvent.uChar.AsciiChar;
            unsigned s = inbuf[0].Event.KeyEvent.wVirtualScanCode;
            if(c == 0) c = s;
            return c;
        }   }
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    char c = 0;
    if(read(0, &c, 1) == 1) return c;
#endif
    return '\0';
}

ADLMIDI_EXPORT ADL_Input::~ADL_Input()
{
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    if(ioctl(0, TCSETA, &back) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) &~ O_NONBLOCK);
#endif
}

ADLMIDI_EXPORT ADL_Input::ADL_Input()
{
#ifdef __WIN32__
    inhandle = GetStdHandle(STD_INPUT_HANDLE);
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    ioctl(0, TCGETA, &back);
    struct termio term = back;
    term.c_lflag &= ~(ICANON|ECHO);
    term.c_cc[VMIN] = 0; // 0=no block, 1=do block
    if(ioctl(0, TCSETA, &term) < 0)
        fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
}
