#include <vector>
#include <string>
#include <map>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <stdarg.h>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil

#include <SDL.h>
#include <deque>

#ifndef __MINGW32__
# include <pthread.h>
#endif

#include "dbopl.h"

static const unsigned MaxSamplesAtTime = 512; // dbopl limitation

static unsigned AdlBank = 0;

extern const struct adldata
{
    unsigned carrier_E862, modulator_E862;  // See below
    unsigned char carrier_40, modulator_40; // KSL/attenuation settings
    unsigned char feedconn; // Feedback/connection bits for the channel
    char symbol;            // Drawing symbol
} adl[];
extern const struct adlinsdata
{
    unsigned short adlno;
    unsigned char tone;
    unsigned char bit27, byte17;
} adlins[];
extern const unsigned short banks[][256];

static void AddMonoAudio(unsigned long count, int* samples);
static void AddStereoAudio(unsigned long count, int* samples);

static const unsigned short Operators[18] =
    {0x000,0x001,0x002, 0x008,0x009,0x00A, 0x010,0x011,0x012,
     0x100,0x101,0x102, 0x108,0x109,0x10A, 0x110,0x111,0x112 };
static const unsigned short Channels[18] =
    {0x000,0x001,0x002, 0x003,0x004,0x005, 0x006,0x007,0x008,
     0x100,0x101,0x102, 0x103,0x104,0x105, 0x106,0x107,0x108 };
struct OPL3
{
    static const unsigned long PCM_RATE = 48000;
    unsigned short ins[18], pit[18];
    DBOPL::Handler dbopl_handler;

    void Poke(unsigned index, unsigned value)
    {
        dbopl_handler.WriteReg(index, value);
    }
    void ProduceAudio(double seconds)
    {
        static double carry = 0.0;
        carry += PCM_RATE * seconds;

        const unsigned long n_samples = (unsigned)carry;
        carry -= n_samples;

        dbopl_handler.Generate(AddMonoAudio, AddStereoAudio, n_samples);
    }
    void NoteOff(unsigned c)
    {
        Poke(0xB0 + Channels[c], pit[c] & 0xDF);
    }
    void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
    {
        unsigned x = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x += 0x400; } // Calculate octave
        x += (int)(hertz + 0.5);
        Poke(0xA0 + Channels[c], x & 0xFF);
        Poke(0xB0 + Channels[c], pit[c] = x >> 8);
    }
    void Touch_Real(unsigned c, unsigned volume)
    {
        unsigned i = ins[c], x = adl[i].carrier_40, y = adl[i].modulator_40;
        Poke(0x40 + Operators[c], (x|63) - volume + volume*(x&63)/63);
        Poke(0x43 + Operators[c], (y|63) - volume + volume*(y&63)/63);
        // Correct formula (ST3, AdPlug):
        //   63-((63-(instrvol))/63)*chanvol
        // Reduces to (tested identical):
        //   63 - chanvol + chanvol*instrvol/63
        // Also (slower, floats):
        //   63 + chanvol * (instrvol / 63.0 - 1)
    }
    void Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        Touch_Real(c, volume>8725  ? std::log(volume)*11.541561 + (0.5 - 104.22845) : 0);
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
    void Patch(unsigned c, unsigned i)
    {
        static const unsigned char data[4] = {0x20,0x60,0x80,0xE0};
        ins[c] = i;
        unsigned o = Operators[c], x = adl[i].carrier_E862, y = adl[i].modulator_E862;
        for(unsigned a=0; a<4; ++a)
        {
            Poke(data[a]+o,   x&0xFF); x>>=8;
            Poke(data[a]+o+3, y&0xFF); y>>=8;
        }
    }
    void Pan(unsigned c, unsigned value)
    {
        Poke(0xC0+Channels[c], adl[ins[c]].feedconn | value);
    }
    void Silence() // Silence all OPL channels.
    {
        for(unsigned c=0; c<18; ++c) { NoteOff(c); Touch_Real(c, 0); }
    }
    void Reset()
    {
        dbopl_handler.Init(PCM_RATE);
        for(unsigned a=0; a<18; ++a)
            { ins[a] = 198; pit[a] = 0; }
        static const short data[(2+3+2+2)*2] =
        { 0x004,96, 0x004,128,        // Pulse timer
          0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable
          0x001,32, 0x0BD,0,          // Enable wave & melodic
          0x105, 1, 0x104,0           // Enable OPL3 18-channel
        };
        for(unsigned a=0; a<18; a+=2) Poke(data[a], data[a+1]);
        Silence();
    }
};

static class UI
{
public:
    int x, y, color, txtline;
    char slots[80][19], background[80][19];
    bool cursor_visible;
public:
    UI(): x(0), y(0), color(-1), txtline(1),
          cursor_visible(true)
        { std::fputc('\r', stderr);
          std::memset(slots, 0,        sizeof(slots));
          std::memset(background, '.', sizeof(background));
        }
    void HideCursor()
    {
        if(!cursor_visible) return;
        cursor_visible = false;
        std::fprintf(stderr, "\33[?25l"); // hide cursor
    }
    void ShowCursor()
    {
        if(cursor_visible) return;
        cursor_visible = true;
        GotoXY(0,19); Color(7);
        std::fprintf(stderr, "\33[?25h"); // show cursor
        std::fflush(stderr);
    }
    void PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)))
    {
        va_list ap;
        va_start(ap, fmt);
        char Line[512];
      #ifndef __CYGWIN__
        int nchars = vsnprintf(Line, sizeof(Line), fmt, ap);
      #else
        int nchars = vsprintf(Line, fmt, ap); /* SECURITY: POSSIBLE BUFFER OVERFLOW */
      #endif
        va_end(ap);

        if(nchars == 0) return;

        const int beginx = 2;

        HideCursor();
        GotoXY(beginx,txtline);
        for(x=beginx; x-beginx<nchars && x < 80; ++x)
        {
            if(Line[x-beginx] == '\n') break;
            Color(Line[x-beginx] == '.' ? 1 : 8);
            std::fputc( background[x][txtline] = Line[x-beginx], stderr);
        }
        for(int tx=x; tx<80; ++tx)
        {
            if(background[tx][txtline]!='.' && slots[tx][txtline]=='.')
            {
                GotoXY(tx,txtline);
                Color(1);
                std::fputc(background[tx][txtline] = '.', stderr);
                ++x;
            }
        }
        std::fflush(stderr);

        txtline=1 + (txtline)%18;
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
    {
        HideCursor();
        int notex = 2 + (note+55)%77;
        int notey = 1+adlchn;
        char illustrate_char = background[notex][notey];
        if(pressure > 0)
        {
            illustrate_char = adl[ins].symbol;
            if(bend < 0) illustrate_char = '<';
            if(bend > 0) illustrate_char = '>';
        }
        else if(pressure < 0)
        {
            illustrate_char = '%';
        }
        if(slots[notex][notey] != illustrate_char)
        {
            slots[notex][notey] = illustrate_char;
            GotoXY(notex, notey);
            if(!pressure)
                Color(illustrate_char=='.' ? 1 : 8);
            else
                Color(  AllocateColor(ins) );
            std::fputc(illustrate_char, stderr);
            std::fflush(stderr);
            ++x;
        }
    }
    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy)
    {
        while(newy > y) { std::fputc('\n', stderr); y+=1; x=0; }
        if(newy < y) { std::fprintf(stderr, "\33[%dA", y-newy); y = newy; }
        if(newx != x)
        {
            if(newx == 0 || (newx<10 && std::abs(newx-x)>=10))
                { std::fputc('\r', stderr); x = 0; }
            if(newx < x) std::fprintf(stderr, "\33[%dD", x-newx);
            if(newx > x) std::fprintf(stderr, "\33[%dC", newx-x);
            x = newx;
        }
    }
    // Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor)
    {
        if(color != newcolor)
        {
            static const char map[8+1] = "04261537";
            std::fprintf(stderr, "\33[0;%s40;3%c",
                (newcolor&8) ? "1;" : "", map[newcolor&7]);
            // If xterm-256color is used, try using improved colors:
            //        Translate 8 (dark gray) into #003366 (bluish dark cyan)
            //        Translate 1 (dark blue) into #000033 (darker blue)
            if(newcolor==8) std::fprintf(stderr, ";38;5;24;25");
            if(newcolor==1) std::fprintf(stderr, ";38;5;17;25");
            std::fputc('m', stderr);
            color=newcolor;
        }
    }
    // Choose a permanent color for given instrument
    int AllocateColor(int ins)
    {
        static char ins_colors[256] = { 0 }, ins_color_counter = 0;
        if(ins_colors[ins])
            return ins_colors[ins];
        if(ins & 0x80)
        {
            static const char shuffle[] = {2,3,4,5,6,7};
            return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
        }
        else
        {
            static const char shuffle[] = {10,11,12,13,14,15};
            return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
        }
    }
} UI;

class MIDIplay
{
    // Information about each track
    struct Position
    {
        bool began;
        double wait;
        struct TrackInfo
        {
            size_t ptr;
            long   delay;
            int    status;

            TrackInfo(): ptr(0), delay(0), status(0) { }
        };
        std::vector<TrackInfo> track;

        Position(): began(false), wait(0.0), track() { }
    } CurrentPosition, LoopBeginPosition;

    // Persistent settings for each MIDI channel
    struct MIDIchannel
    {
        unsigned short bank, portamento;
        unsigned char patch;
        unsigned char volume, expression;
        unsigned char panning, vibrato, sustain;
        double bend, bendsense;
        double vibpos, vibspeed, vibdepth;
        long vibdelay;
        unsigned char lastlrpn,lastmrpn; bool nrpn;
        struct NoteInfo
        {
            signed char adlchn; // adlib channel
            unsigned char  vol; // pressure
            unsigned short ins; // instrument selected on noteon
            unsigned short tone; // tone selected for note
        };
        typedef std::map<unsigned char,NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        activenotemap_t activenotes;

        MIDIchannel()
            : bank(0), portamento(0), patch(0),
              volume(100),expression(100),
              panning(0x30), vibrato(0), sustain(0),
              bend(0.0), bendsense(2 / 8192.0),
              vibpos(0), vibspeed(2*3.141592653*5.0),
              vibdepth(0.5/127), vibdelay(0),
              lastlrpn(0),lastmrpn(0),nrpn(false),
              activenotes() { }
    } Ch[16];

    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For collisions
        unsigned char midichn, note;
        // For channel allocation:
        enum { off, on, sustained } state;
        long age;
        AdlChannel(): midichn(0),note(0), state(off),age(0) { }
    } ch[18];

    std::vector< std::vector<unsigned char> > TrackData;
public:
    double InvDeltaTicks, Tempo;
    bool loopStart, loopEnd;
    OPL3 opl;
public:
    static unsigned long ReadBEInt(const void* buffer, unsigned nbytes)
    {
        unsigned long result=0;
        const unsigned char* data = (const unsigned char*) buffer;
        for(unsigned n=0; n<nbytes; ++n)
            result = (result << 8) + data[n];
        return result;
    }
    unsigned long ReadVarLen(unsigned tk)
    {
        unsigned long result = 0;
        for(;;)
        {
            unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
            result = (result << 7) + (byte & 0x7F);
            if(!(byte & 0x80)) break;
        }
        return result;
    }

    bool LoadMIDI(const std::string& filename)
    {
        FILE* fp = std::fopen(filename.c_str(), "rb");
        if(!fp) { std::perror(filename.c_str()); return false; }
        char HeaderBuf[4+4+2+2+2]="";
    riffskip:;
        std::fread(HeaderBuf, 1, 4+4+2+2+2, fp);
        if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
            { fseek(fp, 6, SEEK_CUR); goto riffskip; }
        if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
        { InvFmt:
            std::fclose(fp);
            std::fprintf(stderr, "%s: Invalid format\n", filename.c_str());
            return false;
        }
        size_t Fmt        = ReadBEInt(HeaderBuf+8,  2);
        size_t TrackCount = ReadBEInt(HeaderBuf+10, 2);
        size_t DeltaTicks = ReadBEInt(HeaderBuf+12, 2);
        TrackData.resize(TrackCount);
        CurrentPosition.track.resize(TrackCount);
        InvDeltaTicks = 1e-6 / DeltaTicks;
        Tempo         = 1e6 * InvDeltaTicks;
        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            // Read track header
            std::fread(HeaderBuf, 1, 8, fp);
            if(std::memcmp(HeaderBuf, "MTrk", 4) != 0) goto InvFmt;
            size_t TrackLength = ReadBEInt(HeaderBuf+4, 4);
            // Read track data
            TrackData[tk].resize(TrackLength);
            std::fread(&TrackData[tk][0], 1, TrackLength, fp);
            // Read next event time
            CurrentPosition.track[tk].delay = ReadVarLen(tk);
        }
        loopStart = true;

        opl.Reset(); // Reset AdLib
        opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
        return true;
    }

    /* Periodic tick handler.
     *   Input: s           = seconds since last call
     *   Input: granularity = don't expect intervals smaller than this, in seconds
     *   Output: desired number of seconds until next call
     */
    double Tick(double s, double granularity)
    {
        if(CurrentPosition.began) CurrentPosition.wait -= s;
        while(CurrentPosition.wait < granularity/2)
        {
            //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            ProcessEvents();
        }

        for(unsigned a=0; a<16; ++a)
            if(Ch[a].vibrato && !Ch[a].activenotes.empty()) 
            {
                NoteUpdate_All(a, Upd_Pitch);
                Ch[a].vibpos += s * Ch[a].vibspeed;
            }
            else
                Ch[a].vibpos = 0.0;

        return CurrentPosition.wait;
    }
