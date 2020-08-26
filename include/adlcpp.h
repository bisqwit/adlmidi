#ifndef ADLCPP_H
#define ADLCPP_H

#include <string>

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

#ifndef __DJGPP__
static const unsigned long PCM_RATE = 48000;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
#else // DJGPP
static const unsigned MaxCards = 1;
static const unsigned OPLBase = 0x388;
#endif

class ADL_UserInterface;
class ADL_Input;

extern unsigned AdlBank;
extern unsigned NumFourOps;
extern unsigned NumCards;
extern bool HighTremoloMode;
extern bool HighVibratoMode;
extern bool AdlPercussionMode;
extern bool LogarithmicVolumes;
extern bool CartoonersVolumes;
extern bool QuitFlag, FakeDOSshell;
extern unsigned SkipForward;
extern bool DoingInstrumentTesting;
extern bool QuitWithoutLooping;
extern bool WritePCMfile;
extern std::string PCMfilepath;
#ifdef SUPPORT_VIDEO_OUTPUT
extern std::string VidFilepath;
extern bool WriteVideoFile;
#endif

extern bool ScaleModulators;
extern bool WritingToTTY;

#endif // ADLCPP_H
