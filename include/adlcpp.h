#ifndef ADLCPP_H
#define ADLCPP_H

#ifdef ADLMIDI_BUILD
#   ifndef ADLMIDI_DECLSPEC
#       if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#           define ADLMIDI_DECLSPEC __declspec(dllexport)
#       else
#           define ADLMIDI_DECLSPEC
#       endif
#   endif
#else
#   define ADLMIDI_DECLSPEC
#endif


#endif // ADLCPP_H