private:
    enum { Upd_Patch  = 0x1,
           Upd_Pan    = 0x2,
           Upd_Volume = 0x4,
           Upd_Pitch  = 0x8,
           Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
           Upd_Off    = 0x20 };

    void NoteUpdate
        (unsigned MidCh,
         MIDIchannel::activenoteiterator i,
         unsigned props_mask)
    {
        // Determine the instrument and the note value (tone)
        int c = i->second.adlchn, tone = i->second.tone, ins = i->second.ins;

        if(props_mask & Upd_Off) // note off
        {
            if(Ch[MidCh].sustain == 0)
            {
                opl.NoteOff(c);
                ch[c].age   = 0;
                ch[c].state = AdlChannel::off;
                UI.IllustrateNote(c, tone, ins, 0, 0.0);
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                ch[c].state = AdlChannel::sustained;
                UI.IllustrateNote(c, tone, ins, -1, 0.0);
            }
            Ch[MidCh].activenotes.erase(i);
        }
        if(props_mask & Upd_Patch)
        {
            opl.Patch(c, ins);
            ch[c].age = 0;
        }
        if(props_mask & Upd_Pan)
        {
            opl.Pan(c, Ch[MidCh].panning);
        }
        if(props_mask & Upd_Volume)
        {
            opl.Touch(c, i->second.vol * Ch[MidCh].volume * Ch[MidCh].expression);
        }
        if(props_mask & Upd_Pitch)
        {
            double bend = Ch[MidCh].bend;
            if(Ch[MidCh].vibrato && ch[c].age >= Ch[MidCh].vibdelay)
                bend += Ch[MidCh].vibrato * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);
            opl.NoteOn(c, 172.00093 * std::exp(0.057762265 * (tone + bend)));
            ch[c].state = AdlChannel::on;
            UI.IllustrateNote(c, tone, ins, i->second.vol, Ch[MidCh].bend);
        }
    }

    void ProcessEvents()
    {
        loopEnd = false;
        const size_t TrackCount = TrackData.size();
        const Position RowBeginPosition ( CurrentPosition );
        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            if(CurrentPosition.track[tk].status >= 0
            && CurrentPosition.track[tk].delay <= 0)
            {
                // Handle event
                HandleEvent(tk);
                // Read next event time (unless the track just ended)
                if(CurrentPosition.track[tk].ptr >= TrackData[tk].size())
                    CurrentPosition.track[tk].status = -1;
                if(CurrentPosition.track[tk].status >= 0)
                    CurrentPosition.track[tk].delay += ReadVarLen(tk);
            }
        }
        // Find shortest delay from all track
        long shortest = -1;
        for(size_t tk=0; tk<TrackCount; ++tk)
            if(CurrentPosition.track[tk].status >= 0
            && (shortest == -1
               || CurrentPosition.track[tk].delay < shortest))
            {
                shortest = CurrentPosition.track[tk].delay;
            }

        // Schedule the next playevent to be processed after that delay
        for(size_t tk=0; tk<TrackCount; ++tk)
            CurrentPosition.track[tk].delay -= shortest;

        double t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t;
        for(unsigned a=0; a<18; ++a)
            if(ch[a].age < 0x70000000)
                ch[a].age += t*1000;

        if(loopStart)
        {
            LoopBeginPosition = RowBeginPosition;
            loopStart = false;
        }
        if(shortest < 0 || loopEnd)
        {
            // Loop if song end reached
            loopEnd         = false;
            CurrentPosition = LoopBeginPosition;
            shortest        = 0;
        }
    }
    void HandleEvent(size_t tk)
    {
        unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
        if(byte >= 0xF0)
        {
            //std::fprintf(stderr, "@%X Track %u: %02X\n", CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte);
            // Special Fx events
            if(byte == 0xF7 || byte == 0xF0)
                { unsigned length = ReadVarLen(tk);
                  //std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
                  CurrentPosition.track[tk].ptr += length;
                  UI.PrintLn("SysEx %02X: %u bytes", byte, length/*, data.c_str()*/);
                  return; } // Ignore SysEx
            if(byte == 0xF3) { CurrentPosition.track[tk].ptr += 1; return; }
            if(byte == 0xF2) { CurrentPosition.track[tk].ptr += 2; return; }
            // Special event FF
            unsigned char evtype = TrackData[tk][CurrentPosition.track[tk].ptr++];
            unsigned long length = ReadVarLen(tk);
            std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
            CurrentPosition.track[tk].ptr += length;
            if(evtype == 0x2F) { CurrentPosition.track[tk].status = -1; return; }
            if(evtype == 0x51) { Tempo = ReadBEInt(data.data(), data.size()) * InvDeltaTicks; return; }
            if(evtype == 6 && data == "loopStart") loopStart = true;
            if(evtype == 6 && data == "loopEnd"  ) loopEnd   = true;
            if(evtype >= 1 && evtype <= 6)
                UI.PrintLn("Meta %d: %s", evtype, data.c_str());
            return;
        }
        // Any normal event (80..EF)
        if(byte < 0x80)
          { byte = CurrentPosition.track[tk].status | 0x80;
            CurrentPosition.track[tk].ptr--; }
        //std::fprintf(stderr, "@%X Track %u: %02X\n", CurrentPosition.track[tk].ptr, (unsigned)tk, byte);
        unsigned MidCh = byte & 0x0F;
        CurrentPosition.track[tk].status = byte;
        switch(byte >> 4)
        {
            case 0x8: // Note off
            {
            Actually8x:;
                int note = TrackData[tk][CurrentPosition.track[tk].ptr];
                CurrentPosition.track[tk].ptr += 2;
                NoteOff(MidCh, note);
                break;
            }
            case 0x9: // Note on
            {
                if(TrackData[tk][CurrentPosition.track[tk].ptr+1] == 0) goto Actually8x;
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                if(Ch[MidCh].activenotes.find(note) != Ch[MidCh].activenotes.end())
                {
                    // Ignore repeat notes w/o keyoffs
                    break;
                }
                // Allocate AdLib channel (the physical sound channel for the note)
                long bs = -9;
                unsigned c = ~0u, i = Ch[MidCh].patch;
                if(MidCh == 9) i = 128 + note; // Percussion instrument
                i = banks[AdlBank][i];

                int tone = adlins[i].tone ? adlins[i].tone : note;
                i = adlins[i].adlno;

                for(unsigned a = 0; a < 18; ++a)
                {
                    long s = ch[a].age;   // Age in seconds = better score
                    switch(ch[a].state)
                    {
                        case AdlChannel::off:
                            s += 15000; // Empty channel = privileged
                            break;
                        case AdlChannel::sustained:
                            s += 5000;  // Sustained = free but deferred
                            break;
                        default: break;
                    }
                    if(i == opl.ins[a]) s += 50;  // Same instrument = good
                    if(a == MidCh) s += 20;
                    if(i<128 && opl.ins[a]>127)
                        s += 800;   // Percussion is inferior to melody
                    else if(opl.ins[a]<128 && i>127)
                        s -= 800; // Percussion is inferior to melody
                    if(s > bs) { bs=s; c = a; } // Best candidate wins
                }
                if(c == ~0u) break; // Could not play this note. Ignore it.
                if(ch[c].state == AdlChannel::on)
                    NoteOff(ch[c].midichn, ch[c].note); // Collision: Kill old note
                if(ch[c].state == AdlChannel::sustained)
                {
                    NoteOffSustain(c);
                    // A sustained note needs to be keyoff'd
                    // first so that it can be retriggered.
                }
                // Allocate active note for MIDI channel
                std::pair<MIDIchannel::activenoteiterator,bool>
                    ir = Ch[MidCh].activenotes.insert(
                        std::make_pair(note, MIDIchannel::NoteInfo()));
                ir.first->second.adlchn = c;
                ir.first->second.vol    = vol;
                ir.first->second.ins    = i;
                ir.first->second.tone   = tone;
                ch[c].midichn = MidCh;
                ch[c].note    = note;
                CurrentPosition.began  = true;
                NoteUpdate(MidCh, ir.first, Upd_All | Upd_Patch);
                break;
            }
            case 0xA: // Note touch
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                MIDIchannel::activenoteiterator
                    i = Ch[MidCh].activenotes.find(note);
                if(i == Ch[MidCh].activenotes.end())
                {
                    // Ignore touch if note is not active
                    break;
                }
                i->second.vol = vol;
                NoteUpdate(MidCh, i, Upd_Volume);
                break;
            }
            case 0xB: // Controller change
            {
                int ctrlno = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  value = TrackData[tk][CurrentPosition.track[tk].ptr++];
                switch(ctrlno)
                {
                    case 1: // Adjust vibrato
                        //UI.PrintLn("%u:vibrato %d", MidCh,value);
                        Ch[MidCh].vibrato = value; break;
                    case 0: // Set bank msb
                        Ch[MidCh].bank = (Ch[MidCh].bank & 0xFF) | (value<<8);
                        break;
                    case 32: // Set bank lsb
                        Ch[MidCh].bank = (Ch[MidCh].bank & 0xFF00) | (value);
                        break;
                    case 5: // Set portamento msb
                        Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x7F) | (value<<7);
                        UpdatePortamento(MidCh);
                        break;
                    case 37: // Set portamento lsb
                        Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x3F80) | (value);
                        UpdatePortamento(MidCh);
                        break;
                    case 7: // Change volume
                        Ch[MidCh].volume = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 64: // Change sustain
                        Ch[MidCh].sustain = value;
                        if(!value)
                            for(unsigned c=0; c<18; ++c)
                                if(ch[c].state == AdlChannel::sustained)
                                    NoteOffSustain(c);
                        break;
                    case 11: // Change expression (another volume factor)
                        Ch[MidCh].expression = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 10: // Change panning
                        Ch[MidCh].panning = 0x00;
                        if(value  < 64+32) Ch[MidCh].panning |= 0x10;
                        if(value >= 64-32) Ch[MidCh].panning |= 0x20;
                        NoteUpdate_All(MidCh, Upd_Pan);
                        break;
                    case 121: // Reset all controllers
                        Ch[MidCh].bend       = 0;
                        Ch[MidCh].volume     = 100;
                        Ch[MidCh].expression = 100;
                        Ch[MidCh].sustain    = 0;
                        Ch[MidCh].vibrato    = 0;
                        Ch[MidCh].vibspeed   = 2*3.141592653*5.0;
                        Ch[MidCh].vibdepth   = 0.5/127;
                        Ch[MidCh].vibdelay   = 0;
                        Ch[MidCh].panning    = 0x30;
                        Ch[MidCh].portamento = 0;
                        UpdatePortamento(MidCh);
                        NoteUpdate_All(MidCh, Upd_Pan+Upd_Volume+Upd_Pitch);
                        // Kill all sustained notes
                        for(unsigned c=0; c<18; ++c)
                            if(ch[c].state == AdlChannel::sustained)
                                NoteOffSustain(c);
                        break;
                    case 123: // All notes off
                        NoteUpdate_All(MidCh, Upd_Off);
                        break;
                    case 98: Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=true; break;
                    case 99: Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=true; break;
                    case 100:Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=false; break;
                    case 101:Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=false; break;
                    case  6: SetRPN(MidCh, value, true); break;
                    case 38: SetRPN(MidCh, value, false); break;
                    default:
                        UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
                }
                break;
            }
            case 0xC: // Patch change
                Ch[MidCh].patch = TrackData[tk][CurrentPosition.track[tk].ptr++];
                if(Ch[MidCh].bank)
                {
                    UI.PrintLn("Bank %Xh ignored (ch %u)", Ch[MidCh].bank, MidCh);
                }
                break;
            case 0xD: // Channel after-touch
            {
                // TODO: Verify, is this correct action?
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                for(MIDIchannel::activenoteiterator
                    i = Ch[MidCh].activenotes.begin();
                    i != Ch[MidCh].activenotes.end();
                    ++i)
                {
                    // Set this pressure to all active notes on the channel
                    i->second.vol = vol;
                }
                NoteUpdate_All(MidCh, Upd_Volume);
                break;
            }
            case 0xE: // Wheel/pitch bend
            {
                int a = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int b = TrackData[tk][CurrentPosition.track[tk].ptr++];
                Ch[MidCh].bend = (a + b*128 - 8192) * Ch[MidCh].bendsense;
                NoteUpdate_All(MidCh, Upd_Pitch);
                break;
            }
        }
    }

    void SetRPN(unsigned MidCh, unsigned value, bool MSB)
    {
        bool nrpn = Ch[MidCh].nrpn;
        unsigned addr = Ch[MidCh].lastmrpn*0x100 + Ch[MidCh].lastlrpn;
        switch(addr + nrpn*0x10000 + MSB*0x20000)
        {
            case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
                Ch[MidCh].bendsense = value/8192.0;
                break;
            case 0x0108 + 1*0x10000 + 1*0x20000: // Vibrato speed
                if(value == 64)
                    Ch[MidCh].vibspeed = 1.0;
                else if(value < 100)
                    Ch[MidCh].vibspeed = 1.0/(1.6e-2*(value?value:1));
                else
                    Ch[MidCh].vibspeed = 1.0/(0.051153846*value-3.4965385);
                Ch[MidCh].vibspeed *= 2*3.141592653*5.0;
                break;
            case 0x0109 + 1*0x10000 + 1*0x20000: // Vibrato depth
                Ch[MidCh].vibdepth = ((value-64)*0.15)*0.01;
                break;
            case 0x010A + 1*0x10000 + 1*0x20000: // Vibrato delay in millisecons
                Ch[MidCh].vibdelay =
                    value ? long(0.2092 * std::exp(0.0795 * value)) : 0.0;
                break;
            default: UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);
        }
    }

    void UpdatePortamento(unsigned MidCh)
    {
        // mt = 2^(portamento/2048) * (1.0 / 5000.0)
        /*
        double mt = std::exp(0.00033845077 * Ch[MidCh].portamento);
        NoteUpdate_All(MidCh, Upd_Pitch);
        */
        UI.PrintLn("Portamento %u: %u (unimplemented)", MidCh, Ch[MidCh].portamento);
    }

    void NoteUpdate_All(unsigned MidCh, unsigned props_mask)
    {
        for(MIDIchannel::activenoteiterator
            i = Ch[MidCh].activenotes.begin();
            i != Ch[MidCh].activenotes.end();
            )
        {
            MIDIchannel::activenoteiterator j(i++);
            NoteUpdate(MidCh, j, props_mask);
        }
    }
    void NoteOff(unsigned MidCh, int note)
    {
        MIDIchannel::activenoteiterator
            i = Ch[MidCh].activenotes.find(note);
        if(i != Ch[MidCh].activenotes.end())
            NoteUpdate(MidCh, i, Upd_Off);
    }
    void NoteOffSustain(unsigned c)
    {
        UI.IllustrateNote(c, ch[c].note, opl.ins[c], 0, 0.0);
        opl.NoteOff(c);
        ch[c].state = AdlChannel::off;
    }
};

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
        double wet_gain_dB,
        double room_scale, double reverberance, double fhf_damping, /* 0..1 */
        double pre_delay_s, double stereo_depth,
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
static struct MyReverbData
{
    bool wetonly;
    Reverb chan[2];

    MyReverbData() : wetonly(false)
    {
        for(size_t i=0; i<2; ++i)
            chan[i].Create(OPL3::PCM_RATE,
                5.0,  // wet_gain_dB  (-10..10)
                .7,   // room_scale   (0..1)
                .6,   // reverberance (0..1)
                .5,   // hf_damping   (0..1)
                .000, // pre_delay_s  (0.. 0.5)
                .6,   // stereo_depth (0..1)
                MaxSamplesAtTime);
    }
} reverb_data;

static std::deque<short> AudioBuffer;
#ifndef __MINGW32__
static pthread_mutex_t AudioBuffer_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void SDL_AudioCallback(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    #ifndef __MINGW32__
    pthread_mutex_lock(&AudioBuffer_lock);
    #endif
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
        target[a] = AudioBuffer[a];
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    //fprintf(stderr, " - remain %u\n", (unsigned) AudioBuffer.size()/2);
    #ifndef __MINGW32__
    pthread_mutex_unlock(&AudioBuffer_lock);
    #endif
    SDL_UnlockAudio();
}
static void AddMonoAudio(unsigned long /*count*/, int* /*samples*/)
{
    std::fprintf(stderr, "Mono audio not implemented");
    std::abort();
}
static void AddStereoAudio(unsigned long count, int* samples)
{
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
    static unsigned amplitude_display_counter = 0;
    if(!amplitude_display_counter--)
    {
        amplitude_display_counter = (OPL3::PCM_RATE / count) / 24;
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
            amp[w] = 18*dB/maxdB;
        }
        for(unsigned y=0; y<18; ++y)
            for(unsigned w=0; w<2; ++w)
            {
                char c = amp[w] > 17-y ? '|' : UI.background[w][y+1];
                if(UI.slots[w][y+1] != c)
                {
                    UI.slots[w][y+1] = c;
                    UI.HideCursor();
                    UI.GotoXY(w,y+1);
                    UI.Color(c=='|' ? (y?(y<4?12:(y<8?14:10)):15) :
                            (c=='.' ? 1 : 8));
                    std::fputc(c, stderr);
                    UI.x += 1;
    }       }   }

    // Convert input to float format
    std::vector<float> dry[2];
    for(unsigned w=0; w<2; ++w)
    {
        dry[w].resize(count);
        for(unsigned long p = 0; p < count; ++p)
            dry[w][p] = (samples[p*2+w] - average_flt[w]) * double(0.6/32768.0);
        reverb_data.chan[w].input_fifo.insert(
        reverb_data.chan[w].input_fifo.end(),
            dry[w].begin(), dry[w].end());
    }
    // Reverbify it
    for(unsigned w=0; w<2; ++w)
        reverb_data.chan[w].Process(count);

    // Convert to signed 16-bit int format and put to playback queue
    #ifndef __MINGW32__
    pthread_mutex_lock(&AudioBuffer_lock);
    #endif
    size_t pos = AudioBuffer.size();
    AudioBuffer.resize(pos + count*2);
    for(unsigned long p = 0; p < count; ++p)
        for(unsigned w=0; w<2; ++w)
        {
            float out = ((1 - reverb_data.wetonly) * dry[w][p] +
                .5 * (reverb_data.chan[0].out[w][p]
                    + reverb_data.chan[1].out[w][p])) * 32768.0f
                 + average_flt[w];
            AudioBuffer[pos+p*2+w] =
                out<-32768.f ? -32768 :
                out>32767.f ?  32767 : out;
        }
    #ifndef __MINGW32__
    pthread_mutex_unlock(&AudioBuffer_lock);
    #endif
}

