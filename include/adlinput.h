#pragma once

#ifndef ADLINPUT_H
#define ADLINPUT_H

#if !defined(__WIN32__) || defined(__CYGWIN__)
# include <termios.h>
# include <sys/ioctl.h>
#endif

#ifndef ADLCPP_H
#include "adlcpp.h"
#endif

#ifndef ADLGLOBAL_H
#include "adlglob.h"
#endif

class ADLMIDI_DECLSPEC ADL_Input
{
#ifdef __WIN32__
    void* inhandle;
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    struct termio back;
#endif
public:
    ADL_Input();
    ~ADL_Input();
    char PeekInput();
};

#endif // ADLINPUT_H
