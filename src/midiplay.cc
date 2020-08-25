//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif

#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <stdarg.h>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#include <ctime>

#include <assert.h>

#include "../include/adlcpp.h"
#include "../include/adlinput.h"
#include "../include/adlui.h"
#include "../include/adlplay.h"


#ifdef __DJGPP__
# include <conio.h>
# include <pc.h>
# include <dpmi.h>
# include <go32.h>
# include <sys/farptr.h>
# include <dos.h>
# define BIOStimer _farpeekl(_dos_ds, 0x46C)
static const unsigned NewTimerFreq = 209;
#elif !defined(__WIN32__) || defined(__CYGWIN__)
# include <termios.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <csignal>
#endif

#include <deque>
#include <algorithm>

#include <signal.h>

#include "../include/fraction.hpp"

#ifndef __DJGPP__
#include "dbopl.h"

#include "adldata.hh"
#endif

unsigned AdlBank    = 0;
unsigned NumFourOps = 7;
unsigned NumCards   = 2;
bool HighTremoloMode   = false;
bool HighVibratoMode   = false;
bool AdlPercussionMode = false;
bool LogarithmicVolumes = false;
bool CartoonersVolumes = false;
bool QuitFlag = false, FakeDOSshell = false;
unsigned SkipForward = 0;
bool DoingInstrumentTesting = false;
bool QuitWithoutLooping = false;
bool WritePCMfile = false;
std::string PCMfilepath = "adlmidi.wav";
#ifdef SUPPORT_VIDEO_OUTPUT
std::string VidFilepath = "adlmidi.mkv";
bool WriteVideoFile = false;
#endif

bool ScaleModulators = false;
bool WritingToTTY;



ADL_Input Input;
ADL_UserInterface UI;


#ifndef __DJGPP__
struct Reverb /* This reverb implementation is based on Freeverb impl. in Sox */
{
    float feedback, hf_damping, gain;
    struct FilterArray
    {
        struct Filter
        {
            std::vector<float> Ptr;  size_t pos;  float Store;
            void Create(size_t size) { Ptr.resize(size); pos = 0; Store = 0.f; }
            float Update(float a, float b)
            {
                Ptr[pos] = a;
                if(!pos) pos = Ptr.size()-1; else --pos;
                return b;
            }
            float ProcessComb(float input, const float feedback, const float hf_damping)
            {
                Store = Ptr[pos] + (Store - Ptr[pos]) * hf_damping;
                return Update(input + feedback * Store, Ptr[pos]);
            }
            float ProcessAllPass(float input)
            {
                return Update(input + Ptr[pos] * .5f, Ptr[pos]-input);
            }
        } comb[8], allpass[4];
        void Create(double rate, double scale, double offset)
        {
            /* Filter delay lengths in samples (44100Hz sample-rate) */
            static const int comb_lengths[8] = {1116,1188,1277,1356,1422,1491,1557,1617};
            static const int allpass_lengths[4] = {225,341,441,556};
            double r = rate * (1 / 44100.0); // Compensate for actual sample-rate
            const int stereo_adjust = 12;
            for(size_t i=0; i<8; ++i, offset=-offset)
                comb[i].Create( scale * r * (comb_lengths[i] + stereo_adjust * offset) + .5 );
            for(size_t i=0; i<4; ++i, offset=-offset)
                allpass[i].Create( r * (allpass_lengths[i] + stereo_adjust * offset) + .5 );
        }
        void Process(size_t length,
            const std::deque<float>& input, std::vector<float>& output,
            const float feedback, const float hf_damping, const float gain)
        {
            for(size_t a=0; a<length; ++a)
            {
                float out = 0, in = input[a];
                for(size_t i=8; i-- > 0; ) out += comb[i].ProcessComb(in, feedback, hf_damping);
                for(size_t i=4; i-- > 0; ) out += allpass[i].ProcessAllPass(out);
                output[a] = out * gain;
            }
        }
    } chan[2];
    std::vector<float> out[2];
    std::deque<float> input_fifo;