int main(int argc, char** argv)
{
    const unsigned Interval = 50;
    static SDL_AudioSpec spec;
    spec.freq     = OPL3::PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq / Interval;
    spec.callback = SDL_AudioCallback;
    if(SDL_OpenAudio(&spec, 0) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }

    if(argc < 2 )
    {
        std::printf(
            "Usage: midiplay <midifilename> [<banknumber>]\n"
            "     Banks: 0 = General MIDI\n"
            "            1 = HMI\n"
            "            2 = HMI Int\n"
            "            3 = HMI Ham\n"
            "            4 = HMI Rick\n"
            "     Use banks 1-4 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            );
        return 0;
    }

    MIDIplay player;

    if(!player.LoadMIDI(argv[1]))
        return 2;
    if(argc == 3)
    {
        AdlBank = std::atoi(argv[2]);
        if(AdlBank > 4 || AdlBank < 0)
        {
            std::fprintf(stderr, "bank number may only be 0..4.\n");
            return 0;
        }
        std::printf("Bank %u selected.\n", AdlBank);
    }

    SDL_PauseAudio(0);

    const double mindelay = 0.01;
    const double maxdelay = MaxSamplesAtTime / (double)OPL3::PCM_RATE;

    UI.GotoXY(0,0); UI.Color(15);
    std::fprintf(stderr, "Hit Ctrl-C to quit\r");

    for(double delay=0; ; )
    {
        const double eat_delay = delay < maxdelay ? delay : maxdelay;
        delay -= eat_delay;
        player.opl.ProduceAudio(eat_delay);
        while(AudioBuffer.size() > 2*spec.freq/Interval)
            SDL_Delay(1e3 * eat_delay);

        double nextdelay = player.Tick(eat_delay, mindelay);
        UI.ShowCursor();

        delay = nextdelay;
    }

    SDL_CloseAudio();
    return 0;
}

const adldata adl[] =
{ //    +---------+-------- Wave select settings
  //    | +-------:-+------ Sustain/release rates
  //    | | +-----:-:-+---- Attack/decay rates
  //    | | | +---:-:-:-+-- AM/VIB/EG/KSR/Multiple bits
  //    | | | |   | | | |
    { 0x0F4F201,0x0F7F201, 0x8F,0x06, 0x8,'P' }, // 0: GM0; AcouGrandPiano
    { 0x0F4F201,0x0F7F201, 0x4B,0x00, 0x8,'P' }, // 1: GM1; BrightAcouGrand
    { 0x0F4F201,0x0F6F201, 0x49,0x00, 0x8,'P' }, // 2: GM2; ElecGrandPiano
    { 0x0F7F281,0x0F7F241, 0x12,0x00, 0x6,'P' }, // 3: GM3; Honky-tonkPiano
    { 0x0F7F101,0x0F7F201, 0x57,0x00, 0x0,'P' }, // 4: GM4; Rhodes Piano
    { 0x0F7F101,0x0F7F201, 0x93,0x00, 0x0,'P' }, // 5: GM5; Chorused Piano
    { 0x0F2A101,0x0F5F216, 0x80,0x0E, 0x8,'h' }, // 6: GM6; Harpsichord
    { 0x0F8C201,0x0F8C201, 0x92,0x00, 0xA,'c' }, // 7: GM7; Clavinet
    { 0x0F4F60C,0x0F5F381, 0x5C,0x00, 0x0,'c' }, // 8: GM8; Celesta
    { 0x0F2F307,0x0F1F211, 0x97,0x80, 0x2,'k' }, // 9: GM9; Glockenspiel
    { 0x0F45417,0x0F4F401, 0x21,0x00, 0x2,'m' }, // 10: GM10; Music box
    { 0x0F6F398,0x0F6F281, 0x62,0x00, 0x0,'v' }, // 11: GM11; Vibraphone
    { 0x0F6F618,0x0F7E701, 0x23,0x00, 0x0,'m' }, // 12: GM12; Marimba
    { 0x0F6F615,0x0F6F601, 0x91,0x00, 0x4,'x' }, // 13: GM13; Xylophone
    { 0x0F3D345,0x0F3A381, 0x59,0x80, 0xC,'b' }, // 14: GM14; Tubular Bells
    { 0x1F57503,0x0F5B581, 0x49,0x80, 0x4,'d' }, // 15: GM15; Dulcimer
    { 0x014F671,0x007F131, 0x92,0x00, 0x2,'o' }, // 16: GM16; HMIGM16; Hammond Organ; am016.in
    { 0x058C772,0x008C730, 0x14,0x00, 0x2,'o' }, // 17: GM17; HMIGM17; Percussive Organ; am017.in
    { 0x018AA70,0x0088AB1, 0x44,0x00, 0x4,'o' }, // 18: GM18; HMIGM18; Rock Organ; am018.in
    { 0x1239723,0x01455B1, 0x93,0x00, 0x4,'o' }, // 19: GM19; HMIGM19; Church Organ; am019.in
    { 0x1049761,0x00455B1, 0x13,0x80, 0x0,'o' }, // 20: GM20; HMIGM20; Reed Organ; am020.in
    { 0x12A9824,0x01A46B1, 0x48,0x00, 0xC,'a' }, // 21: GM21; HMIGM21; Accordion; am021.in
    { 0x1069161,0x0076121, 0x13,0x00, 0xA,'h' }, // 22: GM22; HMIGM22; Harmonica; am022.in
    { 0x0067121,0x00761A1, 0x13,0x89, 0x6,'o' }, // 23: GM23; HMIGM23; Tango Accordion; am023.in
    { 0x194F302,0x0C8F341, 0x9C,0x80, 0xC,'G' }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in
    { 0x19AF303,0x0E7F111, 0x54,0x00, 0xC,'G' }, // 25: GM25; HMIGM25; Acoustic Guitar2; am025.in
    { 0x03AF123,0x0F8F221, 0x5F,0x00, 0x0,'G' }, // 26: GM26; HMIGM26; Electric Guitar1; am026.in
    { 0x122F603,0x0F8F321, 0x87,0x80, 0x6,'G' }, // 27: GM27; Electric Guitar2
    { 0x054F903,0x03AF621, 0x47,0x00, 0x0,'G' }, // 28: GM28; HMIGM28; hamM3; hamM60; intM3; rickM3; BPerc.in; BPercin; Electric Guitar3; am028.in; muteguit
    { 0x1419123,0x0198421, 0x4A,0x05, 0x8,'G' }, // 29: GM29; Overdrive Guitar
    { 0x1199523,0x0199421, 0x4A,0x00, 0x8,'G' }, // 30: GM30; HMIGM30; hamM6; intM6; rickM6; Distorton Guitar; GDist.in; GDistin; am030.in
    { 0x04F2009,0x0F8D184, 0xA1,0x80, 0x8,'G' }, // 31: GM31; HMIGM31; hamM5; intM5; rickM5; GFeedbck; Guitar Harmonics; am031.in
    { 0x0069421,0x0A6C3A2, 0x1E,0x00, 0x2,'B' }, // 32: GM32; HMIGM32; Acoustic Bass; am032.in
    { 0x028F131,0x018F131, 0x12,0x00, 0xA,'B' }, // 33: GM33; GM39; HMIGM33; HMIGM39; hamM68; Electric Bass 1; Synth Bass 2; am033.in; am039.in; synbass2
    { 0x0E8F131,0x078F131, 0x8D,0x00, 0xA,'B' }, // 34: GM34; HMIGM34; rickM81; Electric Bass 2; Slapbass; am034.in
    { 0x0285131,0x0487132, 0x5B,0x00, 0xC,'B' }, // 35: GM35; HMIGM35; Fretless Bass; am035.in
    { 0x09AA101,0x0DFF221, 0x8B,0x40, 0x8,'B' }, // 36: GM36; HMIGM36; Slap Bass 1; am036.in
    { 0x016A221,0x0DFA121, 0x8B,0x08, 0x8,'B' }, // 37: GM37; Slap Bass 2
    { 0x0E8F431,0x078F131, 0x8B,0x00, 0xA,'B' }, // 38: GM38; HMIGM38; hamM13; hamM67; intM13; rickM13; BSynth3; BSynth3.; Synth Bass 1; am038.in; synbass1
    { 0x113DD31,0x0265621, 0x15,0x00, 0x8,'V' }, // 39: GM40; HMIGM40; Violin; am040.in
    { 0x113DD31,0x0066621, 0x16,0x00, 0x8,'V' }, // 40: GM41; HMIGM41; Viola; am041.in
    { 0x11CD171,0x00C6131, 0x49,0x00, 0x8,'V' }, // 41: GM42; HMIGM42; Cello; am042.in
    { 0x1127121,0x0067223, 0x4D,0x80, 0x2,'V' }, // 42: GM43; HMIGM43; Contrabass; am043.in
    { 0x121F1F1,0x0166FE1, 0x40,0x00, 0x2,'V' }, // 43: GM44; HMIGM44; Tremulo Strings; am044.in
    { 0x175F502,0x0358501, 0x1A,0x80, 0x0,'H' }, // 44: GM45; HMIGM45; Pizzicato String; am045.in
    { 0x175F502,0x0F4F301, 0x1D,0x80, 0x0,'H' }, // 45: GM46; HMIGM46; Orchestral Harp; am046.in
    { 0x105F510,0x0C3F211, 0x41,0x00, 0x2,'M' }, // 46: GM47; HMIGM47; hamM14; intM14; rickM14; BSynth4; BSynth4.; Timpany; am047.in
    { 0x125B121,0x00872A2, 0x9B,0x01, 0xE,'S' }, // 47: GM48; HMIGM48; String Ensemble1; am048.in
    { 0x1037FA1,0x1073F21, 0x98,0x00, 0x0,'S' }, // 48: GM49; HMIGM49; String Ensemble2; am049.in
    { 0x012C1A1,0x0054F61, 0x93,0x00, 0xA,'S' }, // 49: GM50; HMIGM50; hamM20; intM20; rickM20; PMellow; PMellow.; Synth Strings 1; am050.in
    { 0x022C121,0x0054F61, 0x18,0x00, 0xC,'S' }, // 50: GM51; HMIGM51; SynthStrings 2; am051.in
    { 0x015F431,0x0058A72, 0x5B,0x83, 0x0,'O' }, // 51: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in
    { 0x03974A1,0x0677161, 0x90,0x00, 0x0,'O' }, // 52: GM53; HMIGM53; rickM86; Oohs.ins; Voice Oohs; am053.in
    { 0x0055471,0x0057A72, 0x57,0x00, 0xC,'O' }, // 53: GM54; HMIGM54; Synth Voice; am054.in
    { 0x0635490,0x045A541, 0x00,0x00, 0x8,'c' }, // 54: GM55; HMIGM55; Orchestra Hit; am055.in
    { 0x0178521,0x0098F21, 0x92,0x01, 0xC,'T' }, // 55: GM56; HMIGM56; Trumpet; am056.in
    { 0x0177521,0x0098F21, 0x94,0x05, 0xC,'T' }, // 56: GM57; HMIGM57; Trombone; am057.in
    { 0x0157621,0x0378261, 0x94,0x00, 0xC,'T' }, // 57: GM58; HMIGM58; Tuba; am058.in
    { 0x1179E31,0x12C6221, 0x43,0x00, 0x2,'T' }, // 58: GM59; HMIGM59; Muted Trumpet; am059.in
    { 0x06A6121,0x00A7F21, 0x9B,0x00, 0x2,'T' }, // 59: GM60; HMIGM60; French Horn; am060.in
    { 0x01F7561,0x00F7422, 0x8A,0x06, 0x8,'T' }, // 60: GM61; HMIGM61; Brass Section; am061.in
    { 0x15572A1,0x0187121, 0x86,0x83, 0x0,'T' }, // 61: GM62; Synth Brass 1
    { 0x03C5421,0x01CA621, 0x4D,0x00, 0x8,'T' }, // 62: GM63; HMIGM63; Synth Brass 2; am063.in
    { 0x1029331,0x00B7261, 0x8F,0x00, 0x8,'X' }, // 63: GM64; HMIGM64; Soprano Sax; am064.in
    { 0x1039331,0x0097261, 0x8E,0x00, 0x8,'X' }, // 64: GM65; HMIGM65; Alto Sax; am065.in
    { 0x1039331,0x0098261, 0x91,0x00, 0xA,'X' }, // 65: GM66; HMIGM66; Tenor Sax; am066.in
    { 0x10F9331,0x00F7261, 0x8E,0x00, 0xA,'X' }, // 66: GM67; HMIGM67; Baritone Sax; am067.in
    { 0x116AA21,0x00A8F21, 0x4B,0x00, 0x8,'T' }, // 67: GM68; HMIGM68; Oboe; am068.in
    { 0x1177E31,0x10C8B21, 0x90,0x00, 0x6,'T' }, // 68: GM69; HMIGM69; English Horn; am069.in
    { 0x1197531,0x0196132, 0x81,0x00, 0x0,'T' }, // 69: GM70; HMIGM70; Bassoon; am070.in
    { 0x0219B32,0x0177221, 0x90,0x00, 0x4,'F' }, // 70: GM71; HMIGM71; Clarinet; am071.in
    { 0x05F85E1,0x01A65E1, 0x1F,0x00, 0x0,'F' }, // 71: GM72; HMIGM72; Piccolo; am072.in
    { 0x05F88E1,0x01A65E1, 0x46,0x00, 0x0,'F' }, // 72: GM73; HMIGM73; Flute; am073.in
    { 0x01F75A1,0x00A7521, 0x9C,0x00, 0x2,'F' }, // 73: GM74; HMIGM74; Recorder; am074.in
    { 0x0588431,0x01A6521, 0x8B,0x00, 0x0,'F' }, // 74: GM75; HMIGM75; Pan Flute; am075.in
    { 0x05666E1,0x02665A1, 0x4C,0x00, 0x0,'F' }, // 75: GM76; HMIGM76; Bottle Blow; am076.in
    { 0x0467662,0x03655A1, 0xCB,0x00, 0x0,'F' }, // 76: GM77; HMIGM77; Shakuhachi; am077.in
    { 0x0075762,0x00756A1, 0x99,0x00, 0xB,'F' }, // 77: GM78; HMIGM78; Whistle; am078.in
    { 0x0077762,0x00776A1, 0x93,0x00, 0xB,'F' }, // 78: GM79; HMIGM79; hamM61; Ocarina; am079.in; ocarina
    { 0x203FF22,0x00FFF21, 0x59,0x00, 0x0,'L' }, // 79: GM80; HMIGM80; hamM16; hamM65; intM16; rickM16; LSquare; LSquare.; Lead 1 squareea; am080.in; squarewv
    { 0x10FFF21,0x10FFF21, 0x0E,0x00, 0x0,'L' }, // 80: GM81; HMIGM81; Lead 2 sawtooth; am081.in
    { 0x0558622,0x0186421, 0x46,0x80, 0x0,'L' }, // 81: GM82; HMIGM82; Lead 3 calliope; am082.in
    { 0x0126621,0x00A96A1, 0x45,0x00, 0x0,'L' }, // 82: GM83; HMIGM83; Lead 4 chiff; am083.in
    { 0x12A9221,0x02A9122, 0x8B,0x00, 0x0,'L' }, // 83: GM84; HMIGM84; Lead 5 charang; am084.in
    { 0x005DFA2,0x0076F61, 0x9E,0x40, 0x2,'L' }, // 84: GM85; HMIGM85; hamM17; intM17; rickM17; rickM87; Lead 6 voice; PFlutes; PFlutes.; Solovox.; am085.in
    { 0x001EF20,0x2068F60, 0x1A,0x00, 0x0,'L' }, // 85: GM86; HMIGM86; rickM93; Lead 7 fifths; Saw_wave; am086.in
    { 0x029F121,0x009F421, 0x8F,0x80, 0xA,'L' }, // 86: GM87; HMIGM87; Lead 8 brass; am087.in
    { 0x0945377,0x005A0A1, 0xA5,0x00, 0x2,'p' }, // 87: GM88; HMIGM88; Pad 1 new age; am088.in
    { 0x011A861,0x00325B1, 0x1F,0x80, 0xA,'p' }, // 88: GM89; HMIGM89; Pad 2 warm; am089.in
    { 0x0349161,0x0165561, 0x17,0x00, 0xC,'p' }, // 89: GM90; HMIGM90; hamM21; intM21; rickM21; LTriang; LTriang.; Pad 3 polysynth; am090.in
    { 0x0015471,0x0036A72, 0x5D,0x00, 0x0,'p' }, // 90: GM91; HMIGM91; rickM95; Pad 4 choir; Spacevo.; am091.in
    { 0x0432121,0x03542A2, 0x97,0x00, 0x8,'p' }, // 91: GM92; HMIGM92; Pad 5 bowedpad; am092.in
    { 0x177A1A1,0x1473121, 0x1C,0x00, 0x0,'p' }, // 92: GM93; HMIGM93; hamM22; intM22; rickM22; PSlow.in; PSlowin; Pad 6 metallic; am093.in
    { 0x0331121,0x0254261, 0x89,0x03, 0xA,'p' }, // 93: GM94; HMIGM94; hamM23; hamM54; intM23; rickM23; rickM96; Halopad.; PSweep.i; PSweepi; Pad 7 halo; am094.in; halopad
    { 0x14711A1,0x007CF21, 0x15,0x00, 0x0,'p' }, // 94: GM95; HMIGM95; hamM66; rickM97; Pad 8 sweep; Sweepad.; am095.in; sweepad
    { 0x0F6F83A,0x0028651, 0xCE,0x00, 0x2,'X' }, // 95: GM96; HMIGM96; FX 1 rain; am096.in
    { 0x1232121,0x0134121, 0x15,0x00, 0x0,'X' }, // 96: GM97; HMIGM97; FX 2 soundtrack; am097.in
    { 0x0957406,0x072A501, 0x5B,0x00, 0x0,'X' }, // 97: GM98; HMIGM98; FX 3 crystal; am098.in
    { 0x081B122,0x026F261, 0x92,0x83, 0xC,'X' }, // 98: GM99; HMIGM99; FX 4 atmosphere; am099.in
    { 0x151F141,0x0F5F242, 0x4D,0x00, 0x0,'X' }, // 99: GM100; HMIGM100; hamM51; FX 5 brightness; am100.in; am100in
    { 0x1511161,0x01311A3, 0x94,0x80, 0x6,'X' }, // 100: GM101; HMIGM101; FX 6 goblins; am101.in
    { 0x0311161,0x0031DA1, 0x8C,0x80, 0x6,'X' }, // 101: GM102; HMIGM102; rickM98; Echodrp1; FX 7 echoes; am102.in
    { 0x173F3A4,0x0238161, 0x4C,0x00, 0x4,'X' }, // 102: GM103; HMIGM103; FX 8 sci-fi; am103.in
    { 0x053D202,0x1F6F207, 0x85,0x03, 0x0,'G' }, // 103: GM104; HMIGM104; Sitar; am104.in
    { 0x111A311,0x0E5A213, 0x0C,0x80, 0x0,'G' }, // 104: GM105; HMIGM105; Banjo; am105.in
    { 0x141F611,0x2E6F211, 0x06,0x00, 0x4,'G' }, // 105: GM106; HMIGM106; hamM24; intM24; rickM24; LDist.in; LDistin; Shamisen; am106.in
    { 0x032D493,0x111EB91, 0x91,0x00, 0x8,'G' }, // 106: GM107; HMIGM107; Koto; am107.in
    { 0x056FA04,0x005C201, 0x4F,0x00, 0xC,'G' }, // 107: GM108; HMIGM108; hamM57; Kalimba; am108.in; kalimba
    { 0x0207C21,0x10C6F22, 0x49,0x00, 0x6,'T' }, // 108: GM109; HMIGM109; Bagpipe; am109.in
    { 0x133DD31,0x0165621, 0x85,0x00, 0xA,'S' }, // 109: GM110; HMIGM110; Fiddle; am110.in
    { 0x205DA20,0x00B8F21, 0x04,0x81, 0x6,'S' }, // 110: GM111; HMIGM111; Shanai; am111.in
    { 0x0E5F105,0x0E5C303, 0x6A,0x80, 0x6,'b' }, // 111: GM112; HMIGM112; Tinkle Bell; am112.in
    { 0x026EC07,0x016F802, 0x15,0x00, 0xA,'b' }, // 112: GM113; HMIGM113; hamM50; Agogo Bells; agogoin; am113.in
    { 0x0356705,0x005DF01, 0x9D,0x00, 0x8,'b' }, // 113: GM114; HMIGM114; Steel Drums; am114.in
    { 0x028FA18,0x0E5F812, 0x96,0x00, 0xA,'b' }, // 114: GM115; HMIGM115; rickM100; Woodblk.; Woodblock; am115.in
    { 0x007A810,0x003FA00, 0x86,0x03, 0x6,'M' }, // 115: GM116; HMIGM116; hamM69; hamP90; Taiko Drum; Taikoin; am116.in; taikoin
    { 0x247F811,0x003F310, 0x41,0x03, 0x4,'M' }, // 116: GM117; HMIGM117; hamM58; Melodic Tom; am117.in; melotom
    { 0x206F101,0x002F310, 0x8E,0x00, 0xE,'M' }, // 117: GM118; HMIGM118; Synth Drum; am118.in
    { 0x0001F0E,0x3FF1FC0, 0x00,0x00, 0xE,'c' }, // 118: GM119; HMIGM119; Reverse Cymbal; am119.in
    { 0x024F806,0x2845603, 0x80,0x88, 0xE,'G' }, // 119: GM120; HMIGM120; hamM36; intM36; rickM101; rickM36; DNoise1; DNoise1.; Fretnos.; Guitar FretNoise; am120.in
    { 0x000F80E,0x30434D0, 0x00,0x05, 0xE,'X' }, // 120: GM121; HMIGM121; Breath Noise; am121.in
    { 0x000F60E,0x3021FC0, 0x00,0x00, 0xE,'X' }, // 121: GM122; HMIGM122; Seashore; am122.in
    { 0x0A337D5,0x03756DA, 0x95,0x40, 0x0,'X' }, // 122: GM123; HMIGM123; Bird Tweet; am123.in
    { 0x261B235,0x015F414, 0x5C,0x08, 0xA,'X' }, // 123: GM124; HMIGM124; Telephone; am124.in
    { 0x000F60E,0x3F54FD0, 0x00,0x00, 0xE,'X' }, // 124: GM125; HMIGM125; Helicopter; am125.in
    { 0x001FF26,0x11612E4, 0x00,0x00, 0xE,'X' }, // 125: GM126; HMIGM126; Applause/Noise; am126.in
    { 0x0F0F300,0x2C9F600, 0x00,0x00, 0xE,'X' }, // 126: GM127; HMIGM127; Gunshot; am127.in
    { 0x277F810,0x006F311, 0x44,0x00, 0x8,'D' }, // 127: GP35; GP36; IntP34; IntP35; hamP11; hamP34; hamP35; rickP14; rickP34; rickP35; Ac Bass Drum; Bass Drum 1; apo035.i; apo035i; aps035i; kick2.in
    { 0x0FFF902,0x0FFF811, 0x07,0x00, 0x8,'D' }, // 128: GP37; Side Stick
    { 0x205FC00,0x017FA00, 0x00,0x00, 0xE,'s' }, // 129: GP38; GP40; Acoustic Snare; Electric Snare
    { 0x007FF00,0x008FF01, 0x02,0x00, 0x0,'h' }, // 130: GP39; Hand Clap
    { 0x00CF600,0x006F600, 0x00,0x00, 0x4,'M' }, // 131: GP41; GP43; GP45; GP47; GP48; GP50; GP87; hamP1; hamP2; hamP3; hamP4; hamP5; hamP6; rickP105; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; aps041i; surdu.in
    { 0x008F60C,0x247FB12, 0x00,0x00, 0xA,'h' }, // 132: GP42; IntP55; hamP55; rickP55; Closed High Hat; aps042.i; aps042i
    { 0x008F60C,0x2477B12, 0x00,0x05, 0xA,'h' }, // 133: GP44; Pedal High Hat
    { 0x002F60C,0x243CB12, 0x00,0x00, 0xA,'h' }, // 134: GP46; Open High Hat
    { 0x000F60E,0x3029FD0, 0x00,0x00, 0xE,'C' }, // 135: GP49; GP57; hamP0; Crash Cymbal 1; Crash Cymbal 2; crash1i
    { 0x042F80E,0x3E4F407, 0x08,0x4A, 0xE,'C' }, // 136: GP51; GP59; Ride Cymbal 1; Ride Cymbal 2
    { 0x030F50E,0x0029FD0, 0x00,0x0A, 0xE,'C' }, // 137: GP52; hamP19; Chinese Cymbal; aps052i
    { 0x3E4E40E,0x1E5F507, 0x0A,0x5D, 0x6,'b' }, // 138: GP53; Ride Bell
    { 0x004B402,0x0F79705, 0x03,0x0A, 0xE,'M' }, // 139: GP54; Tambourine
    { 0x000F64E,0x3029F9E, 0x00,0x00, 0xE,'C' }, // 140: GP55; Splash Cymbal
    { 0x237F811,0x005F310, 0x45,0x08, 0x8,'B' }, // 141: GP56; Cow Bell
    { 0x303FF80,0x014FF10, 0x00,0x0D, 0xC,'D' }, // 142: GP58; Vibraslap
    { 0x00CF506,0x008F502, 0x0B,0x00, 0x6,'M' }, // 143: GP60; High Bongo
    { 0x0BFFA01,0x097C802, 0x00,0x00, 0x7,'M' }, // 144: GP61; Low Bongo
    { 0x087FA01,0x0B7FA01, 0x51,0x00, 0x6,'D' }, // 145: GP62; Mute High Conga
    { 0x08DFA01,0x0B8F802, 0x54,0x00, 0x6,'D' }, // 146: GP63; Open High Conga
    { 0x088FA01,0x0B6F802, 0x59,0x00, 0x6,'D' }, // 147: GP64; Low Conga
    { 0x30AF901,0x006FA00, 0x00,0x00, 0xE,'M' }, // 148: GP65; hamP8; rickP98; rickP99; High Timbale; timbale; timbale.
    { 0x389F900,0x06CF600, 0x80,0x00, 0xE,'M' }, // 149: GP66; Low Timbale
    { 0x388F803,0x0B6F60C, 0x80,0x08, 0xF,'D' }, // 150: GP67; High Agogo
    { 0x388F803,0x0B6F60C, 0x85,0x00, 0xF,'D' }, // 151: GP68; Low Agogo
    { 0x04F760E,0x2187700, 0x40,0x08, 0xE,'D' }, // 152: GP69; Cabasa
    { 0x049C80E,0x2699B03, 0x40,0x00, 0xE,'D' }, // 153: GP70; Maracas
    { 0x305ADD7,0x0058DC7, 0xDC,0x00, 0xE,'D' }, // 154: GP71; Short Whistle
    { 0x304A8D7,0x00488C7, 0xDC,0x00, 0xE,'D' }, // 155: GP72; Long Whistle
    { 0x306F680,0x3176711, 0x00,0x00, 0xE,'D' }, // 156: GP73; rickP96; Short Guiro; guiros.i
    { 0x205F580,0x3164611, 0x00,0x09, 0xE,'D' }, // 157: GP74; Long Guiro
    { 0x0F40006,0x0F5F715, 0x3F,0x00, 0x1,'D' }, // 158: GP75; Claves
    { 0x3F40006,0x0F5F712, 0x3F,0x00, 0x0,'D' }, // 159: GP76; High Wood Block
    { 0x0F40006,0x0F5F712, 0x3F,0x00, 0x1,'D' }, // 160: GP77; Low Wood Block
    { 0x0E76701,0x0077502, 0x58,0x00, 0x0,'D' }, // 161: GP78; Mute Cuica
    { 0x048F841,0x0057542, 0x45,0x08, 0x0,'D' }, // 162: GP79; Open Cuica
    { 0x3F0E00A,0x005FF1E, 0x40,0x4E, 0x8,'D' }, // 163: GP80; Mute Triangle
    { 0x3F0E00A,0x002FF1E, 0x7C,0x52, 0x8,'D' }, // 164: GP81; Open Triangle
    { 0x04A7A0E,0x21B7B00, 0x40,0x08, 0xE,'D' }, // 165: GP82; hamP7; aps082i
    { 0x3E4E40E,0x1395507, 0x0A,0x40, 0x6,'D' }, // 166: GP83
    { 0x332F905,0x0A5D604, 0x05,0x40, 0xE,'D' }, // 167: GP84
    { 0x3F30002,0x0F5F715, 0x3F,0x00, 0x8,'D' }, // 168: GP85
    { 0x08DFA01,0x0B5F802, 0x4F,0x00, 0x7,'D' }, // 169: GP86
    { 0x1199523,0x0198421, 0x48,0x00, 0x8,'P' }, // 170: HMIGM0; HMIGM29; am029.in
    { 0x054F231,0x056F221, 0x4B,0x00, 0x8,'P' }, // 171: HMIGM1; am001.in
    { 0x055F231,0x076F221, 0x49,0x00, 0x8,'P' }, // 172: HMIGM2; am002.in
    { 0x03BF2B1,0x00BF361, 0x0E,0x00, 0x6,'P' }, // 173: HMIGM3; am003.in
    { 0x038F101,0x028F121, 0x57,0x00, 0x0,'P' }, // 174: HMIGM4; am004.in
    { 0x038F101,0x028F121, 0x93,0x00, 0x0,'P' }, // 175: HMIGM5; am005.in
    { 0x001A221,0x0D5F136, 0x80,0x0E, 0x8,'h' }, // 176: HMIGM6; am006.in
    { 0x0A8C201,0x058C201, 0x92,0x00, 0xA,'c' }, // 177: HMIGM7; am007.in
    { 0x054F60C,0x0B5F381, 0x5C,0x00, 0x0,'c' }, // 178: HMIGM8; am008.in
    { 0x032F607,0x011F511, 0x97,0x80, 0x2,'k' }, // 179: HMIGM9; am009.in
    { 0x0045617,0x004F601, 0x21,0x00, 0x2,'m' }, // 180: HMIGM10; am010.in
    { 0x0E6F318,0x0F6F281, 0x62,0x00, 0x0,'v' }, // 181: HMIGM11; am011.in
    { 0x055F718,0x0D8E521, 0x23,0x00, 0x0,'m' }, // 182: HMIGM12; am012.in
    { 0x0A6F615,0x0E6F601, 0x91,0x00, 0x4,'x' }, // 183: HMIGM13; am013.in
    { 0x082D345,0x0E3A381, 0x59,0x80, 0xC,'b' }, // 184: HMIGM14; am014.in
    { 0x1557403,0x005B381, 0x49,0x80, 0x4,'d' }, // 185: HMIGM15; am015.in
    { 0x122F603,0x0F3F321, 0x87,0x80, 0x6,'G' }, // 186: HMIGM27; am027.in
    { 0x09AA101,0x0DFF221, 0x89,0x40, 0x8,'B' }, // 187: HMIGM37; am037.in
    { 0x15572A1,0x0187121, 0x86,0x0D, 0x0,'T' }, // 188: HMIGM62; am062.in
    { 0x0F00010,0x0F00010, 0x3F,0x3F, 0x0,'?' }, // 189: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in
    { 0x0F1F02E,0x3487407, 0x00,0x07, 0x8,'?' }, // 190: HMIGP27; HMIGP28; HMIGP29; HMIGP30; HMIGP31; Wierd1.i
    { 0x0FE5229,0x3D9850E, 0x00,0x07, 0x6,'?' }, // 191: HMIGP32; HMIGP33; HMIGP34; Wierd2.i
    { 0x0FE6227,0x3D9950A, 0x00,0x07, 0x8,'D' }, // 192: HMIGP35; Wierd3.i
    { 0x0FDF800,0x0C7F601, 0x0B,0x00, 0x8,'D' }, // 193: HMIGP36; Kick.ins
    { 0x0FBF116,0x069F911, 0x08,0x02, 0x0,'D' }, // 194: HMIGP37; HMIGP85; HMIGP86; RimShot.; rimShot.; rimshot.
    { 0x000FF26,0x0A7F802, 0x00,0x02, 0xE,'s' }, // 195: HMIGP38; HMIGP40; Snare.in
    { 0x00F9F30,0x0FAE83A, 0x00,0x00, 0xE,'h' }, // 196: HMIGP39; Clap.ins
    { 0x01FFA06,0x0F5F511, 0x0A,0x00, 0xF,'M' }, // 197: HMIGP41; HMIGP43; HMIGP45; HMIGP47; HMIGP48; HMIGP50; Toms.ins
    { 0x0F1F52E,0x3F99906, 0x05,0x02, 0x0,'h' }, // 198: HMIGP42; HMIGP44; clshat97
    { 0x0F89227,0x3D8750A, 0x00,0x03, 0x8,'h' }, // 199: HMIGP46; Opnhat96
    { 0x2009F2C,0x3A4C50E, 0x00,0x09, 0xE,'C' }, // 200: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym
    { 0x0009429,0x344F904, 0x10,0x0C, 0xE,'C' }, // 201: HMIGP51; HMIGP53; HMIGP59; Ridecym.; ridecym.
    { 0x0F1F52E,0x3F78706, 0x09,0x02, 0x0,'M' }, // 202: HMIGP54; Tamb.ins
    { 0x2F1F535,0x028F703, 0x19,0x02, 0x0,'B' }, // 203: HMIGP56; Cowbell.
    { 0x100FF80,0x1F7F500, 0x00,0x00, 0xC,'D' }, // 204: HMIGP58; vibrasla
    { 0x0FAFA25,0x0F99803, 0xCD,0x00, 0x0,'M' }, // 205: HMIGP60; HMIGP62; mutecong
    { 0x1FAF825,0x0F7A803, 0x1B,0x00, 0x0,'M' }, // 206: HMIGP61; conga.in
    { 0x1FAF825,0x0F69603, 0x21,0x00, 0xE,'D' }, // 207: HMIGP63; HMIGP64; loconga.
    { 0x2F5F504,0x236F603, 0x16,0x03, 0xA,'M' }, // 208: HMIGP65; HMIGP66; timbale.
    { 0x091F015,0x0E8A617, 0x1E,0x04, 0xE,'D' }, // 209: HMIGP67; HMIGP68; agogo.in
    { 0x001FF0E,0x077780E, 0x06,0x04, 0xE,'D' }, // 210: HMIGP69; HMIGP70; HMIGP82; shaker.i
    { 0x2079F20,0x22B950E, 0x1C,0x00, 0x0,'D' }, // 211: HMIGP71; hiwhist.
    { 0x2079F20,0x23B940E, 0x1E,0x00, 0x0,'D' }, // 212: HMIGP72; lowhist.
    { 0x0F7F020,0x33B8809, 0x00,0x00, 0xC,'D' }, // 213: HMIGP73; higuiro.
    { 0x0F7F420,0x33B560A, 0x03,0x00, 0x0,'D' }, // 214: HMIGP74; loguiro.
    { 0x05BF714,0x089F712, 0x4B,0x00, 0x0,'D' }, // 215: HMIGP75; clavecb.
    { 0x0F2FA27,0x09AF612, 0x22,0x00, 0x0,'D' }, // 216: HMIGP76; HMIGP77; woodblok
    { 0x1F75020,0x03B7708, 0x09,0x05, 0x0,'D' }, // 217: HMIGP78; hicuica.
    { 0x1077F26,0x06B7703, 0x29,0x05, 0x0,'D' }, // 218: HMIGP79; locuica.
    { 0x0F0F126,0x0FCF727, 0x44,0x40, 0x6,'D' }, // 219: HMIGP80; mutringl
    { 0x0F0F126,0x0F5F527, 0x44,0x40, 0x6,'D' }, // 220: HMIGP81; HMIGP83; HMIGP84; triangle
    { 0x0F3F821,0x0ADC620, 0x1C,0x00, 0xC,'D' }, // 221: HMIGP87; taiko.in
    { 0x0FFFF01,0x0FFFF01, 0x3F,0x3F, 0x0,'P' }, // 222: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.
    { 0x4FFEE03,0x0FFE804, 0x80,0x00, 0xC,'P' }, // 223: hamM1; intM1; rickM1; DBlock.i; DBlocki
    { 0x122F603,0x0F8F3A1, 0x87,0x80, 0x6,'P' }, // 224: hamM2; intM2; rickM2; GClean.i; GCleani
    { 0x007A810,0x005FA00, 0x86,0x03, 0x6,'P' }, // 225: hamM4; intM4; rickM4; DToms.in; DTomsin
    { 0x053F131,0x227F232, 0x48,0x00, 0x6,'c' }, // 226: hamM7; intM7; rickM7; rickM84; GOverD.i; GOverDi; Guit_fz2
    { 0x01A9161,0x01AC1E6, 0x40,0x03, 0x8,'c' }, // 227: hamM8; intM8; rickM8; GMetal.i; GMetali
    { 0x071FB11,0x0B9F301, 0x00,0x00, 0x0,'k' }, // 228: hamM9; intM9; rickM9; BPick.in; BPickin
    { 0x1B57231,0x098D523, 0x0B,0x00, 0x8,'m' }, // 229: hamM10; intM10; rickM10; BSlap.in; BSlapin
    { 0x024D501,0x0228511, 0x0F,0x00, 0xA,'v' }, // 230: hamM11; intM11; rickM11; BSynth1; BSynth1.
    { 0x025F911,0x034F131, 0x05,0x00, 0xA,'m' }, // 231: hamM12; intM12; rickM12; BSynth2; BSynth2.
    { 0x01576A1,0x0378261, 0x94,0x00, 0xC,'d' }, // 232: hamM15; intM15; rickM15; PSoft.in; PSoftin
    { 0x1362261,0x0084F22, 0x10,0x40, 0x8,'o' }, // 233: hamM18; intM18; rickM18; PRonStr1
    { 0x2363360,0x0084F22, 0x15,0x40, 0xC,'o' }, // 234: hamM19; intM19; rickM19; PRonStr2
    { 0x007F804,0x0748201, 0x0E,0x05, 0x6,'G' }, // 235: hamM25; intM25; rickM25; LTrap.in; LTrapin
    { 0x0E5F131,0x174F131, 0x89,0x00, 0xC,'G' }, // 236: hamM26; intM26; rickM26; LSaw.ins; LSawins
    { 0x0E3F131,0x073F172, 0x8A,0x00, 0xA,'G' }, // 237: hamM27; intM27; rickM27; PolySyn; PolySyn.
    { 0x0FFF101,0x0FF5091, 0x0D,0x80, 0x6,'G' }, // 238: hamM28; intM28; rickM28; Pobo.ins; Poboins
    { 0x1473161,0x007AF61, 0x0F,0x00, 0xA,'G' }, // 239: hamM29; intM29; rickM29; PSweep2; PSweep2.
    { 0x0D3B303,0x024F204, 0x40,0x80, 0x4,'G' }, // 240: hamM30; intM30; rickM30; LBright; LBright.
    { 0x1037531,0x0445462, 0x1A,0x40, 0xE,'G' }, // 241: hamM31; intM31; rickM31; SynStrin
    { 0x021A1A1,0x116C261, 0x92,0x40, 0x6,'B' }, // 242: hamM32; intM32; rickM32; SynStr2; SynStr2.
    { 0x0F0F240,0x0F4F440, 0x00,0x00, 0x4,'B' }, // 243: hamM33; intM33; rickM33; low_blub
    { 0x003F1C0,0x001107E, 0x4F,0x0C, 0x2,'B' }, // 244: hamM34; intM34; rickM34; DInsect; DInsect.
    { 0x0459BC0,0x015F9C1, 0x05,0x00, 0xE,'B' }, // 245: hamM35; intM35; rickM35; hardshak
    { 0x0064F50,0x003FF50, 0x10,0x00, 0x0,'B' }, // 246: hamM37; intM37; rickM37; WUMP.ins; WUMPins
    { 0x2F0F005,0x1B4F600, 0x08,0x00, 0xC,'B' }, // 247: hamM38; intM38; rickM38; DSnare.i; DSnarei
    { 0x0F2F931,0x042F210, 0x40,0x00, 0x4,'B' }, // 248: hamM39; intM39; rickM39; DTimp.in; DTimpin
    { 0x00FFF7E,0x00F2F6E, 0x00,0x00, 0xE,'V' }, // 249: IntP93; hamM40; intM40; rickM40; DRevCym; DRevCym.; drevcym
    { 0x2F95401,0x2FB5401, 0x19,0x00, 0x8,'V' }, // 250: hamM41; intM41; rickM41; Dorky.in; Dorkyin
    { 0x0665F53,0x0077F00, 0x05,0x00, 0x6,'V' }, // 251: hamM42; intM42; rickM42; DFlab.in; DFlabin
    { 0x003F1C0,0x006707E, 0x4F,0x03, 0x2,'V' }, // 252: hamM43; intM43; rickM43; DInsect2
    { 0x1111EF0,0x11411E2, 0x00,0xC0, 0x8,'V' }, // 253: hamM44; intM44; rickM44; DChopper
    { 0x0F0A006,0x075C584, 0x00,0x00, 0xE,'H' }, // 254: IntP50; hamM45; hamP50; intM45; rickM45; rickP50; DShot.in; DShotin; shot.ins; shotins
    { 0x1F5F213,0x0F5F111, 0xC6,0x05, 0x0,'H' }, // 255: hamM46; intM46; rickM46; KickAss; KickAss.
    { 0x153F101,0x274F111, 0x49,0x02, 0x6,'M' }, // 256: hamM47; intM47; rickM47; RVisCool
    { 0x0E4F4D0,0x006A29E, 0x80,0x00, 0x8,'S' }, // 257: hamM48; intM48; rickM48; DSpring; DSpring.
    { 0x0871321,0x0084221, 0xCD,0x80, 0x8,'S' }, // 258: intM49; Chorar22
    { 0x005F010,0x004D011, 0x25,0x80, 0xE,'?' }, // 259: IntP27; hamP27; rickP27; timpani; timpani.
    { 0x065B400,0x075B400, 0x00,0x00, 0x7,'?' }, // 260: IntP28; hamP28; rickP28; timpanib
    { 0x02AF800,0x145F600, 0x03,0x00, 0x4,'?' }, // 261: IntP29; hamP29; rickP29; APS043.i; APS043i
    { 0x0FFF830,0x07FF511, 0x44,0x00, 0x8,'?' }, // 262: IntP30; hamP30; rickP30; mgun3.in; mgun3in
    { 0x0F9F900,0x023F110, 0x08,0x00, 0xA,'?' }, // 263: IntP31; hamP31; rickP31; kick4r.i; kick4ri
    { 0x0F9F900,0x026F180, 0x04,0x00, 0x8,'?' }, // 264: IntP32; hamP32; rickP32; timb1r.i; timb1ri
    { 0x1FDF800,0x059F800, 0xC4,0x00, 0xA,'?' }, // 265: IntP33; hamP33; rickP33; timb2r.i; timb2ri
    { 0x06FFA00,0x08FF600, 0x0B,0x00, 0x0,'D' }, // 266: IntP36; hamP36; rickP36; hartbeat
    { 0x0F9F900,0x023F191, 0x04,0x00, 0x8,'D' }, // 267: IntP37; IntP38; IntP39; IntP40; hamP37; hamP38; hamP39; hamP40; rickP37; rickP38; rickP39; rickP40; tom1r.in; tom1rin
    { 0x097C802,0x097C802, 0x00,0x00, 0x1,'M' }, // 268: IntP41; IntP42; IntP43; IntP44; IntP71; hamP71; rickP41; rickP42; rickP43; rickP44; rickP71; tom2.ins; tom2ins; woodbloc
    { 0x0BFFA01,0x0BFDA02, 0x00,0x00, 0x8,'M' }, // 269: IntP45; hamP45; rickP45; tom.ins; tomins
    { 0x2F0FB01,0x096F701, 0x10,0x00, 0xE,'h' }, // 270: IntP46; IntP47; hamP46; hamP47; rickP46; rickP47; conga.in; congain
    { 0x002FF04,0x007FF00, 0x00,0x00, 0xE,'M' }, // 271: IntP48; hamP48; rickP48; snare01r
    { 0x0F0F006,0x0B7F600, 0x00,0x00, 0xC,'C' }, // 272: IntP49; hamP49; rickP49; slap.ins; slapins
    { 0x0F0F006,0x034C4C4, 0x00,0x03, 0xE,'C' }, // 273: IntP51; hamP51; rickP51; snrsust; snrsust.
    { 0x0F0F019,0x0F7B720, 0x0E,0x0A, 0xE,'C' }, // 274: IntP52; rickP52; snare.in; snarein
    { 0x0F0F006,0x0B4F600, 0x00,0x00, 0xE,'b' }, // 275: IntP53; hamP53; rickP53; synsnar; synsnar.
    { 0x0F0F006,0x0B6F800, 0x00,0x00, 0xE,'M' }, // 276: IntP54; hamP54; rickP54; synsnr1; synsnr1.
    { 0x0F2F931,0x008F210, 0x40,0x00, 0x4,'B' }, // 277: IntP56; hamP56; rickP56; rimshotb
    { 0x0BFFA01,0x0BFDA09, 0x00,0x08, 0x8,'C' }, // 278: IntP57; hamP57; rickP57; rimshot; rimshot.
    { 0x210BA2E,0x2F4B40E, 0x0E,0x00, 0xE,'D' }, // 279: IntP58; IntP59; hamP58; hamP59; rickP58; rickP59; crash.in; crashin
    { 0x210FA2E,0x2F4F40E, 0x0E,0x00, 0xE,'M' }, // 280: IntP60; rickP60; cymbal.i; cymbali
    { 0x2A2B2A4,0x1D49703, 0x02,0x80, 0xE,'M' }, // 281: IntP61; hamP60; hamP61; rickP61; cymbali; cymbals; cymbals.
    { 0x200FF04,0x206FFC3, 0x00,0x00, 0x8,'D' }, // 282: IntP62; hamP62; rickP62; hammer5r
    { 0x200FF04,0x2F5F6C3, 0x00,0x00, 0x8,'D' }, // 283: IntP63; IntP64; hamP63; hamP64; rickP63; rickP64; hammer3; hammer3.
    { 0x0E1C000,0x153951E, 0x80,0x80, 0x6,'M' }, // 284: IntP65; hamP65; rickP65; ride2.in; ride2in
    { 0x200FF03,0x3F6F6C4, 0x00,0x00, 0x8,'M' }, // 285: IntP66; hamP66; rickP66; hammer1; hammer1.
    { 0x202FF4E,0x3F7F701, 0x00,0x00, 0x8,'D' }, // 286: IntP67; IntP68; rickP67; rickP68; tambour; tambour.
    { 0x202FF4E,0x3F6F601, 0x00,0x00, 0x8,'D' }, // 287: IntP69; IntP70; hamP69; hamP70; rickP69; rickP70; tambou2; tambou2.
    { 0x2588A51,0x018A452, 0x00,0x00, 0xC,'D' }, // 288: IntP72; hamP72; rickP72; woodblok
    { 0x0FFFB13,0x0FFE808, 0x40,0x00, 0x8,'D' }, // 289: IntP73; IntP74; rickP73; rickP74; claves.i; clavesi
    { 0x0FFEE03,0x0FFE808, 0x40,0x00, 0xC,'D' }, // 290: IntP75; hamP75; rickP75; claves2; claves2.
    { 0x0FFEE05,0x0FFE808, 0x55,0x00, 0xE,'D' }, // 291: IntP76; hamP76; rickP76; claves3; claves3.
    { 0x0FF0006,0x0FDF715, 0x3F,0x0D, 0x1,'D' }, // 292: IntP77; hamP77; rickP77; clave.in; clavein
    { 0x0F6F80E,0x0F6F80E, 0x00,0x00, 0x0,'D' }, // 293: IntP78; IntP79; hamP78; hamP79; rickP78; rickP79; agogob4; agogob4.
    { 0x060F207,0x072F212, 0x4F,0x09, 0x8,'D' }, // 294: IntP80; hamP80; rickP80; clarion; clarion.
    { 0x061F217,0x074F212, 0x4F,0x08, 0x8,'D' }, // 295: IntP81; hamP81; rickP81; trainbel
    { 0x022FB18,0x012F425, 0x88,0x80, 0x8,'D' }, // 296: IntP82; hamP82; rickP82; gong.ins; gongins
    { 0x0F0FF04,0x0B5F4C1, 0x00,0x00, 0xE,'D' }, // 297: IntP83; hamP83; rickP83; kalimbai
    { 0x02FC811,0x0F5F531, 0x2D,0x00, 0xC,'D' }, // 298: IntP84; IntP85; hamP84; hamP85; rickP84; rickP85; xylo1.in; xylo1in
    { 0x03D6709,0x3FC692C, 0x00,0x00, 0xE,'D' }, // 299: IntP86; hamP86; rickP86; match.in; matchin
    { 0x053D144,0x05642B2, 0x80,0x15, 0xE,'D' }, // 300: IntP87; hamP87; rickP87; breathi; breathi.
    { 0x0F0F007,0x0DC5C00, 0x00,0x00, 0xE,'?' }, // 301: IntP88; rickP88; scratch; scratch.
    { 0x00FFF7E,0x00F3F6E, 0x00,0x00, 0xE,'?' }, // 302: IntP89; hamP89; rickP89; crowd.in; crowdin
    { 0x0B3FA00,0x005D000, 0x00,0x00, 0xC,'?' }, // 303: IntP90; rickP90; taiko.in; taikoin
    { 0x0FFF832,0x07FF511, 0x84,0x00, 0xE,'?' }, // 304: IntP91; hamP91; rickP91; rlog.ins; rlogins
    { 0x0089FD4,0x0089FD4, 0xC0,0xC0, 0x4,'?' }, // 305: IntP92; hamP92; rickP92; knock.in; knockin
    { 0x253B1C4,0x083B1D2, 0x8F,0x84, 0x2,'O' }, // 306: hamM52; rickM94; Fantasy1; fantasy1
    { 0x175F5C2,0x074F2D1, 0x21,0x83, 0xE,'O' }, // 307: hamM53; guitar1
    { 0x1F6FB34,0x04394B1, 0x83,0x00, 0xC,'c' }, // 308: hamM55; hamatmos
    { 0x0658722,0x0186421, 0x46,0x80, 0x0,'T' }, // 309: hamM56; hamcalio
    { 0x0BDF211,0x09BA004, 0x46,0x40, 0x8,'T' }, // 310: hamM59; moonins
    { 0x144F221,0x3457122, 0x8A,0x40, 0x0,'T' }, // 311: hamM62; Polyham3
    { 0x144F221,0x1447122, 0x8A,0x40, 0x0,'T' }, // 312: hamM63; Polyham
    { 0x053F101,0x153F108, 0x40,0x40, 0x0,'X' }, // 313: hamM64; sitar2i
    { 0x102FF00,0x3FFF200, 0x08,0x00, 0x0,'T' }, // 314: hamM70; weird1a
    { 0x144F221,0x345A122, 0x8A,0x40, 0x0,'F' }, // 315: hamM71; Polyham4
    { 0x028F131,0x018F031, 0x0F,0x00, 0xA,'F' }, // 316: hamM72; hamsynbs
    { 0x307D7E1,0x107B6E0, 0x8D,0x00, 0x1,'F' }, // 317: hamM73; Ocasynth
    { 0x03DD500,0x02CD500, 0x11,0x00, 0xA,'G' }, // 318: hamM120; hambass1
    { 0x1199563,0x219C420, 0x46,0x00, 0x8,'X' }, // 319: hamM121; hamguit1
    { 0x044D08C,0x2F4D181, 0xA1,0x80, 0x8,'X' }, // 320: hamM122; hamharm2
    { 0x0022171,0x1035231, 0x93,0x80, 0x0,'X' }, // 321: hamM123; hamvox1
    { 0x1611161,0x01311A2, 0x91,0x80, 0x8,'X' }, // 322: hamM124; hamgob1
    { 0x25666E1,0x02665A1, 0x4C,0x00, 0x0,'X' }, // 323: hamM125; hamblow1
    { 0x144F5C6,0x018F6C1, 0x5C,0x83, 0xE,'?' }, // 324: hamP9; cowbell
    { 0x0D0CCC0,0x028EAC1, 0x10,0x00, 0x0,'?' }, // 325: hamP10; rickP100; conghi.i; conghii
    { 0x038FB00,0x0DAF400, 0x00,0x00, 0x4,'?' }, // 326: hamP12; rickP15; hamkick; kick3.in
    { 0x2BFFB15,0x31FF817, 0x0A,0x00, 0x0,'?' }, // 327: hamP13; rimshot2
    { 0x0F0F01E,0x0B6F70E, 0x00,0x00, 0xE,'?' }, // 328: hamP14; rickP16; hamsnr1; snr1.ins
    { 0x0BFFBC6,0x02FE8C9, 0x00,0x00, 0xE,'?' }, // 329: hamP15; handclap
    { 0x2F0F006,0x2B7F800, 0x00,0x00, 0xE,'?' }, // 330: hamP16; smallsnr
    { 0x0B0900E,0x0BF990E, 0x03,0x03, 0xA,'?' }, // 331: hamP17; rickP95; clsdhhat
    { 0x283E0C4,0x14588C0, 0x81,0x00, 0xE,'?' }, // 332: hamP18; rickP94; openhht2
    { 0x097C802,0x040E000, 0x00,0x00, 0x1,'M' }, // 333: hamP41; hamP42; hamP43; hamP44; tom2ins
    { 0x00FFF2E,0x04AF602, 0x0A,0x1B, 0xE,'C' }, // 334: hamP52; snarein
    { 0x3A5F0EE,0x36786CE, 0x00,0x00, 0xE,'D' }, // 335: hamP67; hamP68; tambour
    { 0x0B0FCD6,0x008BDD6, 0x00,0x05, 0xA,'D' }, // 336: hamP73; hamP74; clavesi
    { 0x0F0F007,0x0DC5C00, 0x08,0x00, 0xE,'?' }, // 337: hamP88; scratch
    { 0x0E7F301,0x078F211, 0x58,0x00, 0xA,'F' }, // 338: rickM76; Bass.ins
    { 0x0EFF230,0x078F521, 0x1E,0x00, 0xE,'F' }, // 339: rickM77; Basnor04
    { 0x019D530,0x01B6171, 0x88,0x80, 0xC,'F' }, // 340: rickM78; Synbass1
    { 0x001F201,0x0B7F211, 0x0D,0x0D, 0xA,'F' }, // 341: rickM79; Synbass2
    { 0x03DD500,0x02CD500, 0x14,0x00, 0xA,'L' }, // 342: rickM80; Pickbass
    { 0x010E032,0x0337D16, 0x87,0x84, 0x8,'L' }, // 343: rickM82; Harpsi1.
    { 0x0F8F161,0x008F062, 0x80,0x80, 0x8,'L' }, // 344: rickM83; Guit_el3
    { 0x0745391,0x0755451, 0x00,0x00, 0xA,'p' }, // 345: rickM88; Orchit2.
    { 0x08E6121,0x09E7231, 0x15,0x00, 0xE,'p' }, // 346: rickM89; Brass11.
    { 0x0BC7321,0x0BC8121, 0x19,0x00, 0xE,'p' }, // 347: rickM90; Brass2.i
    { 0x23C7320,0x0BC8121, 0x19,0x00, 0xE,'p' }, // 348: rickM91; Brass3.i
    { 0x209A060,0x20FF014, 0x02,0x80, 0x1,'p' }, // 349: rickM92; Squ_wave
    { 0x064F207,0x075F612, 0x73,0x00, 0x8,'X' }, // 350: rickM99; Agogo.in
    { 0x2B7F811,0x006F311, 0x46,0x00, 0x8,'?' }, // 351: rickP13; kick1.in
    { 0x218F401,0x008F800, 0x00,0x00, 0xC,'?' }, // 352: rickP17; rickP19; snare1.i; snare4.i
    { 0x0F0F009,0x0F7B700, 0x0E,0x00, 0xE,'?' }, // 353: rickP18; rickP20; snare2.i; snare5.i
    { 0x0FEF812,0x07ED511, 0x47,0x00, 0xE,'?' }, // 354: rickP21; rickP22; rickP23; rickP24; rickP25; rickP26; rocktom.
    { 0x2F4F50E,0x424120CA, 0x00,0x51, 0x3,'?' }, // 355: rickP93; openhht1
    { 0x0DFDCC2,0x026C9C0, 0x17,0x00, 0x0,'?' }, // 356: rickP97; guirol.i
    { 0x0D0ACC0,0x028EAC1, 0x18,0x00, 0x0,'?' }, // 357: rickP101; rickP102; congas2.
    { 0x0A7CDC2,0x028EAC1, 0x2B,0x02, 0x0,'?' }, // 358: rickP103; rickP104; bongos.i
};

const adlinsdata adlins[] =
{
    {   0,  0, 1,0x04 }, // 0: GM0; AcouGrandPiano
    {   1,  0, 1,0x04 }, // 1: GM1; BrightAcouGrand
    {   2,  0, 1,0x04 }, // 2: GM2; ElecGrandPiano
    {   3,  0, 1,0x04 }, // 3: GM3; Honky-tonkPiano
    {   4,  0, 1,0x04 }, // 4: GM4; Rhodes Piano
    {   5,  0, 1,0x04 }, // 5: GM5; Chorused Piano
    {   6,  0, 1,0x04 }, // 6: GM6; Harpsichord
    {   7,  0, 1,0x04 }, // 7: GM7; Clavinet
    {   8,  0, 1,0x04 }, // 8: GM8; Celesta
    {   9,  0, 1,0x04 }, // 9: GM9; Glockenspiel
    {  10,  0, 1,0x04 }, // 10: GM10; Music box
    {  11,  0, 1,0x04 }, // 11: GM11; Vibraphone
    {  12,  0, 1,0x04 }, // 12: GM12; Marimba
    {  13,  0, 1,0x04 }, // 13: GM13; Xylophone
    {  14,  0, 1,0x04 }, // 14: GM14; Tubular Bells
    {  15,  0, 1,0x04 }, // 15: GM15; Dulcimer
    {  16,  0, 1,0x04 }, // 16: GM16; HMIGM16; Hammond Organ; am016.in
    {  17,  0, 1,0x04 }, // 17: GM17; HMIGM17; Percussive Organ; am017.in
    {  18,  0, 1,0x62 }, // 18: GM18; HMIGM18; Rock Organ; am018.in
    {  19,  0, 1,0x04 }, // 19: GM19; HMIGM19; Church Organ; am019.in
    {  20,  0, 1,0x62 }, // 20: GM20; HMIGM20; Reed Organ; am020.in
    {  21,  0, 1,0x0C }, // 21: GM21; HMIGM21; Accordion; am021.in
    {  22,  0, 1,0x04 }, // 22: GM22; HMIGM22; Harmonica; am022.in
    {  23,  0, 1,0x04 }, // 23: GM23; HMIGM23; Tango Accordion; am023.in
    {  24,  0, 1,0x04 }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in
    {  25,  0, 1,0x04 }, // 25: GM25; HMIGM25; Acoustic Guitar2; am025.in
    {  26,  0, 1,0x04 }, // 26: GM26; HMIGM26; Electric Guitar1; am026.in
    {  27,  0, 1,0x04 }, // 27: GM27; Electric Guitar2
    {  28,  0, 1,0x00 }, // 28: GM28; HMIGM28; Electric Guitar3; am028.in
    {  29,  0, 1,0x04 }, // 29: GM29; Overdrive Guitar
    {  30,  0, 1,0x04 }, // 30: GM30; HMIGM30; Distorton Guitar; am030.in
    {  31,  0, 1,0xEE }, // 31: GM31; HMIGM31; Guitar Harmonics; am031.in
    {  32,  0, 1,0xEE }, // 32: GM32; HMIGM32; Acoustic Bass; am032.in
    {  33,  0, 1,0x0C }, // 33: GM33; HMIGM39; Electric Bass 1; am039.in
    {  34,  0, 1,0x04 }, // 34: GM34; HMIGM34; Electric Bass 2; am034.in
    {  35,  0, 1,0x04 }, // 35: GM35; HMIGM35; Fretless Bass; am035.in
    {  36,  0, 1,0x04 }, // 36: GM36; HMIGM36; Slap Bass 1; am036.in
    {  37,  0, 1,0x04 }, // 37: GM37; Slap Bass 2
    {  38,  0, 1,0x04 }, // 38: GM38; HMIGM38; Synth Bass 1; am038.in
    {  33,  0, 1,0x04 }, // 39: GM39; HMIGM33; Synth Bass 2; am033.in
    {  39,  0, 1,0x0C }, // 40: GM40; HMIGM40; Violin; am040.in
    {  40,  0, 1,0x04 }, // 41: GM41; HMIGM41; Viola; am041.in
    {  41,  0, 1,0x04 }, // 42: GM42; HMIGM42; Cello; am042.in
    {  42,  0, 1,0x04 }, // 43: GM43; HMIGM43; Contrabass; am043.in
    {  43,  0, 1,0x04 }, // 44: GM44; HMIGM44; Tremulo Strings; am044.in
    {  44,  0, 1,0x3D }, // 45: GM45; HMIGM45; Pizzicato String; am045.in
    {  45,  0, 1,0x0C }, // 46: GM46; HMIGM46; Orchestral Harp; am046.in
    {  46,  0, 1,0x04 }, // 47: GM47; HMIGM47; Timpany; am047.in
    {  47,  0, 1,0x04 }, // 48: GM48; HMIGM48; String Ensemble1; am048.in
    {  48,  0, 1,0x00 }, // 49: GM49; HMIGM49; String Ensemble2; am049.in
    {  49,  0, 1,0x04 }, // 50: GM50; HMIGM50; Synth Strings 1; am050.in
    {  50,  0, 1,0x04 }, // 51: GM51; HMIGM51; SynthStrings 2; am051.in
    {  51,  0, 1,0x04 }, // 52: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in
    {  52,  0, 1,0x04 }, // 53: GM53; HMIGM53; rickM86; Oohs.ins; Voice Oohs; am053.in
    {  53,  0, 1,0x62 }, // 54: GM54; HMIGM54; Synth Voice; am054.in
    {  54,  0, 1,0x04 }, // 55: GM55; HMIGM55; Orchestra Hit; am055.in
    {  55,  0, 1,0x04 }, // 56: GM56; HMIGM56; Trumpet; am056.in
    {  56,  0, 1,0x04 }, // 57: GM57; HMIGM57; Trombone; am057.in
    {  57,  0, 1,0x00 }, // 58: GM58; HMIGM58; Tuba; am058.in
    {  58,  0, 1,0x62 }, // 59: GM59; HMIGM59; Muted Trumpet; am059.in
    {  59,  0, 1,0x62 }, // 60: GM60; HMIGM60; French Horn; am060.in
    {  60,  0, 1,0x04 }, // 61: GM61; HMIGM61; Brass Section; am061.in
    {  61,  0, 1,0x04 }, // 62: GM62; Synth Brass 1
    {  62,  0, 1,0x62 }, // 63: GM63; HMIGM63; Synth Brass 2; am063.in
    {  63,  0, 1,0x04 }, // 64: GM64; HMIGM64; Soprano Sax; am064.in
    {  64,  0, 1,0x00 }, // 65: GM65; HMIGM65; Alto Sax; am065.in
    {  65,  0, 1,0x00 }, // 66: GM66; HMIGM66; Tenor Sax; am066.in
    {  66,  0, 1,0x62 }, // 67: GM67; HMIGM67; Baritone Sax; am067.in
    {  67,  0, 1,0x04 }, // 68: GM68; HMIGM68; Oboe; am068.in
    {  68,  0, 1,0x04 }, // 69: GM69; HMIGM69; English Horn; am069.in
    {  69,  0, 1,0x04 }, // 70: GM70; HMIGM70; Bassoon; am070.in
    {  70,  0, 1,0x62 }, // 71: GM71; HMIGM71; Clarinet; am071.in
    {  71,  0, 1,0x04 }, // 72: GM72; HMIGM72; Piccolo; am072.in
    {  72,  0, 1,0x3D }, // 73: GM73; HMIGM73; Flute; am073.in
    {  73,  0, 1,0x62 }, // 74: GM74; HMIGM74; Recorder; am074.in
    {  74,  0, 1,0x04 }, // 75: GM75; HMIGM75; Pan Flute; am075.in
    {  75,  0, 1,0x04 }, // 76: GM76; HMIGM76; Bottle Blow; am076.in
    {  76,  0, 1,0x04 }, // 77: GM77; HMIGM77; Shakuhachi; am077.in
    {  77,  0, 0,0x04 }, // 78: GM78; HMIGM78; Whistle; am078.in
    {  78,  0, 0,0x04 }, // 79: GM79; HMIGM79; Ocarina; am079.in
    {  79,  0, 1,0x04 }, // 80: GM80; HMIGM80; Lead 1 squareea; am080.in
    {  80,  0, 1,0xF4 }, // 81: GM81; HMIGM81; Lead 2 sawtooth; am081.in
    {  81,  0, 1,0x04 }, // 82: GM82; HMIGM82; Lead 3 calliope; am082.in
    {  82,  0, 1,0x04 }, // 83: GM83; HMIGM83; Lead 4 chiff; am083.in
    {  83,  0, 1,0x04 }, // 84: GM84; HMIGM84; Lead 5 charang; am084.in
    {  84,  0, 1,0x04 }, // 85: GM85; HMIGM85; rickM87; Lead 6 voice; Solovox.; am085.in
    {  85,  0, 1,0x04 }, // 86: GM86; HMIGM86; rickM93; Lead 7 fifths; Saw_wave; am086.in
    {  86,  0, 1,0x04 }, // 87: GM87; HMIGM87; Lead 8 brass; am087.in
    {  87,  0, 1,0x04 }, // 88: GM88; HMIGM88; Pad 1 new age; am088.in
    {  88,  0, 1,0x04 }, // 89: GM89; HMIGM89; Pad 2 warm; am089.in
    {  89,  0, 1,0x04 }, // 90: GM90; HMIGM90; Pad 3 polysynth; am090.in
    {  90,  0, 1,0x04 }, // 91: GM91; HMIGM91; rickM95; Pad 4 choir; Spacevo.; am091.in
    {  91,  0, 1,0x04 }, // 92: GM92; HMIGM92; Pad 5 bowedpad; am092.in
    {  92,  0, 1,0x04 }, // 93: GM93; HMIGM93; Pad 6 metallic; am093.in
    {  93,  0, 1,0x04 }, // 94: GM94; HMIGM94; Pad 7 halo; am094.in
    {  94,  0, 1,0x04 }, // 95: GM95; HMIGM95; Pad 8 sweep; am095.in
    {  95,  0, 1,0x04 }, // 96: GM96; HMIGM96; FX 1 rain; am096.in
    {  96,  0, 1,0x04 }, // 97: GM97; HMIGM97; FX 2 soundtrack; am097.in
    {  97,  0, 1,0xF4 }, // 98: GM98; HMIGM98; FX 3 crystal; am098.in
    {  98,  0, 1,0x04 }, // 99: GM99; HMIGM99; FX 4 atmosphere; am099.in
    {  99,  0, 1,0xF4 }, // 100: GM100; HMIGM100; hamM51; FX 5 brightness; am100.in; am100in
    { 100,  0, 1,0x04 }, // 101: GM101; HMIGM101; FX 6 goblins; am101.in
    { 101,  0, 1,0x3D }, // 102: GM102; HMIGM102; rickM98; Echodrp1; FX 7 echoes; am102.in
    { 102,  0, 1,0x04 }, // 103: GM103; HMIGM103; FX 8 sci-fi; am103.in
    { 103,  0, 1,0x04 }, // 104: GM104; HMIGM104; Sitar; am104.in
    { 104,  0, 1,0x04 }, // 105: GM105; HMIGM105; Banjo; am105.in
    { 105,  0, 1,0x04 }, // 106: GM106; HMIGM106; Shamisen; am106.in
    { 106,  0, 1,0x04 }, // 107: GM107; HMIGM107; Koto; am107.in
    { 107,  0, 1,0x04 }, // 108: GM108; HMIGM108; Kalimba; am108.in
    { 108,  0, 1,0xF4 }, // 109: GM109; HMIGM109; Bagpipe; am109.in
    { 109,  0, 1,0xF4 }, // 110: GM110; HMIGM110; Fiddle; am110.in
    { 110,  0, 1,0x04 }, // 111: GM111; HMIGM111; Shanai; am111.in
    { 111,  0, 1,0x04 }, // 112: GM112; HMIGM112; Tinkle Bell; am112.in
    { 112,  0, 1,0x04 }, // 113: GM113; HMIGM113; Agogo Bells; am113.in
    { 113,  0, 1,0x04 }, // 114: GM114; HMIGM114; Steel Drums; am114.in
    { 114,  0, 1,0x04 }, // 115: GM115; HMIGM115; Woodblock; am115.in
    { 115,  0, 1,0x04 }, // 116: GM116; HMIGM116; Taiko Drum; am116.in
    { 116,  0, 1,0x04 }, // 117: GM117; HMIGM117; Melodic Tom; am117.in
    { 117,  0, 1,0x62 }, // 118: GM118; HMIGM118; Synth Drum; am118.in
    { 118,  0, 1,0xF4 }, // 119: GM119; HMIGM119; Reverse Cymbal; am119.in
    { 119,  0, 1,0x04 }, // 120: GM120; HMIGM120; rickM101; Fretnos.; Guitar FretNoise; am120.in
    { 120,  0, 1,0x04 }, // 121: GM121; HMIGM121; Breath Noise; am121.in
    { 121,  0, 1,0x62 }, // 122: GM122; HMIGM122; Seashore; am122.in
    { 122,  0, 1,0x0C }, // 123: GM123; HMIGM123; Bird Tweet; am123.in
    { 123,  0, 1,0x04 }, // 124: GM124; HMIGM124; Telephone; am124.in
    { 124,  0, 1,0x62 }, // 125: GM125; HMIGM125; Helicopter; am125.in
    { 125,  0, 1,0x04 }, // 126: GM126; HMIGM126; Applause/Noise; am126.in
    { 126,  0, 1,0x04 }, // 127: GM127; HMIGM127; Gunshot; am127.in
    { 127, 35, 1,0x04 }, // 128: GP35; GP36; Ac Bass Drum; Bass Drum 1
    { 128, 52, 1,0x04 }, // 129: GP37; Side Stick
    { 129, 48, 1,0x04 }, // 130: GP38; Acoustic Snare
    { 130, 58, 1,0x04 }, // 131: GP39; Hand Clap
    { 129, 60, 1,0x04 }, // 132: GP40; Electric Snare
    { 131, 47, 1,0x04 }, // 133: GP41; hamP1; Low Floor Tom; aps041i
    { 132, 43, 1,0x04 }, // 134: GP42; Closed High Hat
    { 131, 49, 1,0x04 }, // 135: GP43; hamP2; High Floor Tom; aps041i
    { 133, 43, 1,0x04 }, // 136: GP44; Pedal High Hat
    { 131, 51, 1,0x04 }, // 137: GP45; hamP3; Low Tom; aps041i
    { 134, 43, 1,0x04 }, // 138: GP46; Open High Hat
    { 131, 54, 1,0x04 }, // 139: GP47; hamP4; Low-Mid Tom; aps041i
    { 131, 57, 1,0x04 }, // 140: GP48; hamP5; High-Mid Tom; aps041i
    { 135, 72, 1,0x62 }, // 141: GP49; Crash Cymbal 1
    { 131, 60, 1,0x04 }, // 142: GP50; hamP6; High Tom; aps041i
    { 136, 76, 1,0x04 }, // 143: GP51; Ride Cymbal 1
    { 137, 84, 1,0x04 }, // 144: GP52; hamP19; Chinese Cymbal; aps052i
    { 138, 36, 1,0x04 }, // 145: GP53; Ride Bell
    { 139, 65, 1,0x04 }, // 146: GP54; Tambourine
    { 140, 84, 1,0x04 }, // 147: GP55; Splash Cymbal
    { 141, 83, 1,0x04 }, // 148: GP56; Cow Bell
    { 135, 84, 1,0x04 }, // 149: GP57; Crash Cymbal 2
    { 142, 24, 1,0x04 }, // 150: GP58; Vibraslap
    { 136, 77, 1,0x04 }, // 151: GP59; Ride Cymbal 2
    { 143, 60, 1,0x04 }, // 152: GP60; High Bongo
    { 144, 65, 1,0x04 }, // 153: GP61; Low Bongo
    { 145, 59, 1,0x04 }, // 154: GP62; Mute High Conga
    { 146, 51, 1,0x04 }, // 155: GP63; Open High Conga
    { 147, 45, 1,0x04 }, // 156: GP64; Low Conga
    { 148, 71, 1,0x62 }, // 157: GP65; hamP8; rickP98; High Timbale; timbale; timbale.
    { 149, 60, 1,0x04 }, // 158: GP66; Low Timbale
    { 150, 58, 1,0x04 }, // 159: GP67; High Agogo
    { 151, 53, 1,0x04 }, // 160: GP68; Low Agogo
    { 152, 64, 1,0x04 }, // 161: GP69; Cabasa
    { 153, 71, 1,0x04 }, // 162: GP70; Maracas
    { 154, 61, 1,0x04 }, // 163: GP71; Short Whistle
    { 155, 61, 1,0x04 }, // 164: GP72; Long Whistle
    { 156, 44, 1,0x04 }, // 165: GP73; rickP96; Short Guiro; guiros.i
    { 157, 40, 1,0x04 }, // 166: GP74; Long Guiro
    { 158, 69, 1,0x04 }, // 167: GP75; Claves
    { 159, 68, 1,0x04 }, // 168: GP76; High Wood Block
    { 160, 63, 1,0x04 }, // 169: GP77; Low Wood Block
    { 161, 74, 1,0x04 }, // 170: GP78; Mute Cuica
    { 162, 60, 1,0x04 }, // 171: GP79; Open Cuica
    { 163, 80, 1,0x04 }, // 172: GP80; Mute Triangle
    { 164, 64, 1,0x04 }, // 173: GP81; Open Triangle
    { 165, 72, 1,0x04 }, // 174: GP82; hamP7; aps082i
    { 166, 73, 1,0x04 }, // 175: GP83
    { 167, 70, 1,0x04 }, // 176: GP84
    { 168, 68, 1,0x04 }, // 177: GP85
    { 169, 48, 1,0x04 }, // 178: GP86
    { 131, 53, 1,0x04 }, // 179: GP87
    { 170,  0, 1,0x62 }, // 180: HMIGM0; HMIGM29; am029.in
    { 171,  0, 1,0x04 }, // 181: HMIGM1; am001.in
    { 172,  0, 1,0x62 }, // 182: HMIGM2; am002.in
    { 173,  0, 1,0x62 }, // 183: HMIGM3; am003.in
    { 174,  0, 1,0x62 }, // 184: HMIGM4; am004.in
    { 175,  0, 1,0x62 }, // 185: HMIGM5; am005.in
    { 176,  0, 1,0x3D }, // 186: HMIGM6; am006.in
    { 177,  0, 1,0x62 }, // 187: HMIGM7; am007.in
    { 178,  0, 1,0x62 }, // 188: HMIGM8; am008.in
    { 179,  0, 1,0x62 }, // 189: HMIGM9; am009.in
    { 180,  0, 1,0x04 }, // 190: HMIGM10; am010.in
    { 181,  0, 1,0x04 }, // 191: HMIGM11; am011.in
    { 182,  0, 1,0x04 }, // 192: HMIGM12; am012.in
    { 183,  0, 1,0x04 }, // 193: HMIGM13; am013.in
    { 184,  0, 1,0x00 }, // 194: HMIGM14; am014.in
    { 185,  0, 1,0x04 }, // 195: HMIGM15; am015.in
    { 186,  0, 1,0x04 }, // 196: HMIGM27; am027.in
    { 187,  0, 1,0x3D }, // 197: HMIGM37; am037.in
    { 188,  0, 1,0x04 }, // 198: HMIGM62; am062.in
    { 189, 60, 1,0x3E }, // 199: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in
    { 190, 73, 1,0x06 }, // 200: HMIGP27; Wierd1.i
    { 190, 74, 1,0x06 }, // 201: HMIGP28; Wierd1.i
    { 190, 80, 1,0x06 }, // 202: HMIGP29; Wierd1.i
    { 190, 84, 1,0x06 }, // 203: HMIGP30; Wierd1.i
    { 190, 92, 1,0x06 }, // 204: HMIGP31; Wierd1.i
    { 191, 81, 1,0x3E }, // 205: HMIGP32; Wierd2.i
    { 191, 83, 1,0x3E }, // 206: HMIGP33; Wierd2.i
    { 191, 95, 1,0x3E }, // 207: HMIGP34; Wierd2.i
    { 192, 83, 1,0x3E }, // 208: HMIGP35; Wierd3.i
    { 193, 35, 1,0x06 }, // 209: HMIGP36; Kick.ins
    { 194, 36, 1,0x06 }, // 210: HMIGP37; HMIGP86; RimShot.; rimshot.
    { 195, 60, 1,0x06 }, // 211: HMIGP38; Snare.in
    { 196, 59, 1,0x06 }, // 212: HMIGP39; Clap.ins
    { 195, 44, 1,0x06 }, // 213: HMIGP40; Snare.in
    { 197, 41, 0,0x06 }, // 214: HMIGP41; Toms.ins
    { 198, 97, 1,0x3E }, // 215: HMIGP42; HMIGP44; clshat97
    { 197, 44, 0,0x06 }, // 216: HMIGP43; Toms.ins
    { 197, 48, 0,0x06 }, // 217: HMIGP45; Toms.ins
    { 199, 96, 1,0x06 }, // 218: HMIGP46; Opnhat96
    { 197, 51, 0,0x06 }, // 219: HMIGP47; Toms.ins
    { 197, 54, 0,0x06 }, // 220: HMIGP48; Toms.ins
    { 200, 40, 1,0x06 }, // 221: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym
    { 197, 57, 0,0x06 }, // 222: HMIGP50; Toms.ins
    { 201, 58, 1,0x06 }, // 223: HMIGP51; HMIGP53; Ridecym.
    { 202, 97, 1,0x06 }, // 224: HMIGP54; Tamb.ins
    { 203, 50, 1,0x06 }, // 225: HMIGP56; Cowbell.
    { 204, 28, 1,0x06 }, // 226: HMIGP58; vibrasla
    { 201, 60, 1,0x06 }, // 227: HMIGP59; ridecym.
    { 205, 53, 1,0x06 }, // 228: HMIGP60; mutecong
    { 206, 46, 1,0x06 }, // 229: HMIGP61; conga.in
    { 205, 57, 1,0x06 }, // 230: HMIGP62; mutecong
    { 207, 42, 1,0x06 }, // 231: HMIGP63; loconga.
    { 207, 37, 1,0x06 }, // 232: HMIGP64; loconga.
    { 208, 41, 1,0x06 }, // 233: HMIGP65; timbale.
    { 208, 37, 1,0x06 }, // 234: HMIGP66; timbale.
    { 209, 77, 1,0x06 }, // 235: HMIGP67; agogo.in
    { 209, 72, 1,0x06 }, // 236: HMIGP68; agogo.in
    { 210, 70, 1,0x06 }, // 237: HMIGP69; HMIGP82; shaker.i
    { 210, 90, 1,0x06 }, // 238: HMIGP70; shaker.i
    { 211, 39, 1,0x3E }, // 239: HMIGP71; hiwhist.
    { 212, 36, 1,0x3E }, // 240: HMIGP72; lowhist.
    { 213, 46, 1,0x06 }, // 241: HMIGP73; higuiro.
    { 214, 48, 1,0x06 }, // 242: HMIGP74; loguiro.
    { 215, 85, 1,0x4E }, // 243: HMIGP75; clavecb.
    { 216, 66, 1,0x06 }, // 244: HMIGP76; woodblok
    { 216, 61, 1,0x06 }, // 245: HMIGP77; woodblok
    { 217, 41, 1,0x3E }, // 246: HMIGP78; hicuica.
    { 218, 41, 1,0x3E }, // 247: HMIGP79; locuica.
    { 219, 81, 1,0x4E }, // 248: HMIGP80; mutringl
    { 220, 81, 1,0x26 }, // 249: HMIGP81; triangle
    { 220, 76, 1,0x26 }, // 250: HMIGP83; triangle
    { 220,103, 1,0x26 }, // 251: HMIGP84; triangle
    { 194, 60, 1,0x06 }, // 252: HMIGP85; rimShot.
    { 221, 53, 1,0x06 }, // 253: HMIGP87; taiko.in
    { 222,  0, 0,0x00 }, // 254: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.
    { 223,  0, 0,0x00 }, // 255: hamM1; intM1; rickM1; DBlock.i; DBlocki
    { 224,  0, 0,0x00 }, // 256: hamM2; intM2; rickM2; GClean.i; GCleani
    {  28,  0, 0,0x00 }, // 257: hamM3; intM3; rickM3; BPerc.in; BPercin
    { 225,  0, 0,0x00 }, // 258: hamM4; intM4; rickM4; DToms.in; DTomsin
    {  31,  0, 0,0x00 }, // 259: hamM5; intM5; rickM5; GFeedbck
    {  30,  0, 0,0x00 }, // 260: hamM6; intM6; rickM6; GDist.in; GDistin
    { 226,  0, 0,0x00 }, // 261: hamM7; intM7; rickM7; GOverD.i; GOverDi
    { 227,  0, 0,0x00 }, // 262: hamM8; intM8; rickM8; GMetal.i; GMetali
    { 228,  0, 0,0x00 }, // 263: hamM9; intM9; rickM9; BPick.in; BPickin
    { 229,  0, 0,0x00 }, // 264: hamM10; intM10; rickM10; BSlap.in; BSlapin
    { 230,  0, 0,0x00 }, // 265: hamM11; intM11; rickM11; BSynth1; BSynth1.
    { 231,  0, 0,0x00 }, // 266: hamM12; intM12; rickM12; BSynth2; BSynth2.
    {  38,  0, 0,0x00 }, // 267: hamM13; intM13; rickM13; BSynth3; BSynth3.
    {  46,  0, 0,0x00 }, // 268: hamM14; intM14; rickM14; BSynth4; BSynth4.
    { 232,  0, 0,0x00 }, // 269: hamM15; intM15; rickM15; PSoft.in; PSoftin
    {  79,  0, 0,0x00 }, // 270: hamM16; intM16; rickM16; LSquare; LSquare.
    {  84,  0, 0,0x00 }, // 271: hamM17; intM17; rickM17; PFlutes; PFlutes.
    { 233,  0, 0,0x00 }, // 272: hamM18; intM18; rickM18; PRonStr1
    { 234,  0, 0,0x00 }, // 273: hamM19; intM19; rickM19; PRonStr2
    {  49,  0, 0,0x00 }, // 274: hamM20; intM20; rickM20; PMellow; PMellow.
    {  89,  0, 0,0x00 }, // 275: hamM21; intM21; rickM21; LTriang; LTriang.
    {  92,  0, 0,0x00 }, // 276: hamM22; intM22; rickM22; PSlow.in; PSlowin
    {  93,  0, 0,0x00 }, // 277: hamM23; intM23; rickM23; PSweep.i; PSweepi
    { 105,  0, 0,0x00 }, // 278: hamM24; intM24; rickM24; LDist.in; LDistin
    { 235,  0, 0,0x00 }, // 279: hamM25; intM25; rickM25; LTrap.in; LTrapin
    { 236,  0, 0,0x00 }, // 280: hamM26; intM26; rickM26; LSaw.ins; LSawins
    { 237,  0, 0,0x00 }, // 281: hamM27; intM27; rickM27; PolySyn; PolySyn.
    { 238,  0, 0,0x00 }, // 282: hamM28; intM28; rickM28; Pobo.ins; Poboins
    { 239,  0, 0,0x00 }, // 283: hamM29; intM29; rickM29; PSweep2; PSweep2.
    { 240,  0, 0,0x00 }, // 284: hamM30; intM30; rickM30; LBright; LBright.
    { 241,  0, 0,0x00 }, // 285: hamM31; intM31; rickM31; SynStrin
    { 242,  0, 0,0x00 }, // 286: hamM32; intM32; rickM32; SynStr2; SynStr2.
    { 243,  0, 0,0x00 }, // 287: hamM33; intM33; rickM33; low_blub
    { 244,  0, 0,0x00 }, // 288: hamM34; intM34; rickM34; DInsect; DInsect.
    { 245,  0, 0,0x00 }, // 289: hamM35; intM35; rickM35; hardshak
    { 119,  0, 0,0x00 }, // 290: hamM36; intM36; rickM36; DNoise1; DNoise1.
    { 246,  0, 0,0x00 }, // 291: hamM37; intM37; rickM37; WUMP.ins; WUMPins
    { 247,  0, 0,0x00 }, // 292: hamM38; intM38; rickM38; DSnare.i; DSnarei
    { 248,  0, 0,0x00 }, // 293: hamM39; intM39; rickM39; DTimp.in; DTimpin
    { 249,  0, 0,0x00 }, // 294: hamM40; intM40; rickM40; DRevCym; DRevCym.
    { 250,  0, 0,0x00 }, // 295: hamM41; intM41; rickM41; Dorky.in; Dorkyin
    { 251,  0, 0,0x00 }, // 296: hamM42; intM42; rickM42; DFlab.in; DFlabin
    { 252,  0, 0,0x00 }, // 297: hamM43; intM43; rickM43; DInsect2
    { 253,  0, 0,0x00 }, // 298: hamM44; intM44; rickM44; DChopper
    { 254,  0, 0,0x00 }, // 299: hamM45; intM45; rickM45; DShot.in; DShotin
    { 255,  0, 0,0x00 }, // 300: hamM46; intM46; rickM46; KickAss; KickAss.
    { 256,  0, 0,0x00 }, // 301: hamM47; intM47; rickM47; RVisCool
    { 257,  0, 0,0x00 }, // 302: hamM48; intM48; rickM48; DSpring; DSpring.
    { 258,  0, 0,0x00 }, // 303: intM49; Chorar22
    { 259, 36, 0,0x00 }, // 304: IntP27; hamP27; rickP27; timpani; timpani.
    { 260, 50, 0,0x00 }, // 305: IntP28; hamP28; rickP28; timpanib
    { 261, 37, 0,0x00 }, // 306: IntP29; rickP29; APS043.i; APS043i
    { 262, 39, 0,0x00 }, // 307: IntP30; rickP30; mgun3.in; mgun3in
    { 263, 39, 0,0x00 }, // 308: IntP31; hamP31; rickP31; kick4r.i; kick4ri
    { 264, 86, 0,0x00 }, // 309: IntP32; hamP32; rickP32; timb1r.i; timb1ri
    { 265, 43, 0,0x00 }, // 310: IntP33; hamP33; rickP33; timb2r.i; timb2ri
    { 127, 24, 0,0x00 }, // 311: IntP34; rickP34; apo035.i; apo035i
    { 127, 29, 0,0x00 }, // 312: IntP35; rickP35; apo035.i; apo035i
    { 266, 50, 0,0x00 }, // 313: IntP36; hamP36; rickP36; hartbeat
    { 267, 30, 0,0x00 }, // 314: IntP37; hamP37; rickP37; tom1r.in; tom1rin
    { 267, 33, 0,0x00 }, // 315: IntP38; hamP38; rickP38; tom1r.in; tom1rin
    { 267, 38, 0,0x00 }, // 316: IntP39; hamP39; rickP39; tom1r.in; tom1rin
    { 267, 42, 0,0x00 }, // 317: IntP40; hamP40; rickP40; tom1r.in; tom1rin
    { 268, 24, 0,0x00 }, // 318: IntP41; rickP41; tom2.ins; tom2ins
    { 268, 27, 0,0x00 }, // 319: IntP42; rickP42; tom2.ins; tom2ins
    { 268, 29, 0,0x00 }, // 320: IntP43; rickP43; tom2.ins; tom2ins
    { 268, 32, 0,0x00 }, // 321: IntP44; rickP44; tom2.ins; tom2ins
    { 269, 32, 0,0x00 }, // 322: IntP45; rickP45; tom.ins; tomins
    { 270, 53, 0,0x00 }, // 323: IntP46; hamP46; rickP46; conga.in; congain
    { 270, 57, 0,0x00 }, // 324: IntP47; hamP47; rickP47; conga.in; congain
    { 271, 60, 0,0x00 }, // 325: IntP48; hamP48; rickP48; snare01r
    { 272, 55, 0,0x00 }, // 326: IntP49; hamP49; rickP49; slap.ins; slapins
    { 254, 85, 0,0x00 }, // 327: IntP50; hamP50; rickP50; shot.ins; shotins
    { 273, 90, 0,0x00 }, // 328: IntP51; rickP51; snrsust; snrsust.
    { 274, 84, 0,0x00 }, // 329: IntP52; rickP52; snare.in; snarein
    { 275, 48, 0,0x00 }, // 330: IntP53; hamP53; rickP53; synsnar; synsnar.
    { 276, 48, 0,0x00 }, // 331: IntP54; rickP54; synsnr1; synsnr1.
    { 132, 72, 0,0x00 }, // 332: IntP55; rickP55; aps042.i; aps042i
    { 277, 72, 0,0x00 }, // 333: IntP56; hamP56; rickP56; rimshotb
    { 278, 72, 0,0x00 }, // 334: IntP57; rickP57; rimshot; rimshot.
    { 279, 63, 0,0x00 }, // 335: IntP58; hamP58; rickP58; crash.in; crashin
    { 279, 65, 0,0x00 }, // 336: IntP59; hamP59; rickP59; crash.in; crashin
    { 280, 79, 0,0x00 }, // 337: IntP60; rickP60; cymbal.i; cymbali
    { 281, 38, 0,0x00 }, // 338: IntP61; hamP61; rickP61; cymbals; cymbals.
    { 282, 94, 0,0x00 }, // 339: IntP62; hamP62; rickP62; hammer5r
    { 283, 87, 0,0x00 }, // 340: IntP63; hamP63; rickP63; hammer3; hammer3.
    { 283, 94, 0,0x00 }, // 341: IntP64; hamP64; rickP64; hammer3; hammer3.
    { 284, 80, 0,0x00 }, // 342: IntP65; hamP65; rickP65; ride2.in; ride2in
    { 285, 47, 0,0x00 }, // 343: IntP66; hamP66; rickP66; hammer1; hammer1.
    { 286, 61, 0,0x00 }, // 344: IntP67; rickP67; tambour; tambour.
    { 286, 68, 0,0x00 }, // 345: IntP68; rickP68; tambour; tambour.
    { 287, 61, 0,0x00 }, // 346: IntP69; hamP69; rickP69; tambou2; tambou2.
    { 287, 68, 0,0x00 }, // 347: IntP70; hamP70; rickP70; tambou2; tambou2.
    { 268, 60, 0,0x00 }, // 348: IntP71; hamP71; rickP71; woodbloc
    { 288, 60, 0,0x00 }, // 349: IntP72; hamP72; rickP72; woodblok
    { 289, 36, 0,0x00 }, // 350: IntP73; rickP73; claves.i; clavesi
    { 289, 60, 0,0x00 }, // 351: IntP74; rickP74; claves.i; clavesi
    { 290, 60, 0,0x00 }, // 352: IntP75; hamP75; rickP75; claves2; claves2.
    { 291, 60, 0,0x00 }, // 353: IntP76; hamP76; rickP76; claves3; claves3.
    { 292, 68, 0,0x00 }, // 354: IntP77; hamP77; rickP77; clave.in; clavein
    { 293, 71, 0,0x00 }, // 355: IntP78; hamP78; rickP78; agogob4; agogob4.
    { 293, 72, 0,0x00 }, // 356: IntP79; hamP79; rickP79; agogob4; agogob4.
    { 294,101, 0,0x00 }, // 357: IntP80; hamP80; rickP80; clarion; clarion.
    { 295, 36, 0,0x00 }, // 358: IntP81; rickP81; trainbel
    { 296, 25, 0,0x00 }, // 359: IntP82; hamP82; rickP82; gong.ins; gongins
    { 297, 37, 0,0x00 }, // 360: IntP83; hamP83; rickP83; kalimbai
    { 298, 36, 0,0x00 }, // 361: IntP84; rickP84; xylo1.in; xylo1in
    { 298, 41, 0,0x00 }, // 362: IntP85; rickP85; xylo1.in; xylo1in
    { 299, 84, 0,0x00 }, // 363: IntP86; hamP86; rickP86; match.in; matchin
    { 300, 54, 0,0x00 }, // 364: IntP87; hamP87; rickP87; breathi; breathi.
    { 301, 36, 0,0x00 }, // 365: IntP88; rickP88; scratch; scratch.
    { 302, 60, 0,0x00 }, // 366: IntP89; hamP89; rickP89; crowd.in; crowdin
    { 303, 37, 0,0x00 }, // 367: IntP90; rickP90; taiko.in; taikoin
    { 304, 36, 0,0x00 }, // 368: IntP91; rickP91; rlog.ins; rlogins
    { 305, 32, 0,0x00 }, // 369: IntP92; hamP92; rickP92; knock.in; knockin
    { 249, 48, 0,0x00 }, // 370: IntP93; drevcym
    { 112,  0, 1,0x62 }, // 371: hamM50; agogoin
    { 306,  0, 1,0x62 }, // 372: hamM52; rickM94; Fantasy1; fantasy1
    { 307,  0, 1,0x62 }, // 373: hamM53; guitar1
    {  93,  0, 1,0x62 }, // 374: hamM54; rickM96; Halopad.; halopad
    { 308,  0, 1,0x62 }, // 375: hamM55; hamatmos
    { 309,  0, 1,0x62 }, // 376: hamM56; hamcalio
    { 107,  0, 1,0x62 }, // 377: hamM57; kalimba
    { 116,  0, 1,0x62 }, // 378: hamM58; melotom
    { 310,  0, 1,0x62 }, // 379: hamM59; moonins
    {  28,  0, 1,0x62 }, // 380: hamM60; muteguit
    {  78,  0, 0,0x62 }, // 381: hamM61; ocarina
    { 311,  0, 1,0x62 }, // 382: hamM62; Polyham3
    { 312,  0, 1,0x62 }, // 383: hamM63; Polyham
    { 313,  0, 1,0x62 }, // 384: hamM64; sitar2i
    {  79,  0, 1,0x62 }, // 385: hamM65; squarewv
    {  94,  0, 1,0x62 }, // 386: hamM66; rickM97; Sweepad.; sweepad
    {  38,  0, 1,0x62 }, // 387: hamM67; synbass1
    {  33,  0, 1,0x62 }, // 388: hamM68; synbass2
    { 115,  0, 1,0x62 }, // 389: hamM69; Taikoin
    { 314,  0, 1,0x62 }, // 390: hamM70; weird1a
    { 315,  0, 1,0x62 }, // 391: hamM71; Polyham4
    { 316,  0, 1,0x62 }, // 392: hamM72; hamsynbs
    { 317,  0, 0,0x62 }, // 393: hamM73; Ocasynth
    { 318,  0, 1,0x62 }, // 394: hamM120; hambass1
    { 319,  0, 1,0x62 }, // 395: hamM121; hamguit1
    { 320,  0, 1,0x62 }, // 396: hamM122; hamharm2
    { 321,  0, 1,0x62 }, // 397: hamM123; hamvox1
    { 322,  0, 1,0x62 }, // 398: hamM124; hamgob1
    { 323,  0, 1,0x62 }, // 399: hamM125; hamblow1
    { 135, 49, 1,0x62 }, // 400: hamP0; crash1i
    { 324, 72, 1,0x62 }, // 401: hamP9; cowbell
    { 325, 74, 1,0x62 }, // 402: hamP10; rickP100; conghi.i; conghii
    { 127, 35, 1,0x3D }, // 403: hamP11; rickP14; aps035i; kick2.in
    { 326, 35, 1,0x62 }, // 404: hamP12; rickP15; hamkick; kick3.in
    { 327, 41, 1,0x62 }, // 405: hamP13; rimshot2
    { 328, 38, 1,0x62 }, // 406: hamP14; rickP16; hamsnr1; snr1.ins
    { 329, 39, 1,0x62 }, // 407: hamP15; handclap
    { 330, 49, 1,0x62 }, // 408: hamP16; smallsnr
    { 331, 83, 1,0x62 }, // 409: hamP17; rickP95; clsdhhat
    { 332, 59, 1,0x62 }, // 410: hamP18; openhht2
    { 261, 37, 1,0x62 }, // 411: hamP29; APS043i
    { 262, 39, 1,0x62 }, // 412: hamP30; mgun3in
    { 127, 24, 1,0x3D }, // 413: hamP34; apo035i
    { 127, 29, 1,0x3D }, // 414: hamP35; apo035i
    { 333, 24, 0,0x00 }, // 415: hamP41; tom2ins
    { 333, 27, 0,0x00 }, // 416: hamP42; tom2ins
    { 333, 29, 0,0x00 }, // 417: hamP43; tom2ins
    { 333, 32, 0,0x00 }, // 418: hamP44; tom2ins
    { 269, 32, 1,0x04 }, // 419: hamP45; tomins
    { 273, 90, 1,0x1A }, // 420: hamP51; snrsust
    { 334, 84, 1,0x07 }, // 421: hamP52; snarein
    { 276, 48, 1,0x39 }, // 422: hamP54; synsnr1
    { 132, 72, 1,0x62 }, // 423: hamP55; aps042i
    { 278, 72, 1,0x62 }, // 424: hamP57; rimshot
    { 281, 79, 1,0x07 }, // 425: hamP60; cymbali
    { 335, 61, 1,0x62 }, // 426: hamP67; tambour
    { 335, 68, 1,0x62 }, // 427: hamP68; tambour
    { 336, 36, 1,0x62 }, // 428: hamP73; clavesi
    { 336, 60, 1,0x62 }, // 429: hamP74; clavesi
    { 295, 36, 1,0x00 }, // 430: hamP81; trainbel
    { 298, 36, 1,0x06 }, // 431: hamP84; xylo1in
    { 298, 41, 1,0x06 }, // 432: hamP85; xylo1in
    { 337, 36, 1,0x62 }, // 433: hamP88; scratch
    { 115, 37, 1,0x62 }, // 434: hamP90; taikoin
    { 304, 36, 1,0x07 }, // 435: hamP91; rlogins
    { 338,  0, 1,0x62 }, // 436: rickM76; Bass.ins
    { 339,  0, 1,0x62 }, // 437: rickM77; Basnor04
    { 340,  0, 0,0x00 }, // 438: rickM78; Synbass1
    { 341,  0, 1,0x62 }, // 439: rickM79; Synbass2
    { 342,  0, 1,0x62 }, // 440: rickM80; Pickbass
    {  34,  0, 1,0x3D }, // 441: rickM81; Slapbass
    { 343,  0, 1,0x62 }, // 442: rickM82; Harpsi1.
    { 344,  0, 1,0x16 }, // 443: rickM83; Guit_el3
    { 226,  0, 1,0x62 }, // 444: rickM84; Guit_fz2
    { 345,  0, 1,0x16 }, // 445: rickM88; Orchit2.
    { 346,  0, 1,0x62 }, // 446: rickM89; Brass11.
    { 347,  0, 1,0x0C }, // 447: rickM90; Brass2.i
    { 348,  0, 1,0x62 }, // 448: rickM91; Brass3.i
    { 349,  0, 0,0x00 }, // 449: rickM92; Squ_wave
    { 350,  0, 0,0x00 }, // 450: rickM99; Agogo.in
    { 114,  0, 1,0x62 }, // 451: rickM100; Woodblk.
    { 351, 35, 1,0x62 }, // 452: rickP13; kick1.in
    { 352, 38, 1,0x3D }, // 453: rickP17; snare1.i
    { 353, 38, 1,0x3D }, // 454: rickP18; snare2.i
    { 352, 38, 1,0x0C }, // 455: rickP19; snare4.i
    { 353, 38, 1,0x62 }, // 456: rickP20; snare5.i
    { 354, 31, 1,0x0C }, // 457: rickP21; rocktom.
    { 354, 35, 1,0x0C }, // 458: rickP22; rocktom.
    { 354, 38, 1,0x0C }, // 459: rickP23; rocktom.
    { 354, 41, 1,0x0C }, // 460: rickP24; rocktom.
    { 354, 45, 1,0x0C }, // 461: rickP25; rocktom.
    { 354, 50, 1,0x0C }, // 462: rickP26; rocktom.
    { 355, 50, 0,0x00 }, // 463: rickP93; openhht1
    { 332, 50, 1,0x62 }, // 464: rickP94; openhht2
    { 356, 72, 1,0x62 }, // 465: rickP97; guirol.i
    { 148, 59, 1,0x62 }, // 466: rickP99; timbale.
    { 357, 64, 1,0x62 }, // 467: rickP101; congas2.
    { 357, 60, 1,0x62 }, // 468: rickP102; congas2.
    { 358, 72, 1,0x62 }, // 469: rickP103; bongos.i
    { 358, 62, 1,0x62 }, // 470: rickP104; bongos.i
    { 131, 53, 1,0x37 }, // 471: rickP105; surdu.in
};

const unsigned short banks[][256] =
{
    { // bank 0, MS GM
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,128,128,129,130,131,132,133,134,135,136,137,138,139,
140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,
156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,
172,173,174,175,176,177,178,179,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
    },
    { // bank 1, HMI
180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,196, 28,180, 30, 31,
 32, 39, 34, 35, 36,197, 38, 33, 40, 41, 42, 43, 44, 45, 46, 47,
 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,198, 63,
 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,200,201,202,203,204,
205,206,207,208,209,210,211,212,213,214,215,216,215,217,218,219,
220,221,222,223,221,223,224,221,225,221,226,227,228,229,230,231,
232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,
248,249,237,250,251,252,210,253,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
    },
    { // bank 2, HMI int
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,281,282,283,284,285,
286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,
302,303,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,304,305,306,307,308,
309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,
325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,
341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,
357,358,359,360,361,362,363,364,365,366,367,368,369,370,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
    },
    { // bank 3, HMI ham
254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,
270,271,272,273,254,254,254,254,254,254,254,281,282,283,284,285,
286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,
302,254,371,100,372,373,374,375,376,377,378,379,380,381,382,383,
384,385,386,387,388,389,390,391,392,393,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
400,133,135,137,139,140,142,174,157,401,402,403,404,405,406,407,
408,409,410,144,199,199,199,199,199,199,199,304,305,411,412,308,
309,310,413,414,313,314,315,316,317,415,416,417,418,419,323,324,
325,326,327,420,421,330,422,423,333,424,335,336,425,338,339,340,
341,342,343,426,427,346,347,348,349,428,429,352,353,354,355,356,
357,430,359,360,431,432,363,364,433,366,434,435,369,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
    },
    { // bank 4, HMI rick
254,254,254,254,254,254,254,254,254,254,254,254,254,267,268,269,
270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,
286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,
302,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,436,437,438,439,
440,441,442,443,444, 52, 53, 85,445,446,447,448,449, 86,372, 91,
374,386,102,450,451,120,254,254,254,254,254,254,254,254,254,254,
254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,254,
199,199,199,199,199,199,199,199,199,199,199,199,199,452,403,404,
406,453,454,455,456,457,458,459,460,461,462,304,305,306,307,308,
309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,
325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,
341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,
357,358,359,360,361,362,363,364,365,366,367,368,369,463,464,409,
165,465,157,466,402,467,468,469,470,471,199,199,199,199,199,199,
199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,199,
    },
};
