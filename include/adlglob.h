#pragma once

#ifndef ADLGLOBAL_H
#define ADLGLOBAL_H

#include <string>

#ifndef __DJGPP__
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

#endif /* ADLGLOBAL_H */
