#ifndef ADLPRIV_HH
#define ADLPRIV_HH

#include "adlinput.h"
#include "adlui.h"

// Setup compiler defines useful for exporting required public API symbols in gme.cpp
#ifndef ADLMIDI_EXPORT
#   if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#       define ADLMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBADLMIDI_VISIBILITY) && defined (__GNUC__)
#       define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define ADLMIDI_EXPORT
#   endif
#endif

#endif // ADLPRIV_HH