    void Create(double sample_rate_Hz,
        float wet_gain_dB,
        float room_scale, float reverberance, float fhf_damping, /* 0..1 */
        float pre_delay_s, float stereo_depth,
        size_t buffer_size)
    {
        size_t delay = pre_delay_s  * sample_rate_Hz + .5;
        double scale = room_scale * .9 + .1;
        double depth = stereo_depth;
        double a =  -1 /  std::log(1 - /**/.3 /**/);          // Set minimum feedback
        double b = 100 / (std::log(1 - /**/.98/**/) * a + 1); // Set maximum feedback
        feedback = 1 - std::exp((reverberance*100.0 - b) / (a * b));
        hf_damping = fhf_damping * .3 + .2;
        gain = std::exp(wet_gain_dB * (std::log(10.0) * 0.05)) * .015;
        input_fifo.insert(input_fifo.end(), delay, 0.f);
        for(size_t i = 0; i <= std::ceil(depth); ++i)
        {
            chan[i].Create(sample_rate_Hz, scale, i * depth);
            out[i].resize(buffer_size);
        }
    }
    void Process(size_t length)
    {
        for(size_t i=0; i<2; ++i)
            if(!out[i].empty())
                chan[i].Process(length,
                    input_fifo,
                    out[i], feedback, hf_damping, gain);
        input_fifo.erase(input_fifo.begin(), input_fifo.begin() + length);
    }
};
static union ReverbSpecs
{
    float array[7];
    struct byname
    {
        float do_reverb    = 1.0f; // boolean...
        float wet_gain_db  = 6.0f; // wet_gain_dB  (-10..10)
        float room_scale   = 0.7f; // room_scale   (0..1)
        float reverberance = 0.6f; // reverberance (0..1)
        float hf_damping   = 0.8f; // hf_damping   (0..1)
        float pre_delay_s  = 0.f;   // pre_delay_s  (0.. 0.5)
        float stereo_depth = 1.f;   // stereo_depth (0..1)
    } byname = {1.f, 6.f, .7f, .6f, .8f, 0.f, 1.f};
} ReverbSpecs;
static struct MyReverbData
{
    float  wetonly;
    Reverb chan[2];

    MyReverbData()
    {
        ReInit();
    }
    void ReInit()
    {
        wetonly = ReverbSpecs.byname.do_reverb;
        for(std::size_t i=0; i<2; ++i)
        {
            chan[i].Create(PCM_RATE,
                ReverbSpecs.byname.wet_gain_db,
                ReverbSpecs.byname.room_scale,
                ReverbSpecs.byname.reverberance,
                ReverbSpecs.byname.hf_damping,
                ReverbSpecs.byname.pre_delay_s,
                ReverbSpecs.byname.stereo_depth,
                MaxSamplesAtTime);
        }
    }
} reverb_data;

static void ParseReverb(std::string_view specs)
{
    while(!specs.empty())
    {
        std::size_t colon = specs.find(':');
        if(colon == 0)
        {
            specs.remove_prefix(1);
            continue;
        }
        if(colon == specs.npos)
        {
            colon = specs.size();
        }
        std::string_view term(&specs[0], colon);
        if(!term.empty())
        {
            std::size_t eq = specs.find('=');
            std::size_t key_end     = (eq == specs.npos) ? specs.size() : eq;
            std::size_t value_begin = (eq == specs.npos) ? specs.size() : (eq+1);
            std::string_view key(&term[0], key_end);
            std::string_view value(&term[value_begin], term.size()-value_begin);
            float floatvalue = std::strtof(&value[0], nullptr); // FIXME: Requires specs to be nul-terminated
            //if(!value.empty())
            //    std::from_chars(&value[0], &value[value.size()], floatvalue); // Not implemented in GCC

            // Create a hash of the key
            //Good: start=0xF4939CE1DDA3,mul=0xE519FDFD45F8,add=0x9770000,perm=0x67EE6A00, mod=17,shift=32   distance = 7854  min=0 max=16
            std::uint_fast64_t a=0xF4939CE1DDA3, b=0x67EE6A00, c=0x9770000, d=0xE519FDFD45F8, result=a + d*key.size();
            unsigned mod=17, shift=32;
            for(unsigned char ch: key)
            {
                result += ch;
                result = (result ^ (result >> (1 + 1 * ((shift>>0) & 7)))) * b;
                result = (result ^ (result >> (1 + 1 * ((shift>>3) & 7)))) * c;
            }
            unsigned index = result % mod;
            // switch(index) {
            // case 0: index = 0; break; // 0
            // case 1: index = 3; break; // reverberance, reverb, amount, f
            // case 2: index = 1; break; // g
            // case 3: index = 2; break; // room_scale, scale, room-scale, roomscale
            // case 4: index = 5; break; // delay, pre_delay, pre-delay, predelay, wait
            // case 5: index = 4; break; // hf, hf_damping, hf-damping, damping, dampening, d
            // case 6: index = 2; break; // r
            // case 7: index = 0; break; // no, n
            // case 8: index = 5; break; // seconds, w
            // case 9: index = 6; break; // stereo
            // case 10: index = 0; break; // none, false
            // case 11: index = 4; break; // damp
            // case 12: index = 2; break; // room
            // case 13: index = 6; break; // depth, stereo_depth, stereo-depth, stereodepth, width, wide, s
            // case 14: index = 0; break; // off
            // case 15: index = 1; break; // gain, wet_gain, wetgain, wet, wet-gain
            // case 16: index = 3; break; // factor
            // }
            index = (031062406502452130 >> (index * 3)) & 7;
            /*if(index < 7)*/ ReverbSpecs.array[index] = floatvalue;
        }
        specs.remove_prefix(colon);
    }
    std::fprintf(stderr, "Reverb settings: Wet-dry=%g gain=%g room=%g reverb=%g damping=%g delay=%g stereo=%g\n",
        ReverbSpecs.array[0],
        ReverbSpecs.byname.wet_gain_db,
        ReverbSpecs.byname.room_scale,
        ReverbSpecs.byname.reverberance,
        ReverbSpecs.byname.hf_damping,
        ReverbSpecs.byname.pre_delay_s,
        ReverbSpecs.byname.stereo_depth);
}

