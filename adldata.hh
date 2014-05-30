#ifdef __WIN32__
# include <cctype>
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <mmsystem.h>
#endif

#if defined(__WIN32__) || defined(__DJGPP__)
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned Uint32;
#else
# include <SDL.h>
class MutexType
{
    SDL_mutex* mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType() { SDL_DestroyMutex(mut); }
    void Lock() { SDL_mutexP(mut); }
    void Unlock() { SDL_mutexV(mut); }
};
#endif

extern const struct adldata
{
    Uint32 modulator_E862, carrier_E862;  // See below
    Uint8 modulator_40, carrier_40; // KSL/attenuation settings
    Uint8 feedconn; // Feedback/connection bits for the channel

    signed char finetune;
} adl[];
extern const struct adlinsdata
{
    enum { Flag_Pseudo4op = 0x01 };

    Uint16 adlno1, adlno2;
    Uint8 tone;
    Uint8 flags;
    Uint16 ms_sound_kon;  // Number of milliseconds it produces sound;
    Uint16 ms_sound_koff;
} adlins[];
extern const unsigned short banks[][256];
extern const char* const banknames[65];