#ifdef __WIN32__
namespace WindowsAudio
{
  static const unsigned BUFFER_COUNT = 16;
  static const unsigned BUFFER_SIZE  = 8192;
  static HWAVEOUT hWaveOut;
  static WAVEHDR headers[BUFFER_COUNT];
  static volatile unsigned buf_read=0, buf_write=0;

  static void CALLBACK Callback(HWAVEOUT,UINT msg,DWORD,DWORD,DWORD)
  {
      if(msg == WOM_DONE)
      {
          buf_read = (buf_read+1) % BUFFER_COUNT;
      }
  }
  static void Open(const int rate, const int channels, const int bits)
  {
      WAVEFORMATEX wformat;
      MMRESULT result;

      //fill waveformatex
      memset(&wformat, 0, sizeof(wformat));
      wformat.nChannels       = channels;
      wformat.nSamplesPerSec  = rate;
      wformat.wFormatTag      = WAVE_FORMAT_PCM;
      wformat.wBitsPerSample  = bits;
      wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
      wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;

      //open sound device
      //WAVE_MAPPER always points to the default wave device on the system
      result = waveOutOpen
      (
        &hWaveOut,WAVE_MAPPER,&wformat,
        (DWORD_PTR)Callback,0,CALLBACK_FUNCTION
      );
      if(result == WAVERR_BADFORMAT)
      {
          fprintf(stderr, "ao_win32: format not supported\n");
          return;
      }
      if(result != MMSYSERR_NOERROR)
      {
          fprintf(stderr, "ao_win32: unable to open wave mapper device\n");
          return;
      }
      char* buffer = new char[BUFFER_COUNT*BUFFER_SIZE];
      std::memset(headers,0,sizeof(headers));
      std::memset(buffer, 0,BUFFER_COUNT*BUFFER_SIZE);
      for(unsigned a=0; a<BUFFER_COUNT; ++a)
          headers[a].lpData = buffer + a*BUFFER_SIZE;
  }
  static void Close()
  {
      waveOutReset(hWaveOut);
      waveOutClose(hWaveOut);
  }
  static void Write(const unsigned char* Buf, unsigned len)
  {
      static std::vector<unsigned char> cache;
      size_t cache_reduction = 0;
      if(0&&len < BUFFER_SIZE&&cache.size()+len<=BUFFER_SIZE)
      {
          cache.insert(cache.end(), Buf, Buf+len);
          Buf = &cache[0];
          len = cache.size();
          if(len < BUFFER_SIZE/2)
              return;
          cache_reduction = cache.size();
      }

      while(len > 0)
      {
          unsigned buf_next = (buf_write+1) % BUFFER_COUNT;
          WAVEHDR* Work = &headers[buf_write];
          while(buf_next == buf_read)
          {
              /*UI.Color(4);
              UI.GotoXY(60,-5+5); fprintf(stderr, "waits\r"); UI.x=0; std::fflush(stderr);
              UI.Color(4);
              UI.GotoXY(60,-4+5); fprintf(stderr, "r%u w%u n%u\r",buf_read,buf_write,buf_next); UI.x=0; std::fflush(stderr);
              */
              /* Wait until at least one of the buffers is free */
              Sleep(0);
              /*UI.Color(2);
              UI.GotoXY(60,-3+5); fprintf(stderr, "wait completed\r"); UI.x=0; std::fflush(stderr);*/
          }

          unsigned npending = (buf_write + BUFFER_COUNT - buf_read) % BUFFER_COUNT;
          static unsigned counter=0, lo=0;
          if(!counter-- || npending < lo) { lo = npending; counter=100; }

          if(!DoingInstrumentTesting)
          {
              if(UI.maxy >= 5) {
                  UI.Color(9);
                  UI.GotoXY(70,-5+6); fprintf(stderr, "%3u bufs\r", (unsigned)npending); UI.x=0; std::fflush(stderr);
                  UI.GotoXY(71,-4+6); fprintf(stderr, "lo:%3u\r", lo); UI.x=0; }
          }

          //unprepare the header if it is prepared
          if(Work->dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
          unsigned x = BUFFER_SIZE; if(x > len) x = len;
          std::memcpy(Work->lpData, Buf, x);
          Buf += x; len -= x;
          //prepare the header and write to device
          Work->dwBufferLength = x;
          {int err=waveOutPrepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
           if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutPrepareHeader: %d\n", err);}
          {int err=waveOutWrite(hWaveOut, Work, sizeof(WAVEHDR));
           if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutWrite: %d\n", err);}
          buf_write = buf_next;
          //if(npending>=BUFFER_COUNT-2)
          //    buf_read=(buf_read+1)%BUFFER_COUNT; // Simulate a read
      }
      if(cache_reduction)
          cache.erase(cache.begin(), cache.begin()+cache_reduction);
  }
}
#else
static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;
static void AdlAudioCallback(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
        target[a] = AudioBuffer[a];
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    //fprintf(stderr, " - remain %u\n", (unsigned) AudioBuffer.size()/2);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}
#endif // WIN32

struct FourChars
{
    char ret[4];

    FourChars(const char* s)
    {
        for(unsigned c=0; c<4; ++c) ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c=0; c<4; ++c) ret[c] = (w >> (c*8)) & 0xFF;
    }
};


static void SendStereoAudio(unsigned long count, int* samples)
{
    if(count > MaxSamplesAtTime)
    {
        SendStereoAudio(MaxSamplesAtTime, samples);
        SendStereoAudio(count-MaxSamplesAtTime, samples+MaxSamplesAtTime);
        return;
    }
#if 0
    if(count % 2 == 1)
    {
        // An uneven number of samples? To avoid complicating matters,
        // just ignore the odd sample.
        count   -= 1;
        samples += 1;
    }
#endif
    if(!count) return;

    // Attempt to filter out the DC component. However, avoid doing
    // sudden changes to the offset, for it can be audible.
    double average[2]={0,0};
    for(unsigned w=0; w<2; ++w)
        for(unsigned long p = 0; p < count; ++p)
            average[w] += samples[p*2+w];
    static float prev_avg_flt[2] = {0,0};
    float average_flt[2] =
    {
        prev_avg_flt[0] = (prev_avg_flt[0] + average[0]*0.04/double(count)) / 1.04,
        prev_avg_flt[1] = (prev_avg_flt[1] + average[1]*0.04/double(count)) / 1.04
    };
    // Figure out the amplitude of both channels
    if(!DoingInstrumentTesting)
    {
        static unsigned amplitude_display_counter = 0;
        if(!amplitude_display_counter--)
        {
            amplitude_display_counter = (PCM_RATE / count) / 24;
            double amp[2]={0,0};
            for(unsigned w=0; w<2; ++w)
            {
                average[w] /= double(count);
                for(unsigned long p = 0; p < count; ++p)
                    amp[w] += std::fabs(samples[p*2+w] - average[w]);
                amp[w] /= double(count);
                // Turn into logarithmic scale
                const double dB = std::log(amp[w]<1 ? 1 : amp[w]) * 4.328085123;
                const double maxdB = 3*16; // = 3 * log2(65536)
                amp[w] = dB/maxdB;
            }
            UI.IllustrateVolumes(amp[0], amp[1]);
        }
    }

    //static unsigned counter = 0; if(++counter < 8000)  return;

#if defined(__WIN32__) && 0
    // Cheat on dosbox recording: easier on the cpu load.
   {count*=2;
    std::vector<short> AudioBuffer(count);
    for(unsigned long p = 0; p < count; ++p)
        AudioBuffer[p] = samples[p];
    WindowsAudio::Write( (const unsigned char*) &AudioBuffer[0], count*2);
    return;}
#endif

    // Convert input to float format
    std::vector<float> dry[2];
    for(unsigned w=0; w<2; ++w)
    {
        dry[w].resize(count);
        float a = average_flt[w];
        for(unsigned long p = 0; p < count; ++p)
        {
            int   s = samples[p*2+w];
            dry[w][p] = (s - a) * double(0.3/32768.0);
        }
        // ^  Note: ftree-vectorize causes an error in this loop on g++-4.4.5
        reverb_data.chan[w].input_fifo.insert(
        reverb_data.chan[w].input_fifo.end(),
            dry[w].begin(), dry[w].end());
    }
    // Reverbify it
    for(unsigned w=0; w<2; ++w)
        reverb_data.chan[w].Process(count);

    // Convert to signed 16-bit int format and put to playback queue
#ifdef __WIN32__
    std::vector<short> AudioBuffer(count*2);
    const size_t pos = 0;
#else
    AudioBuffer_lock.Lock();
    size_t pos = AudioBuffer.size();
    AudioBuffer.resize(pos + count*2);
#endif
    for(unsigned long p = 0; p < count; ++p)
        for(unsigned w=0; w<2; ++w)
        {
            float out = ((1 - reverb_data.wetonly) * dry[w][p] +
                          reverb_data.wetonly * (
                .5 * (reverb_data.chan[0].out[w][p]
                    + reverb_data.chan[1].out[w][p]))
                        ) * 32768.0f
                 + average_flt[w];
            AudioBuffer[pos+p*2+w] =
                out<-32768.f ? -32768 :
                out>32767.f ?  32767 : out;
        }
    if(WritePCMfile)
    {
        /* HACK: Cheat on DOSBox recording: Record audio separately on Windows. */
        static FILE* fp = nullptr;
        if(!fp)
        {
            fp = PCMfilepath == "-" ? stdout
                                    : fopen(PCMfilepath.c_str(), "wb");
            if(fp)
            {
                FourChars Bufs[] = {
                    "RIFF", (0x24u),  // RIFF type, file length - 8
                    "WAVE",           // WAVE file
                    "fmt ", (0x10u),  // fmt subchunk, which is 16 bytes:
                      "\1\0\2\0",     // PCM (1) & stereo (2)
                      (48000u    ), // sampling rate
                      (48000u*2*2), // byte rate
                      "\2\0\20\0",    // block align & bits per sample
                    "data", (0x00u)  //  data subchunk, which is so far 0 bytes.
                };
                for(unsigned c=0; c<sizeof(Bufs)/sizeof(*Bufs); ++c)
                    std::fwrite(Bufs[c].ret, 1, 4, fp);
            }
        }

        // Using a loop, because our data type is a deque, and
        // the data might not be contiguously stored in memory.
        for(unsigned long p = 0; p < 2*count; ++p)
            std::fwrite(&AudioBuffer[pos+p], 1, 2, fp);

        /* Update the WAV header */
        if(true)
        {
            long pos = std::ftell(fp);
            if(pos != -1)
            {
                long datasize = pos - 0x2C;
                if(std::fseek(fp, 4,  SEEK_SET) == 0) // Patch the RIFF length
                    std::fwrite( FourChars(0x24u+datasize).ret, 1,4, fp);
                if(std::fseek(fp, 40, SEEK_SET) == 0) // Patch the data length
                    std::fwrite( FourChars(datasize).ret, 1,4, fp);
                std::fseek(fp, pos, SEEK_SET);
            }
        }

        std::fflush(fp);

        //if(std::ftell(fp) >= 48000*4*10*60)
        //    raise(SIGINT);
    }
#ifdef SUPPORT_VIDEO_OUTPUT
    if(WriteVideoFile)
    {
        static constexpr unsigned framerate = 15;
        static FILE* fp = nullptr;
        static unsigned long samples_carry = 0;

        if(!fp)
        {
            std::string cmdline =
                "ffmpeg -f rawvideo"
                " -pixel_format bgra "
                " -video_size " + std::to_string(UI.VidWidth) + "x" + std::to_string(UI.VidHeight) +
                " -framerate " + std::to_string(framerate) +
                " -i -"
                " -c:v h264"
                " -aspect " + std::to_string(UI.VidWidth) + "/" + std::to_string(UI.VidHeight) +
                " -pix_fmt yuv420p"
                " -preset superfast -partitions all -refs 2 -tune animation -y '" + VidFilepath + "'"; // FIXME: escape filename
            cmdline += " >/dev/null 2>/dev/null";
            fp = popen(cmdline.c_str(), "w");
        }
        if(fp)
        {
            samples_carry += count;
            while(samples_carry >= PCM_RATE / framerate)
            {
                UI.VidRender();

                const unsigned char* source = (const unsigned char*)&UI.PixelBuffer;
                std::size_t bytes_remain    = sizeof(UI.PixelBuffer);
                while(bytes_remain)
                {
                    int r = std::fwrite(source, 1, bytes_remain, fp);
                    if(r == 0) break;
                    bytes_remain -= r;
                    source       += r;
                }
                samples_carry -= PCM_RATE / framerate;
            }
        }
    }
#endif
#ifndef __WIN32__
    AudioBuffer_lock.Unlock();
#else
    if(!WritePCMfile)
        WindowsAudio::Write( (const unsigned char*) &AudioBuffer[0], 2*AudioBuffer.size());
#endif
}
#endif /* not DJGPP */


static void TidyupAndExit(int)
{
    UI.ShowCursor();
    UI.Color(7);
    std::fflush(stderr);
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

#ifdef __WIN32__
/* Parse a command line buffer into arguments */
static void UnEscapeQuotes( char *arg )
{
    for(char *last=0; *arg != '\0'; last=arg++)
        if( *arg == '"' && *last == '\\' ) {
            char *c_last = last;
            for(char*c_curr=arg; *c_curr; ++c_curr) {
                *c_last = *c_curr;
                c_last = c_curr;
            }
            *c_last = '\0';
        }
}
static int ParseCommandLine(char *cmdline, char **argv)
{
    char *bufp, *lastp=NULL;
    int argc=0, last_argc=0;
    for (bufp = cmdline; *bufp; ) {
        /* Skip leading whitespace */
        while ( std::isspace(*bufp) ) ++bufp;
        /* Skip over argument */
        if ( *bufp == '"' ) {
            ++bufp;
            if ( *bufp ) { if (argv) argv[argc]=bufp; ++argc; }
            /* Skip over word */
            while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
                lastp = bufp;
                ++bufp;
            }
        } else {
            if ( *bufp ) { if (argv) argv[argc] = bufp; ++argc; }
            /* Skip over word */
            while ( *bufp && ! std::isspace(*bufp) ) ++bufp;
        }
        if(*bufp) { if(argv) *bufp = '\0'; ++bufp; }
        /* Strip out \ from \" sequences */
        if( argv && last_argc != argc ) UnEscapeQuotes( argv[last_argc]);
        last_argc = argc;
    }
    if(argv) argv[argc] = 0;
    return(argc);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    extern int main(int,char**);
    char* cmdline = GetCommandLineA();
    int argc = ParseCommandLine(cmdline, NULL);
    char**argv = new char* [argc+1];
    ParseCommandLine(cmdline, argv);
#else
#undef main

int main(int argc, char** argv)
{
#endif
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often AdlAudioCallBack()
    // is called.
    const double AudioBufferLength = 0.045;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.

    WritingToTTY = isatty(STDOUT_FILENO);
    if (WritingToTTY)
    {
        UI.Print(0, 15,true,
#ifdef __DJGPP__
            "ADLMIDI_A: MIDI player for OPL3 hardware"
#else
            "ADLMIDI: MIDI (etc.) player with OPL3 emulation"
#endif
        );
    }
    if (WritingToTTY)
    {
        UI.Print(0,3, true, "(C) -- http://iki.fi/bisqwit/source/adlmidi.html");
    }
    UI.Color(7); std::fflush(stderr);

    signal(SIGINT, TidyupAndExit);

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        UI.Color(7);  std::fflush(stderr);
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "       adlmidi <midifilename> -1   To enter instrument tester\n"
            " -p              Enables adlib percussion instrument mode (use with CMF files)\n"
            " -t              Enables tremolo amplification mode\n"
            " -v              Enables vibrato amplification mode\n"
            " -s              Enables scaling of modulator volumes\n"
            " -nl             Quit without looping\n"
#ifndef __DJGPP__
            " -reverb <specs> Controls reverb (default: gain=6:room=.7:factor=.6:damping=.8:predelay=0:stereo=1)\n"
            " -reverb none    Disables reverb (also -nr)\n"
#endif
            " -w [<filename>] Write WAV file rather than playing\n"
#ifdef SUPPORT_VIDEO_OUTPUT
            " -d [<filename>] Write video file using ffmpeg\n"
#endif
        );
        for(unsigned a=0; a<sizeof(banknames)/sizeof(*banknames); ++a)
            std::printf("%10s%2u = %s\n",
                a?"":"Banks:",
                a,
                banknames[a]);
        std::printf(
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     <numfourops> can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
            "     When playing Creative Music Files (CMF), try the\n"
            "     -p and -v options if it sounds wrong otherwise.\n"
            "\n"
            );
        UI.ShowCursor();
        return 0;
    }

    std::srand(std::time(0));

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            AdlPercussionMode = true;
        else if(!std::strcmp("-v", argv[2]))
            HighVibratoMode = true;
        else if(!std::strcmp("-t", argv[2]))
            HighTremoloMode = true;
        else if(!std::strcmp("-nl", argv[2]))
            QuitWithoutLooping = true;
#ifndef __DJGPP__
        else if(!std::strcmp("-nr", argv[2]))
            ParseReverb("none");
        else if(!std::strcmp("-reverb", argv[2]))
        {
            ParseReverb(argv[3]);
            had_option = true;
        }
#endif
        else if(!std::strcmp("-w", argv[2]))
        {
            WritePCMfile = true;
            if (argc > 3 && argv[3][0] != '\0' && (argv[3][0] != '-' || argv[3][1] == '\0'))
            {
                // Allow the option argument if
                // - it's not empty, and...
                // - it does not begin with "-" or it is "-"
                // - it is not a positive integer
                char* endptr = 0;
                if(std::strtol(argv[3], &endptr, 10) < 0 || (endptr && *endptr))
                {
                    PCMfilepath = argv[3];
                    had_option  = true;
                }
            }
        }
        else if(!std::strcmp("-d", argv[2]))
        {
#ifdef SUPPORT_VIDEO_OUTPUT
            WriteVideoFile = true;
            if (argc > 3 && argv[3][0] != '\0' && (argv[3][0] != '-' || argv[3][1] == '\0'))
            {
                char* endptr = 0;
                if(std::strtol(argv[3], &endptr, 10) < 0 || (endptr && *endptr))
                {
                    VidFilepath = argv[3];
                    had_option  = true;
                }
            }
#else
            std::fprintf(stderr, "Video output not enabled at compilation time; -d option ignored\n");
#endif
        }
        else if(!std::strcmp("-s", argv[2]))
            ScaleModulators = true;
        else break;

        std::copy(argv + (had_option ? 4 : 3), argv + argc,
                  argv+2);
        argc -= (had_option ? 2 : 1);
    }

#ifndef __DJGPP__

#ifndef __WIN32__
    static SDL_AudioSpec spec, obtained;
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq * AudioBufferLength;
    spec.callback = AdlAudioCallback;
    if (!WritePCMfile)
    {
        // Set up SDL
        if(SDL_OpenAudio(&spec, &obtained) < 0)
        {
            std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
            //return 1;
        }
        if(spec.samples != obtained.samples)
            std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
                spec.samples,    spec.freq,    spec.channels,
                obtained.samples,obtained.freq,obtained.channels);
    }
#endif

#endif /* not DJGPP */

    if(argc >= 3)
    {
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);
        int bankno = std::atoi(argv[2]);
        if(bankno == -1)
        {
            bankno = 0;
            DoingInstrumentTesting = true;
        }
        AdlBank = bankno;
        if(AdlBank >= NumBanks)
        {
            std::fprintf(stderr, "bank number may only be 0..%u.\n", NumBanks-1);
            UI.ShowCursor();
            return 0;
        }
        if(WritingToTTY)
            UI.PrintLn("FM instrument bank %u selected.", AdlBank);
    }

    unsigned n_fourop[2] = {0,0}, n_total[2] = {0,0};
    for(unsigned a=0; a<256; ++a)
    {
        unsigned insno = banks[AdlBank][a];
        if(insno == 198) continue;
        ++n_total[a/128];
        if(adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a/128];
    }
    if (WritingToTTY)
    {
        UI.PrintLn("This bank has %u/%u four-op melodic instruments", n_fourop[0], n_total[0]);
        UI.PrintLn("          and %u/%u percussive ones.", n_fourop[1], n_total[1]);
    }

    if(argc >= 4)
    {
        NumCards = std::atoi(argv[3]);
        if(NumCards < 1 || NumCards > MaxCards)
        {
            std::fprintf(stderr, "number of cards may only be 1..%u.\n", MaxCards);
            UI.ShowCursor();
            return 0;
        }
    }
    if(argc >= 5)
    {
        NumFourOps = std::atoi(argv[4]);
        if(NumFourOps > 6 * NumCards)
        {
            std::fprintf(stderr, "number of four-op channels may only be 0..%u when %u OPL3 cards are used.\n",
                6*NumCards, NumCards);
            UI.ShowCursor();
            return 0;
        }
    }
    else
        NumFourOps =
            DoingInstrumentTesting ? 2
          : (n_fourop[0] >= n_total[0]*7/8) ? NumCards * 6
          : (n_fourop[0] < n_total[0]*1/8) ? 0
          : (NumCards==1 ? 1 : NumCards*4);
    if (WritingToTTY)
    {
        UI.PrintLn("Simulating %u OPL3 cards for a total of %u operators.", NumCards, NumCards*36);
        std::string s = "Operator set-up: "
                       + std::to_string(NumFourOps)
                       + " 4op, "
                       + std::to_string((AdlPercussionMode ? 15 : 18) * NumCards - NumFourOps*2)
                       + " 2op";
        if(AdlPercussionMode)
            s += ", " + std::to_string(NumCards * 5) + " percussion";
        s += " channels";
        UI.PrintLn("%s", s.c_str());
    }

    MIDIplay player;
    player.ChooseDevice("");

    UI.Color(7);
    if(!player.LoadMIDI(argv[1]))
    {
        UI.ShowCursor();
        return 2;
    }

    if(n_fourop[0] >= n_total[0]*15/16 && NumFourOps == 0)
    {
        std::fprintf(stderr,
            "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n");
        return 0;
    }

#ifdef __DJGPP__

    unsigned TimerPeriod = 0x1234DDul / NewTimerFreq;
    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, TimerPeriod & 0xFF);
    outportb(0x40, TimerPeriod >>   8);
    //enable();
    unsigned long BIOStimer_begin = BIOStimer;

#else

    const double mindelay = 1 / (double)PCM_RATE;
    const double maxdelay = MaxSamplesAtTime / (double)PCM_RATE;
    reverb_data.ReInit();

#ifdef __WIN32
    WindowsAudio::Open(PCM_RATE, 2, 16);
#else
    SDL_PauseAudio(0);
#endif

#endif /* djgpp */

    Tester InstrumentTester(player.opl);

    UI.TetrisLaunched = true;
    for(double delay=0; !QuitFlag; )
    {
    #ifndef __DJGPP__
        const double eat_delay = delay < maxdelay ? delay : maxdelay;
        delay -= eat_delay;

        static double carry = 0.0;
        carry += PCM_RATE * eat_delay;
        const unsigned long n_samples = (unsigned) carry;
        carry -= n_samples;

        if(SkipForward > 0)
            SkipForward -= 1;
        else
        {
            if(NumCards == 1)
            {
                player.Generate(0, 0, SendStereoAudio, n_samples);
            }
            else if(n_samples > 0)
            {
                /* Mix together the audio from different cards */
                static std::vector<int> sample_buf;
                sample_buf.clear();
                sample_buf.resize(n_samples*2);
                struct Mix
                {
                    static void AddStereoAudio(unsigned long count, int* samples)
                    {
                        for(unsigned long a=0; a<count*2; ++a)
                            sample_buf[a] += samples[a];
                    }
                };
                for(unsigned card = 0; card < NumCards; ++card)
                {
                    player.Generate(card,
                        0,
                        Mix::AddStereoAudio,
                        n_samples);
                }
                /* Process it */
                SendStereoAudio(n_samples, &sample_buf[0]);
            }

            //fprintf(stderr, "Enter: %u (%.2f ms)\n", (unsigned)AudioBuffer.size(),
            //    AudioBuffer.size() * .5e3 / obtained.freq);
        #ifndef __WIN32__
            const SDL_AudioSpec& spec_ = (WritePCMfile ? spec : obtained);
            for(unsigned grant=0; AudioBuffer.size() > spec_.samples + (spec_.freq*2) * OurHeadRoomLength; ++grant)
            {
                if(!WritePCMfile)
                {
                    if(UI.CheckTetris() || grant%4==0)
                    {
                        SDL_Delay(1); // std::min(10.0, 1e3 * eat_delay) );
                    }
                }
                else
                {
                    for(unsigned n=0; n<128; ++n) UI.CheckTetris();
                    AudioBuffer_lock.Lock();
                    AudioBuffer.clear();
                    AudioBuffer_lock.Unlock();
                }
            }
        #else
            //Sleep(1e3 * eat_delay);
        #endif
            //fprintf(stderr, "Exit: %u\n", (unsigned)AudioBuffer.size());
        }
    #else /* DJGPP */
        UI.IllustrateVolumes(0,0);
        const double mindelay = 1.0 / NewTimerFreq;

        //__asm__ volatile("sti\nhlt");
        //usleep(10000);
        __dpmi_yield();

        static unsigned long PrevTimer = BIOStimer;
        const unsigned long CurTimer = BIOStimer;
        const double eat_delay = (CurTimer - PrevTimer) / (double)NewTimerFreq;
        PrevTimer = CurTimer;
    #endif

        double nextdelay =
            DoingInstrumentTesting
            ? InstrumentTester.Tick(eat_delay, mindelay)
            : player.Tick(eat_delay, mindelay);

        UI.GotoXY(0,0);
        UI.ShowCursor();

        delay = nextdelay;
    }

#ifdef __DJGPP__

    // Fix the skewed clock and reset BIOS tick rate
    _farpokel(_dos_ds, 0x46C, BIOStimer_begin +
        (BIOStimer - BIOStimer_begin)
        * (0x1234DD/65536.0) / NewTimerFreq );
    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, 0);
    outportb(0x40, 0);
    //enable();

#else

#ifdef __WIN32__
    WindowsAudio::Close();
#else
    SDL_CloseAudio();
#endif

#endif /* djgpp */
    if(FakeDOSshell)
    {
        fprintf(stderr,
            "Going TSR. Type 'EXIT' to return to ADLMIDI.\n"
            "\n"
          /*"Megasoft(R) Orifices 98\n"
            "    (C)Copyright Megasoft Corp 1981 - 1999.\n"*/
            ""
        );
    }
    return 0;
}
