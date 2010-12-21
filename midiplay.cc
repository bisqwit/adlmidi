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
    signed char finetune;
    char symbol;            // Drawing symbol
} adl[];
extern const struct adlinsdata
{
    unsigned short adlno1, adlno2;
    unsigned char tone;
    unsigned char bit27, byte17;
    long ms_sound_kon;  // Number of milliseconds it produces sound;
    long ms_sound_koff;
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
    unsigned short ins[18], pit[18], insmeta[18], midiins[18];

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
            { insmeta[a] = 199; midiins[a] = 0; ins[a] = 189; pit[a] = 0; }
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
        { std::fputc('\r', stderr); // Ensure cursor is at x=0
          std::memset(slots, '.',      sizeof(slots));
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
        unsigned short portamento;
        unsigned char bank_lsb, bank_msb;
        unsigned char patch;
        unsigned char volume, expression;
        unsigned char panning, vibrato, sustain;
        double bend, bendsense;
        double vibpos, vibspeed, vibdepth;
        long   vibdelay;
        unsigned char lastlrpn,lastmrpn; bool nrpn;
        struct NoteInfo
        {
            signed char adlchn1, adlchn2; // adlib channel
            unsigned char  vol;           // pressure
            unsigned short ins1, ins2;    // instrument selected on noteon
            unsigned short tone;          // tone selected for note
        };
        typedef std::map<unsigned char,NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        activenotemap_t activenotes;

        MIDIchannel()
            : portamento(0),
              bank_lsb(0), bank_msb(0), patch(0),
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
        while(CurrentPosition.wait <= granularity/2)
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

    void NoteUpdate_Sub(
        int c,
        int tone,
        int ins,
        int vol,
        unsigned MidCh,
        unsigned props_mask)
    {
        if(c < 0) return;

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
            opl.Touch(c, vol * Ch[MidCh].volume * Ch[MidCh].expression);
        }
        if(props_mask & Upd_Pitch)
        {
            double bend = Ch[MidCh].bend + adl[ opl.ins[c] ].finetune;
            if(Ch[MidCh].vibrato && ch[c].age >= Ch[MidCh].vibdelay)
                bend += Ch[MidCh].vibrato * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);
            opl.NoteOn(c, 172.00093 * std::exp(0.057762265 * (tone + bend)));
            ch[c].state = AdlChannel::on;
            UI.IllustrateNote(c, tone, ins, vol, Ch[MidCh].bend);
        }
    }

    void NoteUpdate
        (unsigned MidCh,
         MIDIchannel::activenoteiterator& i,
         unsigned props_mask)
     {
        NoteUpdate_Sub(
            i->second.adlchn1,
            i->second.tone,
            i->second.ins1,
            i->second.vol,
            MidCh,
            props_mask);

        NoteUpdate_Sub(
            i->second.adlchn2,
            i->second.tone,
            i->second.ins2,
            i->second.vol,
            MidCh,
            props_mask);

        if(props_mask & Upd_Off)
        {
            Ch[MidCh].activenotes.erase(i);
            i = Ch[MidCh].activenotes.end();
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
        //if(shortest > 0) UI.PrintLn("shortest: %ld", shortest);

        // Schedule the next playevent to be processed after that delay
        for(size_t tk=0; tk<TrackCount; ++tk)
            CurrentPosition.track[tk].delay -= shortest;

        double t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t;
        for(unsigned a=0; a<18; ++a)
            if(ch[a].age < 0x70000000)
                ch[a].age += t*1000;
        /*for(unsigned a=0; a<18; ++a)
        {
            UI.GotoXY(64,a+1); UI.Color(2);
            std::fprintf(stderr, "%7ld,%c,%6ld\r",
                ch[a].age,
                "01s"[ch[a].state],
                ch[a].state == AdlChannel::off
                ? adlins[opl.insmeta[a]].ms_sound_koff
                : adlins[opl.insmeta[a]].ms_sound_kon);
            UI.x = 0;
        }*/

        //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest,t);

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
            //UI.PrintLn("@%X Track %u: %02X", CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte);
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
        /*UI.PrintLn("@%X Track %u: %02X %02X",
            CurrentPosition.track[tk].ptr-1, (unsigned)tk, byte,
            TrackData[tk][CurrentPosition.track[tk].ptr]);*/
        unsigned MidCh = byte & 0x0F, EvType = byte >> 4;
        CurrentPosition.track[tk].status = byte;
        switch(EvType)
        {
            case 0x8: // Note off
            case 0x9: // Note on
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                NoteOff(MidCh, note);
                // On Note on, Keyoff the note first, just in case keyoff
                // was omitted; this fixes Dance of sugar-plum fairy
                // by Microsoft. Now that we've done a Keyoff,
                // check if we still need to do a Keyon.
                // vol=0 and event 8x are both Keyoff-only.
                if(vol == 0 || EvType == 0x8) break;

                unsigned midiins = Ch[MidCh].patch;
                if(MidCh == 9) midiins = 128 + note; // Percussion instrument

                static std::set<unsigned> bank_warnings;
                if(Ch[MidCh].bank_msb)
                {
                    unsigned bankid = midiins + 256*Ch[MidCh].bank_msb;
                    std::set<unsigned>::iterator
                        i = bank_warnings.lower_bound(bankid);
                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        UI.PrintLn("[%u]Bank %u undefined, patch=%c%u",
                            MidCh,
                            Ch[MidCh].bank_msb,
                            (midiins&128)?'P':'M', midiins&127);
                        bank_warnings.insert(i, bankid);
                    }
                }
                if(Ch[MidCh].bank_lsb)
                {
                    unsigned bankid = Ch[MidCh].bank_lsb*65536;
                    std::set<unsigned>::iterator
                        i = bank_warnings.lower_bound(bankid);
                    if(i == bank_warnings.end() || *i != bankid)
                    {
                        UI.PrintLn("[%u]Bank lsb %u undefined",
                            MidCh,
                            Ch[MidCh].bank_lsb);
                        bank_warnings.insert(i, bankid);
                    }
                }

                int meta = banks[AdlBank][midiins];
                int tone = adlins[meta].tone ? adlins[meta].tone : note;
                int i1 = adlins[meta].adlno1;
                int i2 = adlins[meta].adlno2;

                // Allocate AdLib channel (the physical sound channel for the note)
                int adlchannel[2] = { -1, -1 };
                for(unsigned ccount = 0; ccount < 2; ++ccount)
                {
                    int c = -1;
                    long bs = -9;
                    for(int a = 0; a < 18; ++a)
                    {
                        long s = ch[a].age;   // Age in seconds = better score
                        switch(ch[a].state)
                        {
                            case AdlChannel::on:
                            {
                                s -= adlins[opl.insmeta[a]].ms_sound_kon;
                                break;
                            }
                            case AdlChannel::sustained:
                            {
                                s -= adlins[opl.insmeta[a]].ms_sound_kon / 2;
                                break;
                            }
                            case AdlChannel::off:
                            {
                                s -= adlins[opl.insmeta[a]].ms_sound_koff / 2;
                                break;
                            }
                        }
                        if(i1 == opl.ins[a]
                        || i2 == opl.ins[a]) s += 50;  // Same instrument = good
                        if(a == MidCh) s += 1;
                        s += 50 * (opl.midiins[a] / 128); // Percussion is inferior to melody
                        if(s > bs) { bs=s; c = a; } // Best candidate wins
                    }
                    if(c < 0)
                    {
                        //UI.PrintLn("ignored unplaceable note");
                        continue; // Could not play this note. Ignore it.
                    }
                    if(ch[c].state == AdlChannel::on)
                    {
                        if(ccount == 1) break; // Don't kill a note for a secondary sound.
                        /*UI.PrintLn(
                            "collision @%u: G%c%u[%ld] <- G%c%u",
                            c,
                            opl.midiins[c]<128?'M':'P', opl.midiins[c]&127,
                            ch[c].age,
                            midiins<128?'M':'P', midiins&127
                            );*/
                        NoteOff(ch[c].midichn, ch[c].note); // Collision: Kill old note
                    }
                    if(ch[c].state == AdlChannel::sustained)
                    {
                        NoteOffSustain(c);
                        // A sustained note needs to be keyoff'd
                        // first so that it can be retriggered.
                    }
                    adlchannel[ccount] = c;

                    if(i1 == i2) break;
                }
                if(adlchannel[0] < 0 && adlchannel[1] < 0) break;

                // Allocate active note for MIDI channel
                std::pair<MIDIchannel::activenoteiterator,bool>
                    ir = Ch[MidCh].activenotes.insert(
                        std::make_pair(note, MIDIchannel::NoteInfo()));
                ir.first->second.adlchn1 = adlchannel[0];
                ir.first->second.adlchn2 = adlchannel[1];
                ir.first->second.vol     = vol;
                ir.first->second.ins1    = i1;
                ir.first->second.ins2    = i2;
                ir.first->second.tone    = tone;
                for(unsigned ccount=0; ccount<2; ++ccount)
                {
                    int c = adlchannel[ccount];
                    if(c < 0) continue;
                    ch[c].midichn = MidCh;
                    ch[c].note    = note;
                    opl.insmeta[c] = meta;
                    opl.midiins[c] = midiins;
                }
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
                    case 0: // Set bank msb (GM bank)
                        Ch[MidCh].bank_msb = value;
                        break;
                    case 32: // Set bank lsb (XG bank)
                        Ch[MidCh].bank_lsb = value;
                        break;
                    case 5: // Set portamento msb
                        Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x7F) | (value<<7);
                        UpdatePortamento(MidCh);
                        break;
                    case 37: // Set portamento lsb
                        Ch[MidCh].portamento = (Ch[MidCh].portamento & 0x3F80) | (value);
                        UpdatePortamento(MidCh);
                        break;
                    case 65: // Enable/disable portamento
                        // value >= 64 ? enabled : disabled
                        //UpdatePortamento(MidCh);
                        break;
                    case 7: // Change volume
                        Ch[MidCh].volume = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 64: // Enable/disable sustain
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
                    case 91: break; // Reverb effect depth. We don't do per-channel reverb.
                    case 92: break; // Tremolo effect depth. We don't do...
                    case 93: break; // Chorus effect depth. We don't do.
                    case 94: break; // Celeste effect depth. We don't do.
                    case 95: break; // Phaser effect depth. We don't do.
                    case 98: Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=true; break;
                    case 99: Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=true; break;
                    case 100:Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=false; break;
                    case 101:Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=false; break;
                    case 113: break; // Related to pitch-bender, used by missimp.mid in Duke3D
                    case  6: SetRPN(MidCh, value, true); break;
                    case 38: SetRPN(MidCh, value, false); break;
                    default:
                        UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
                }
                break;
            }
            case 0xC: // Patch change
                Ch[MidCh].patch = TrackData[tk][CurrentPosition.track[tk].ptr++];
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
        {
            NoteUpdate(MidCh, i, Upd_Off);
        }
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
                4.0,  // wet_gain_dB  (-10..10)
                .8,//.7,   // room_scale   (0..1)
                0.,//.6,   // reverberance (0..1)
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

    const double mindelay = 1 / (double)OPL3::PCM_RATE;
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

/* THIS ADLIB FM INSTRUMENT DATA IS AUTOMATICALLY GENERATED FROM
 * THE FOLLOWING SOURCES:
 *    DESCENT DATA FILES (HUMAN MACHINE INTERFACES & PARALLAX SOFTWARE)
 *    <UNIDENTIFIED> SOUNDCARD DRIVER FOR WINDOWS 98
 * PREPROCESSED, CONVERTED, AND POSTPROCESSED OFF-SCREEN.
 */
const struct adldata adl[] =
{ //    ,---------+-------- Wave select settings
  //    | ,-------÷-+------ Sustain/release rates
  //    | | ,-----÷-÷-+---- Attack/decay rates
  //    | | | ,---÷-÷-÷-+-- AM/VIB/EG/KSR/Multiple bits
  //    | | | |   | | | |
  //    | | | |   | | | |     ,----+-- KSL/attenuation settings
  //    | | | |   | | | |     |    |    ,----- Feedback/connection bits
  //    | | | |   | | | |     |    |    |
    { 0x0F4F201,0x0F7F201, 0x8F,0x06, 0x8,+0,'P' }, // 0: GM0; AcouGrandPiano
    { 0x0F4F201,0x0F7F201, 0x4B,0x00, 0x8,+0,'P' }, // 1: GM1; BrightAcouGrand
    { 0x0F4F201,0x0F6F201, 0x49,0x00, 0x8,+0,'P' }, // 2: GM2; ElecGrandPiano
    { 0x0F7F281,0x0F7F241, 0x12,0x00, 0x6,+0,'P' }, // 3: GM3; Honky-tonkPiano
    { 0x0F7F101,0x0F7F201, 0x57,0x00, 0x0,+0,'P' }, // 4: GM4; Rhodes Piano
    { 0x0F7F101,0x0F7F201, 0x93,0x00, 0x0,+0,'P' }, // 5: GM5; Chorused Piano
    { 0x0F2A101,0x0F5F216, 0x80,0x0E, 0x8,+0,'h' }, // 6: GM6; Harpsichord
    { 0x0F8C201,0x0F8C201, 0x92,0x00, 0xA,+0,'c' }, // 7: GM7; Clavinet
    { 0x0F4F60C,0x0F5F381, 0x5C,0x00, 0x0,+0,'c' }, // 8: GM8; Celesta
    { 0x0F2F307,0x0F1F211, 0x97,0x80, 0x2,+0,'k' }, // 9: GM9; Glockenspiel
    { 0x0F45417,0x0F4F401, 0x21,0x00, 0x2,+0,'m' }, // 10: GM10; Music box
    { 0x0F6F398,0x0F6F281, 0x62,0x00, 0x0,+0,'v' }, // 11: GM11; Vibraphone
    { 0x0F6F618,0x0F7E701, 0x23,0x00, 0x0,+0,'m' }, // 12: GM12; Marimba
    { 0x0F6F615,0x0F6F601, 0x91,0x00, 0x4,+0,'x' }, // 13: GM13; Xylophone
    { 0x0F3D345,0x0F3A381, 0x59,0x80, 0xC,+0,'b' }, // 14: GM14; Tubular Bells
    { 0x1F57503,0x0F5B581, 0x49,0x80, 0x4,+0,'d' }, // 15: GM15; Dulcimer
    { 0x014F671,0x007F131, 0x92,0x00, 0x2,+0,'o' }, // 16: GM16; HMIGM16; Hammond Organ; am016.in
    { 0x058C772,0x008C730, 0x14,0x00, 0x2,+0,'o' }, // 17: GM17; HMIGM17; Percussive Organ; am017.in
    { 0x018AA70,0x0088AB1, 0x44,0x00, 0x4,+0,'o' }, // 18: GM18; HMIGM18; Rock Organ; am018.in
    { 0x1239723,0x01455B1, 0x93,0x00, 0x4,+0,'o' }, // 19: GM19; HMIGM19; Church Organ; am019.in
    { 0x1049761,0x00455B1, 0x13,0x80, 0x0,+0,'o' }, // 20: GM20; HMIGM20; Reed Organ; am020.in
    { 0x12A9824,0x01A46B1, 0x48,0x00, 0xC,+0,'a' }, // 21: GM21; HMIGM21; Accordion; am021.in
    { 0x1069161,0x0076121, 0x13,0x00, 0xA,+0,'h' }, // 22: GM22; HMIGM22; Harmonica; am022.in
    { 0x0067121,0x00761A1, 0x13,0x89, 0x6,+0,'o' }, // 23: GM23; HMIGM23; Tango Accordion; am023.in
    { 0x194F302,0x0C8F341, 0x9C,0x80, 0xC,+0,'G' }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in
    { 0x19AF303,0x0E7F111, 0x54,0x00, 0xC,+0,'G' }, // 25: GM25; HMIGM25; Acoustic Guitar2; am025.in
    { 0x03AF123,0x0F8F221, 0x5F,0x00, 0x0,+0,'G' }, // 26: GM26; HMIGM26; Electric Guitar1; am026.in
    { 0x122F603,0x0F8F321, 0x87,0x80, 0x6,+0,'G' }, // 27: GM27; Electric Guitar2
    { 0x054F903,0x03AF621, 0x47,0x00, 0x0,+0,'G' }, // 28: GM28; HMIGM28; hamM3; hamM60; intM3; rickM3; BPerc.in; BPercin; Electric Guitar3; am028.in; muteguit
    { 0x1419123,0x0198421, 0x4A,0x05, 0x8,+0,'G' }, // 29: GM29; Overdrive Guitar
    { 0x1199523,0x0199421, 0x4A,0x00, 0x8,+0,'G' }, // 30: GM30; HMIGM30; hamM6; intM6; rickM6; Distorton Guitar; GDist.in; GDistin; am030.in
    { 0x04F2009,0x0F8D184, 0xA1,0x80, 0x8,+0,'G' }, // 31: GM31; HMIGM31; hamM5; intM5; rickM5; GFeedbck; Guitar Harmonics; am031.in
    { 0x0069421,0x0A6C3A2, 0x1E,0x00, 0x2,+0,'B' }, // 32: GM32; HMIGM32; Acoustic Bass; am032.in
    { 0x028F131,0x018F131, 0x12,0x00, 0xA,+0,'B' }, // 33: GM33; GM39; HMIGM33; HMIGM39; hamM68; Electric Bass 1; Synth Bass 2; am033.in; am039.in; synbass2
    { 0x0E8F131,0x078F131, 0x8D,0x00, 0xA,+0,'B' }, // 34: GM34; HMIGM34; rickM81; Electric Bass 2; Slapbass; am034.in
    { 0x0285131,0x0487132, 0x5B,0x00, 0xC,+0,'B' }, // 35: GM35; HMIGM35; Fretless Bass; am035.in
    { 0x09AA101,0x0DFF221, 0x8B,0x40, 0x8,+0,'B' }, // 36: GM36; HMIGM36; Slap Bass 1; am036.in
    { 0x016A221,0x0DFA121, 0x8B,0x08, 0x8,+0,'B' }, // 37: GM37; Slap Bass 2
    { 0x0E8F431,0x078F131, 0x8B,0x00, 0xA,+0,'B' }, // 38: GM38; HMIGM38; hamM13; hamM67; intM13; rickM13; BSynth3; BSynth3.; Synth Bass 1; am038.in; synbass1
    { 0x113DD31,0x0265621, 0x15,0x00, 0x8,+0,'V' }, // 39: GM40; HMIGM40; Violin; am040.in
    { 0x113DD31,0x0066621, 0x16,0x00, 0x8,+0,'V' }, // 40: GM41; HMIGM41; Viola; am041.in
    { 0x11CD171,0x00C6131, 0x49,0x00, 0x8,+0,'V' }, // 41: GM42; HMIGM42; Cello; am042.in
    { 0x1127121,0x0067223, 0x4D,0x80, 0x2,+0,'V' }, // 42: GM43; HMIGM43; Contrabass; am043.in
    { 0x121F1F1,0x0166FE1, 0x40,0x00, 0x2,+0,'V' }, // 43: GM44; HMIGM44; Tremulo Strings; am044.in
    { 0x175F502,0x0358501, 0x1A,0x80, 0x0,+0,'H' }, // 44: GM45; HMIGM45; Pizzicato String; am045.in
    { 0x175F502,0x0F4F301, 0x1D,0x80, 0x0,+0,'H' }, // 45: GM46; HMIGM46; Orchestral Harp; am046.in
    { 0x105F510,0x0C3F211, 0x41,0x00, 0x2,+0,'M' }, // 46: GM47; HMIGM47; hamM14; intM14; rickM14; BSynth4; BSynth4.; Timpany; am047.in
    { 0x125B121,0x00872A2, 0x9B,0x01, 0xE,+0,'S' }, // 47: GM48; HMIGM48; String Ensemble1; am048.in
    { 0x1037FA1,0x1073F21, 0x98,0x00, 0x0,+0,'S' }, // 48: GM49; HMIGM49; String Ensemble2; am049.in
    { 0x012C1A1,0x0054F61, 0x93,0x00, 0xA,+0,'S' }, // 49: GM50; HMIGM50; hamM20; intM20; rickM20; PMellow; PMellow.; Synth Strings 1; am050.in
    { 0x022C121,0x0054F61, 0x18,0x00, 0xC,+0,'S' }, // 50: GM51; HMIGM51; SynthStrings 2; am051.in
    { 0x015F431,0x0058A72, 0x5B,0x83, 0x0,+0,'O' }, // 51: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in
    { 0x03974A1,0x0677161, 0x90,0x00, 0x0,+0,'O' }, // 52: GM53; HMIGM53; rickM86; Oohs.ins; Voice Oohs; am053.in
    { 0x0055471,0x0057A72, 0x57,0x00, 0xC,+0,'O' }, // 53: GM54; HMIGM54; Synth Voice; am054.in
    { 0x0635490,0x045A541, 0x00,0x00, 0x8,+0,'c' }, // 54: GM55; HMIGM55; Orchestra Hit; am055.in
    { 0x0178521,0x0098F21, 0x92,0x01, 0xC,+0,'T' }, // 55: GM56; HMIGM56; Trumpet; am056.in
    { 0x0177521,0x0098F21, 0x94,0x05, 0xC,+0,'T' }, // 56: GM57; HMIGM57; Trombone; am057.in
    { 0x0157621,0x0378261, 0x94,0x00, 0xC,+0,'T' }, // 57: GM58; HMIGM58; Tuba; am058.in
    { 0x1179E31,0x12C6221, 0x43,0x00, 0x2,+0,'T' }, // 58: GM59; HMIGM59; Muted Trumpet; am059.in
    { 0x06A6121,0x00A7F21, 0x9B,0x00, 0x2,+0,'T' }, // 59: GM60; HMIGM60; French Horn; am060.in
    { 0x01F7561,0x00F7422, 0x8A,0x06, 0x8,+0,'T' }, // 60: GM61; HMIGM61; Brass Section; am061.in
    { 0x15572A1,0x0187121, 0x86,0x83, 0x0,+0,'T' }, // 61: GM62; Synth Brass 1
    { 0x03C5421,0x01CA621, 0x4D,0x00, 0x8,+0,'T' }, // 62: GM63; HMIGM63; Synth Brass 2; am063.in
    { 0x1029331,0x00B7261, 0x8F,0x00, 0x8,+0,'X' }, // 63: GM64; HMIGM64; Soprano Sax; am064.in
    { 0x1039331,0x0097261, 0x8E,0x00, 0x8,+0,'X' }, // 64: GM65; HMIGM65; Alto Sax; am065.in
    { 0x1039331,0x0098261, 0x91,0x00, 0xA,+0,'X' }, // 65: GM66; HMIGM66; Tenor Sax; am066.in
    { 0x10F9331,0x00F7261, 0x8E,0x00, 0xA,+0,'X' }, // 66: GM67; HMIGM67; Baritone Sax; am067.in
    { 0x116AA21,0x00A8F21, 0x4B,0x00, 0x8,+0,'T' }, // 67: GM68; HMIGM68; Oboe; am068.in
    { 0x1177E31,0x10C8B21, 0x90,0x00, 0x6,+0,'T' }, // 68: GM69; HMIGM69; English Horn; am069.in
    { 0x1197531,0x0196132, 0x81,0x00, 0x0,+0,'T' }, // 69: GM70; HMIGM70; Bassoon; am070.in
    { 0x0219B32,0x0177221, 0x90,0x00, 0x4,+0,'F' }, // 70: GM71; HMIGM71; Clarinet; am071.in
    { 0x05F85E1,0x01A65E1, 0x1F,0x00, 0x0,+0,'F' }, // 71: GM72; HMIGM72; Piccolo; am072.in
    { 0x05F88E1,0x01A65E1, 0x46,0x00, 0x0,+0,'F' }, // 72: GM73; HMIGM73; Flute; am073.in
    { 0x01F75A1,0x00A7521, 0x9C,0x00, 0x2,+0,'F' }, // 73: GM74; HMIGM74; Recorder; am074.in
    { 0x0588431,0x01A6521, 0x8B,0x00, 0x0,+0,'F' }, // 74: GM75; HMIGM75; Pan Flute; am075.in
    { 0x05666E1,0x02665A1, 0x4C,0x00, 0x0,+0,'F' }, // 75: GM76; HMIGM76; Bottle Blow; am076.in
    { 0x0467662,0x03655A1, 0xCB,0x00, 0x0,+0,'F' }, // 76: GM77; HMIGM77; Shakuhachi; am077.in
    { 0x0075762,0x00756A1, 0x99,0x00, 0xB,+0,'F' }, // 77: GM78; HMIGM78; Whistle; am078.in
    { 0x0077762,0x00776A1, 0x93,0x00, 0xB,+0,'F' }, // 78: GM79; HMIGM79; hamM61; Ocarina; am079.in; ocarina
    { 0x203FF22,0x00FFF21, 0x59,0x00, 0x0,+0,'L' }, // 79: GM80; HMIGM80; hamM16; hamM65; intM16; rickM16; LSquare; LSquare.; Lead 1 squareea; am080.in; squarewv
    { 0x10FFF21,0x10FFF21, 0x0E,0x00, 0x0,+0,'L' }, // 80: GM81; HMIGM81; Lead 2 sawtooth; am081.in
    { 0x0558622,0x0186421, 0x46,0x80, 0x0,+0,'L' }, // 81: GM82; HMIGM82; Lead 3 calliope; am082.in
    { 0x0126621,0x00A96A1, 0x45,0x00, 0x0,+0,'L' }, // 82: GM83; HMIGM83; Lead 4 chiff; am083.in
    { 0x12A9221,0x02A9122, 0x8B,0x00, 0x0,+0,'L' }, // 83: GM84; HMIGM84; Lead 5 charang; am084.in
    { 0x005DFA2,0x0076F61, 0x9E,0x40, 0x2,+0,'L' }, // 84: GM85; HMIGM85; hamM17; intM17; rickM17; rickM87; Lead 6 voice; PFlutes; PFlutes.; Solovox.; am085.in
    { 0x001EF20,0x2068F60, 0x1A,0x00, 0x0,+0,'L' }, // 85: GM86; HMIGM86; rickM93; Lead 7 fifths; Saw_wave; am086.in
    { 0x029F121,0x009F421, 0x8F,0x80, 0xA,+0,'L' }, // 86: GM87; HMIGM87; Lead 8 brass; am087.in
    { 0x0945377,0x005A0A1, 0xA5,0x00, 0x2,+0,'p' }, // 87: GM88; HMIGM88; Pad 1 new age; am088.in
    { 0x011A861,0x00325B1, 0x1F,0x80, 0xA,+0,'p' }, // 88: GM89; HMIGM89; Pad 2 warm; am089.in
    { 0x0349161,0x0165561, 0x17,0x00, 0xC,+0,'p' }, // 89: GM90; HMIGM90; hamM21; intM21; rickM21; LTriang; LTriang.; Pad 3 polysynth; am090.in
    { 0x0015471,0x0036A72, 0x5D,0x00, 0x0,+0,'p' }, // 90: GM91; HMIGM91; rickM95; Pad 4 choir; Spacevo.; am091.in
    { 0x0432121,0x03542A2, 0x97,0x00, 0x8,+0,'p' }, // 91: GM92; HMIGM92; Pad 5 bowedpad; am092.in
    { 0x177A1A1,0x1473121, 0x1C,0x00, 0x0,+0,'p' }, // 92: GM93; HMIGM93; hamM22; intM22; rickM22; PSlow.in; PSlowin; Pad 6 metallic; am093.in
    { 0x0331121,0x0254261, 0x89,0x03, 0xA,+0,'p' }, // 93: GM94; HMIGM94; hamM23; hamM54; intM23; rickM23; rickM96; Halopad.; PSweep.i; PSweepi; Pad 7 halo; am094.in; halopad
    { 0x14711A1,0x007CF21, 0x15,0x00, 0x0,+0,'p' }, // 94: GM95; HMIGM95; hamM66; rickM97; Pad 8 sweep; Sweepad.; am095.in; sweepad
    { 0x0F6F83A,0x0028651, 0xCE,0x00, 0x2,+0,'X' }, // 95: GM96; HMIGM96; FX 1 rain; am096.in
    { 0x1232121,0x0134121, 0x15,0x00, 0x0,+0,'X' }, // 96: GM97; HMIGM97; FX 2 soundtrack; am097.in
    { 0x0957406,0x072A501, 0x5B,0x00, 0x0,+0,'X' }, // 97: GM98; HMIGM98; FX 3 crystal; am098.in
    { 0x081B122,0x026F261, 0x92,0x83, 0xC,+0,'X' }, // 98: GM99; HMIGM99; FX 4 atmosphere; am099.in
    { 0x151F141,0x0F5F242, 0x4D,0x00, 0x0,+0,'X' }, // 99: GM100; HMIGM100; hamM51; FX 5 brightness; am100.in; am100in
    { 0x1511161,0x01311A3, 0x94,0x80, 0x6,+0,'X' }, // 100: GM101; HMIGM101; FX 6 goblins; am101.in
    { 0x0311161,0x0031DA1, 0x8C,0x80, 0x6,+0,'X' }, // 101: GM102; HMIGM102; rickM98; Echodrp1; FX 7 echoes; am102.in
    { 0x173F3A4,0x0238161, 0x4C,0x00, 0x4,+0,'X' }, // 102: GM103; HMIGM103; FX 8 sci-fi; am103.in
    { 0x053D202,0x1F6F207, 0x85,0x03, 0x0,+0,'G' }, // 103: GM104; HMIGM104; Sitar; am104.in
    { 0x111A311,0x0E5A213, 0x0C,0x80, 0x0,+0,'G' }, // 104: GM105; HMIGM105; Banjo; am105.in
    { 0x141F611,0x2E6F211, 0x06,0x00, 0x4,+0,'G' }, // 105: GM106; HMIGM106; hamM24; intM24; rickM24; LDist.in; LDistin; Shamisen; am106.in
    { 0x032D493,0x111EB91, 0x91,0x00, 0x8,+0,'G' }, // 106: GM107; HMIGM107; Koto; am107.in
    { 0x056FA04,0x005C201, 0x4F,0x00, 0xC,+0,'G' }, // 107: GM108; HMIGM108; hamM57; Kalimba; am108.in; kalimba
    { 0x0207C21,0x10C6F22, 0x49,0x00, 0x6,+0,'T' }, // 108: GM109; HMIGM109; Bagpipe; am109.in
    { 0x133DD31,0x0165621, 0x85,0x00, 0xA,+0,'S' }, // 109: GM110; HMIGM110; Fiddle; am110.in
    { 0x205DA20,0x00B8F21, 0x04,0x81, 0x6,+0,'S' }, // 110: GM111; HMIGM111; Shanai; am111.in
    { 0x0E5F105,0x0E5C303, 0x6A,0x80, 0x6,+0,'b' }, // 111: GM112; HMIGM112; Tinkle Bell; am112.in
    { 0x026EC07,0x016F802, 0x15,0x00, 0xA,+0,'b' }, // 112: GM113; HMIGM113; hamM50; Agogo Bells; agogoin; am113.in
    { 0x0356705,0x005DF01, 0x9D,0x00, 0x8,+0,'b' }, // 113: GM114; HMIGM114; Steel Drums; am114.in
    { 0x028FA18,0x0E5F812, 0x96,0x00, 0xA,+0,'b' }, // 114: GM115; HMIGM115; rickM100; Woodblk.; Woodblock; am115.in
    { 0x007A810,0x003FA00, 0x86,0x03, 0x6,+0,'M' }, // 115: GM116; HMIGM116; hamM69; hamP90; Taiko Drum; Taikoin; am116.in; taikoin
    { 0x247F811,0x003F310, 0x41,0x03, 0x4,+0,'M' }, // 116: GM117; HMIGM117; hamM58; Melodic Tom; am117.in; melotom
    { 0x206F101,0x002F310, 0x8E,0x00, 0xE,+0,'M' }, // 117: GM118; HMIGM118; Synth Drum; am118.in
    { 0x0001F0E,0x3FF1FC0, 0x00,0x00, 0xE,+0,'c' }, // 118: GM119; HMIGM119; Reverse Cymbal; am119.in
    { 0x024F806,0x2845603, 0x80,0x88, 0xE,+0,'G' }, // 119: GM120; HMIGM120; hamM36; intM36; rickM101; rickM36; DNoise1; DNoise1.; Fretnos.; Guitar FretNoise; am120.in
    { 0x000F80E,0x30434D0, 0x00,0x05, 0xE,+0,'X' }, // 120: GM121; HMIGM121; Breath Noise; am121.in
    { 0x000F60E,0x3021FC0, 0x00,0x00, 0xE,+0,'X' }, // 121: GM122; HMIGM122; Seashore; am122.in
    { 0x0A337D5,0x03756DA, 0x95,0x40, 0x0,+0,'X' }, // 122: GM123; HMIGM123; Bird Tweet; am123.in
    { 0x261B235,0x015F414, 0x5C,0x08, 0xA,+0,'X' }, // 123: GM124; HMIGM124; Telephone; am124.in
    { 0x000F60E,0x3F54FD0, 0x00,0x00, 0xE,+0,'X' }, // 124: GM125; HMIGM125; Helicopter; am125.in
    { 0x001FF26,0x11612E4, 0x00,0x00, 0xE,+0,'X' }, // 125: GM126; HMIGM126; Applause/Noise; am126.in
    { 0x0F0F300,0x2C9F600, 0x00,0x00, 0xE,+0,'X' }, // 126: GM127; HMIGM127; Gunshot; am127.in
    { 0x277F810,0x006F311, 0x44,0x00, 0x8,+0,'D' }, // 127: GP35; GP36; IntP34; IntP35; hamP11; hamP34; hamP35; rickP14; rickP34; rickP35; Ac Bass Drum; Bass Drum 1; apo035.i; apo035i; aps035i; kick2.in
    { 0x0FFF902,0x0FFF811, 0x07,0x00, 0x8,+0,'D' }, // 128: GP37; Side Stick
    { 0x205FC00,0x017FA00, 0x00,0x00, 0xE,+0,'s' }, // 129: GP38; GP40; Acoustic Snare; Electric Snare
    { 0x007FF00,0x008FF01, 0x02,0x00, 0x0,+0,'h' }, // 130: GP39; Hand Clap
    { 0x00CF600,0x006F600, 0x00,0x00, 0x4,+0,'M' }, // 131: GP41; GP43; GP45; GP47; GP48; GP50; GP87; hamP1; hamP2; hamP3; hamP4; hamP5; hamP6; rickP105; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; aps041i; surdu.in
    { 0x008F60C,0x247FB12, 0x00,0x00, 0xA,+0,'h' }, // 132: GP42; IntP55; hamP55; rickP55; Closed High Hat; aps042.i; aps042i
    { 0x008F60C,0x2477B12, 0x00,0x05, 0xA,+0,'h' }, // 133: GP44; Pedal High Hat
    { 0x002F60C,0x243CB12, 0x00,0x00, 0xA,+0,'h' }, // 134: GP46; Open High Hat
    { 0x000F60E,0x3029FD0, 0x00,0x00, 0xE,+0,'C' }, // 135: GP49; GP57; hamP0; Crash Cymbal 1; Crash Cymbal 2; crash1i
    { 0x042F80E,0x3E4F407, 0x08,0x4A, 0xE,+0,'C' }, // 136: GP51; GP59; Ride Cymbal 1; Ride Cymbal 2
    { 0x030F50E,0x0029FD0, 0x00,0x0A, 0xE,+0,'C' }, // 137: GP52; hamP19; Chinese Cymbal; aps052i
    { 0x3E4E40E,0x1E5F507, 0x0A,0x5D, 0x6,+0,'b' }, // 138: GP53; Ride Bell
    { 0x004B402,0x0F79705, 0x03,0x0A, 0xE,+0,'M' }, // 139: GP54; Tambourine
    { 0x000F64E,0x3029F9E, 0x00,0x00, 0xE,+0,'C' }, // 140: GP55; Splash Cymbal
    { 0x237F811,0x005F310, 0x45,0x08, 0x8,+0,'B' }, // 141: GP56; Cow Bell
    { 0x303FF80,0x014FF10, 0x00,0x0D, 0xC,+0,'D' }, // 142: GP58; Vibraslap
    { 0x00CF506,0x008F502, 0x0B,0x00, 0x6,+0,'M' }, // 143: GP60; High Bongo
    { 0x0BFFA01,0x097C802, 0x00,0x00, 0x7,+0,'M' }, // 144: GP61; Low Bongo
    { 0x087FA01,0x0B7FA01, 0x51,0x00, 0x6,+0,'D' }, // 145: GP62; Mute High Conga
    { 0x08DFA01,0x0B8F802, 0x54,0x00, 0x6,+0,'D' }, // 146: GP63; Open High Conga
    { 0x088FA01,0x0B6F802, 0x59,0x00, 0x6,+0,'D' }, // 147: GP64; Low Conga
    { 0x30AF901,0x006FA00, 0x00,0x00, 0xE,+0,'M' }, // 148: GP65; hamP8; rickP98; rickP99; High Timbale; timbale; timbale.
    { 0x389F900,0x06CF600, 0x80,0x00, 0xE,+0,'M' }, // 149: GP66; Low Timbale
    { 0x388F803,0x0B6F60C, 0x80,0x08, 0xF,+0,'D' }, // 150: GP67; High Agogo
    { 0x388F803,0x0B6F60C, 0x85,0x00, 0xF,+0,'D' }, // 151: GP68; Low Agogo
    { 0x04F760E,0x2187700, 0x40,0x08, 0xE,+0,'D' }, // 152: GP69; Cabasa
    { 0x049C80E,0x2699B03, 0x40,0x00, 0xE,+0,'D' }, // 153: GP70; Maracas
    { 0x305ADD7,0x0058DC7, 0xDC,0x00, 0xE,+0,'D' }, // 154: GP71; Short Whistle
    { 0x304A8D7,0x00488C7, 0xDC,0x00, 0xE,+0,'D' }, // 155: GP72; Long Whistle
    { 0x306F680,0x3176711, 0x00,0x00, 0xE,+0,'D' }, // 156: GP73; rickP96; Short Guiro; guiros.i
    { 0x205F580,0x3164611, 0x00,0x09, 0xE,+0,'D' }, // 157: GP74; Long Guiro
    { 0x0F40006,0x0F5F715, 0x3F,0x00, 0x1,+0,'D' }, // 158: GP75; Claves
    { 0x3F40006,0x0F5F712, 0x3F,0x00, 0x0,+0,'D' }, // 159: GP76; High Wood Block
    { 0x0F40006,0x0F5F712, 0x3F,0x00, 0x1,+0,'D' }, // 160: GP77; Low Wood Block
    { 0x0E76701,0x0077502, 0x58,0x00, 0x0,+0,'D' }, // 161: GP78; Mute Cuica
    { 0x048F841,0x0057542, 0x45,0x08, 0x0,+0,'D' }, // 162: GP79; Open Cuica
    { 0x3F0E00A,0x005FF1E, 0x40,0x4E, 0x8,+0,'D' }, // 163: GP80; Mute Triangle
    { 0x3F0E00A,0x002FF1E, 0x7C,0x52, 0x8,+0,'D' }, // 164: GP81; Open Triangle
    { 0x04A7A0E,0x21B7B00, 0x40,0x08, 0xE,+0,'D' }, // 165: GP82; hamP7; aps082i
    { 0x3E4E40E,0x1395507, 0x0A,0x40, 0x6,+0,'D' }, // 166: GP83
    { 0x332F905,0x0A5D604, 0x05,0x40, 0xE,+0,'D' }, // 167: GP84
    { 0x3F30002,0x0F5F715, 0x3F,0x00, 0x8,+0,'D' }, // 168: GP85
    { 0x08DFA01,0x0B5F802, 0x4F,0x00, 0x7,+0,'D' }, // 169: GP86
    { 0x1199523,0x0198421, 0x48,0x00, 0x8,+0,'P' }, // 170: HMIGM0; HMIGM29; am029.in
    { 0x054F231,0x056F221, 0x4B,0x00, 0x8,+0,'P' }, // 171: HMIGM1; am001.in
    { 0x055F231,0x076F221, 0x49,0x00, 0x8,+0,'P' }, // 172: HMIGM2; am002.in
    { 0x03BF2B1,0x00BF361, 0x0E,0x00, 0x6,+0,'P' }, // 173: HMIGM3; am003.in
    { 0x038F101,0x028F121, 0x57,0x00, 0x0,+0,'P' }, // 174: HMIGM4; am004.in
    { 0x038F101,0x028F121, 0x93,0x00, 0x0,+0,'P' }, // 175: HMIGM5; am005.in
    { 0x001A221,0x0D5F136, 0x80,0x0E, 0x8,+0,'h' }, // 176: HMIGM6; am006.in
    { 0x0A8C201,0x058C201, 0x92,0x00, 0xA,+0,'c' }, // 177: HMIGM7; am007.in
    { 0x054F60C,0x0B5F381, 0x5C,0x00, 0x0,+0,'c' }, // 178: HMIGM8; am008.in
    { 0x032F607,0x011F511, 0x97,0x80, 0x2,+0,'k' }, // 179: HMIGM9; am009.in
    { 0x0045617,0x004F601, 0x21,0x00, 0x2,+0,'m' }, // 180: HMIGM10; am010.in
    { 0x0E6F318,0x0F6F281, 0x62,0x00, 0x0,+0,'v' }, // 181: HMIGM11; am011.in
    { 0x055F718,0x0D8E521, 0x23,0x00, 0x0,+0,'m' }, // 182: HMIGM12; am012.in
    { 0x0A6F615,0x0E6F601, 0x91,0x00, 0x4,+0,'x' }, // 183: HMIGM13; am013.in
    { 0x082D345,0x0E3A381, 0x59,0x80, 0xC,+0,'b' }, // 184: HMIGM14; am014.in
    { 0x1557403,0x005B381, 0x49,0x80, 0x4,+0,'d' }, // 185: HMIGM15; am015.in
    { 0x122F603,0x0F3F321, 0x87,0x80, 0x6,+0,'G' }, // 186: HMIGM27; am027.in
    { 0x09AA101,0x0DFF221, 0x89,0x40, 0x8,+0,'B' }, // 187: HMIGM37; am037.in
    { 0x15572A1,0x0187121, 0x86,0x0D, 0x0,+0,'T' }, // 188: HMIGM62; am062.in
    { 0x0F00010,0x0F00010, 0x3F,0x3F, 0x0,+0,'?' }, // 189: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in
    { 0x0F1F02E,0x3487407, 0x00,0x07, 0x8,+0,'?' }, // 190: HMIGP27; HMIGP28; HMIGP29; HMIGP30; HMIGP31; Wierd1.i
    { 0x0FE5229,0x3D9850E, 0x00,0x07, 0x6,+0,'?' }, // 191: HMIGP32; HMIGP33; HMIGP34; Wierd2.i
    { 0x0FE6227,0x3D9950A, 0x00,0x07, 0x8,+0,'D' }, // 192: HMIGP35; Wierd3.i
    { 0x0FDF800,0x0C7F601, 0x0B,0x00, 0x8,+0,'D' }, // 193: HMIGP36; Kick.ins
    { 0x0FBF116,0x069F911, 0x08,0x02, 0x0,+0,'D' }, // 194: HMIGP37; HMIGP85; HMIGP86; RimShot.; rimShot.; rimshot.
    { 0x000FF26,0x0A7F802, 0x00,0x02, 0xE,+0,'s' }, // 195: HMIGP38; HMIGP40; Snare.in
    { 0x00F9F30,0x0FAE83A, 0x00,0x00, 0xE,+0,'h' }, // 196: HMIGP39; Clap.ins
    { 0x01FFA06,0x0F5F511, 0x0A,0x00, 0xF,+0,'M' }, // 197: HMIGP41; HMIGP43; HMIGP45; HMIGP47; HMIGP48; HMIGP50; Toms.ins
    { 0x0F1F52E,0x3F99906, 0x05,0x02, 0x0,+0,'h' }, // 198: HMIGP42; HMIGP44; clshat97
    { 0x0F89227,0x3D8750A, 0x00,0x03, 0x8,+0,'h' }, // 199: HMIGP46; Opnhat96
    { 0x2009F2C,0x3A4C50E, 0x00,0x09, 0xE,+0,'C' }, // 200: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym
    { 0x0009429,0x344F904, 0x10,0x0C, 0xE,+0,'C' }, // 201: HMIGP51; HMIGP53; HMIGP59; Ridecym.; ridecym.
    { 0x0F1F52E,0x3F78706, 0x09,0x02, 0x0,+0,'M' }, // 202: HMIGP54; Tamb.ins
    { 0x2F1F535,0x028F703, 0x19,0x02, 0x0,+0,'B' }, // 203: HMIGP56; Cowbell.
    { 0x100FF80,0x1F7F500, 0x00,0x00, 0xC,+0,'D' }, // 204: HMIGP58; vibrasla
    { 0x0FAFA25,0x0F99803, 0xCD,0x00, 0x0,+0,'M' }, // 205: HMIGP60; HMIGP62; mutecong
    { 0x1FAF825,0x0F7A803, 0x1B,0x00, 0x0,+0,'M' }, // 206: HMIGP61; conga.in
    { 0x1FAF825,0x0F69603, 0x21,0x00, 0xE,+0,'D' }, // 207: HMIGP63; HMIGP64; loconga.
    { 0x2F5F504,0x236F603, 0x16,0x03, 0xA,+0,'M' }, // 208: HMIGP65; HMIGP66; timbale.
    { 0x091F015,0x0E8A617, 0x1E,0x04, 0xE,+0,'D' }, // 209: HMIGP67; HMIGP68; agogo.in
    { 0x001FF0E,0x077780E, 0x06,0x04, 0xE,+0,'D' }, // 210: HMIGP69; HMIGP70; HMIGP82; shaker.i
    { 0x2079F20,0x22B950E, 0x1C,0x00, 0x0,+0,'D' }, // 211: HMIGP71; hiwhist.
    { 0x2079F20,0x23B940E, 0x1E,0x00, 0x0,+0,'D' }, // 212: HMIGP72; lowhist.
    { 0x0F7F020,0x33B8809, 0x00,0x00, 0xC,+0,'D' }, // 213: HMIGP73; higuiro.
    { 0x0F7F420,0x33B560A, 0x03,0x00, 0x0,+0,'D' }, // 214: HMIGP74; loguiro.
    { 0x05BF714,0x089F712, 0x4B,0x00, 0x0,+0,'D' }, // 215: HMIGP75; clavecb.
    { 0x0F2FA27,0x09AF612, 0x22,0x00, 0x0,+0,'D' }, // 216: HMIGP76; HMIGP77; woodblok
    { 0x1F75020,0x03B7708, 0x09,0x05, 0x0,+0,'D' }, // 217: HMIGP78; hicuica.
    { 0x1077F26,0x06B7703, 0x29,0x05, 0x0,+0,'D' }, // 218: HMIGP79; locuica.
    { 0x0F0F126,0x0FCF727, 0x44,0x40, 0x6,+0,'D' }, // 219: HMIGP80; mutringl
    { 0x0F0F126,0x0F5F527, 0x44,0x40, 0x6,+0,'D' }, // 220: HMIGP81; HMIGP83; HMIGP84; triangle
    { 0x0F3F821,0x0ADC620, 0x1C,0x00, 0xC,+0,'D' }, // 221: HMIGP87; taiko.in
    { 0x0FFFF01,0x0FFFF01, 0x3F,0x3F, 0x0,+0,'P' }, // 222: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.
    { 0x4FFEE03,0x0FFE804, 0x80,0x00, 0xC,+0,'P' }, // 223: hamM1; intM1; rickM1; DBlock.i; DBlocki
    { 0x122F603,0x0F8F3A1, 0x87,0x80, 0x6,+0,'P' }, // 224: hamM2; intM2; rickM2; GClean.i; GCleani
    { 0x007A810,0x005FA00, 0x86,0x03, 0x6,+0,'P' }, // 225: hamM4; intM4; rickM4; DToms.in; DTomsin
    { 0x053F131,0x227F232, 0x48,0x00, 0x6,+0,'c' }, // 226: hamM7; intM7; rickM7; rickM84; GOverD.i; GOverDi; Guit_fz2
    { 0x01A9161,0x01AC1E6, 0x40,0x03, 0x8,+0,'c' }, // 227: hamM8; intM8; rickM8; GMetal.i; GMetali
    { 0x071FB11,0x0B9F301, 0x00,0x00, 0x0,+0,'k' }, // 228: hamM9; intM9; rickM9; BPick.in; BPickin
    { 0x1B57231,0x098D523, 0x0B,0x00, 0x8,+0,'m' }, // 229: hamM10; intM10; rickM10; BSlap.in; BSlapin
    { 0x024D501,0x0228511, 0x0F,0x00, 0xA,+0,'v' }, // 230: hamM11; intM11; rickM11; BSynth1; BSynth1.
    { 0x025F911,0x034F131, 0x05,0x00, 0xA,+0,'m' }, // 231: hamM12; intM12; rickM12; BSynth2; BSynth2.
    { 0x01576A1,0x0378261, 0x94,0x00, 0xC,+0,'d' }, // 232: hamM15; intM15; rickM15; PSoft.in; PSoftin
    { 0x1362261,0x0084F22, 0x10,0x40, 0x8,+0,'o' }, // 233: hamM18; intM18; rickM18; PRonStr1
    { 0x2363360,0x0084F22, 0x15,0x40, 0xC,+0,'o' }, // 234: hamM19; intM19; rickM19; PRonStr2
    { 0x007F804,0x0748201, 0x0E,0x05, 0x6,+0,'G' }, // 235: hamM25; intM25; rickM25; LTrap.in; LTrapin
    { 0x0E5F131,0x174F131, 0x89,0x00, 0xC,+0,'G' }, // 236: hamM26; intM26; rickM26; LSaw.ins; LSawins
    { 0x0E3F131,0x073F172, 0x8A,0x00, 0xA,+0,'G' }, // 237: hamM27; intM27; rickM27; PolySyn; PolySyn.
    { 0x0FFF101,0x0FF5091, 0x0D,0x80, 0x6,+0,'G' }, // 238: hamM28; intM28; rickM28; Pobo.ins; Poboins
    { 0x1473161,0x007AF61, 0x0F,0x00, 0xA,+0,'G' }, // 239: hamM29; intM29; rickM29; PSweep2; PSweep2.
    { 0x0D3B303,0x024F204, 0x40,0x80, 0x4,+0,'G' }, // 240: hamM30; intM30; rickM30; LBright; LBright.
    { 0x1037531,0x0445462, 0x1A,0x40, 0xE,+0,'G' }, // 241: hamM31; intM31; rickM31; SynStrin
    { 0x021A1A1,0x116C261, 0x92,0x40, 0x6,+0,'B' }, // 242: hamM32; intM32; rickM32; SynStr2; SynStr2.
    { 0x0F0F240,0x0F4F440, 0x00,0x00, 0x4,+0,'B' }, // 243: hamM33; intM33; rickM33; low_blub
    { 0x003F1C0,0x001107E, 0x4F,0x0C, 0x2,+0,'B' }, // 244: hamM34; intM34; rickM34; DInsect; DInsect.
    { 0x0459BC0,0x015F9C1, 0x05,0x00, 0xE,+0,'B' }, // 245: hamM35; intM35; rickM35; hardshak
    { 0x0064F50,0x003FF50, 0x10,0x00, 0x0,+0,'B' }, // 246: hamM37; intM37; rickM37; WUMP.ins; WUMPins
    { 0x2F0F005,0x1B4F600, 0x08,0x00, 0xC,+0,'B' }, // 247: hamM38; intM38; rickM38; DSnare.i; DSnarei
    { 0x0F2F931,0x042F210, 0x40,0x00, 0x4,+0,'B' }, // 248: hamM39; intM39; rickM39; DTimp.in; DTimpin
    { 0x00FFF7E,0x00F2F6E, 0x00,0x00, 0xE,+0,'V' }, // 249: IntP93; hamM40; intM40; rickM40; DRevCym; DRevCym.; drevcym
    { 0x2F95401,0x2FB5401, 0x19,0x00, 0x8,+0,'V' }, // 250: hamM41; intM41; rickM41; Dorky.in; Dorkyin
    { 0x0665F53,0x0077F00, 0x05,0x00, 0x6,+0,'V' }, // 251: hamM42; intM42; rickM42; DFlab.in; DFlabin
    { 0x003F1C0,0x006707E, 0x4F,0x03, 0x2,+0,'V' }, // 252: hamM43; intM43; rickM43; DInsect2
    { 0x1111EF0,0x11411E2, 0x00,0xC0, 0x8,+0,'V' }, // 253: hamM44; intM44; rickM44; DChopper
    { 0x0F0A006,0x075C584, 0x00,0x00, 0xE,+0,'H' }, // 254: IntP50; hamM45; hamP50; intM45; rickM45; rickP50; DShot.in; DShotin; shot.ins; shotins
    { 0x1F5F213,0x0F5F111, 0xC6,0x05, 0x0,+0,'H' }, // 255: hamM46; intM46; rickM46; KickAss; KickAss.
    { 0x153F101,0x274F111, 0x49,0x02, 0x6,+0,'M' }, // 256: hamM47; intM47; rickM47; RVisCool
    { 0x0E4F4D0,0x006A29E, 0x80,0x00, 0x8,+0,'S' }, // 257: hamM48; intM48; rickM48; DSpring; DSpring.
    { 0x0871321,0x0084221, 0xCD,0x80, 0x8,+0,'S' }, // 258: intM49; Chorar22
    { 0x005F010,0x004D011, 0x25,0x80, 0xE,+0,'?' }, // 259: IntP27; hamP27; rickP27; timpani; timpani.
    { 0x065B400,0x075B400, 0x00,0x00, 0x7,+0,'?' }, // 260: IntP28; hamP28; rickP28; timpanib
    { 0x02AF800,0x145F600, 0x03,0x00, 0x4,+0,'?' }, // 261: IntP29; hamP29; rickP29; APS043.i; APS043i
    { 0x0FFF830,0x07FF511, 0x44,0x00, 0x8,+0,'?' }, // 262: IntP30; hamP30; rickP30; mgun3.in; mgun3in
    { 0x0F9F900,0x023F110, 0x08,0x00, 0xA,+0,'?' }, // 263: IntP31; hamP31; rickP31; kick4r.i; kick4ri
    { 0x0F9F900,0x026F180, 0x04,0x00, 0x8,+0,'?' }, // 264: IntP32; hamP32; rickP32; timb1r.i; timb1ri
    { 0x1FDF800,0x059F800, 0xC4,0x00, 0xA,+0,'?' }, // 265: IntP33; hamP33; rickP33; timb2r.i; timb2ri
    { 0x06FFA00,0x08FF600, 0x0B,0x00, 0x0,+0,'D' }, // 266: IntP36; hamP36; rickP36; hartbeat
    { 0x0F9F900,0x023F191, 0x04,0x00, 0x8,+0,'D' }, // 267: IntP37; IntP38; IntP39; IntP40; hamP37; hamP38; hamP39; hamP40; rickP37; rickP38; rickP39; rickP40; tom1r.in; tom1rin
    { 0x097C802,0x097C802, 0x00,0x00, 0x1,+0,'M' }, // 268: IntP41; IntP42; IntP43; IntP44; IntP71; hamP71; rickP41; rickP42; rickP43; rickP44; rickP71; tom2.ins; tom2ins; woodbloc
    { 0x0BFFA01,0x0BFDA02, 0x00,0x00, 0x8,+0,'M' }, // 269: IntP45; hamP45; rickP45; tom.ins; tomins
    { 0x2F0FB01,0x096F701, 0x10,0x00, 0xE,+0,'h' }, // 270: IntP46; IntP47; hamP46; hamP47; rickP46; rickP47; conga.in; congain
    { 0x002FF04,0x007FF00, 0x00,0x00, 0xE,+0,'M' }, // 271: IntP48; hamP48; rickP48; snare01r
    { 0x0F0F006,0x0B7F600, 0x00,0x00, 0xC,+0,'C' }, // 272: IntP49; hamP49; rickP49; slap.ins; slapins
    { 0x0F0F006,0x034C4C4, 0x00,0x03, 0xE,+0,'C' }, // 273: IntP51; hamP51; rickP51; snrsust; snrsust.
    { 0x0F0F019,0x0F7B720, 0x0E,0x0A, 0xE,+0,'C' }, // 274: IntP52; rickP52; snare.in; snarein
    { 0x0F0F006,0x0B4F600, 0x00,0x00, 0xE,+0,'b' }, // 275: IntP53; hamP53; rickP53; synsnar; synsnar.
    { 0x0F0F006,0x0B6F800, 0x00,0x00, 0xE,+0,'M' }, // 276: IntP54; hamP54; rickP54; synsnr1; synsnr1.
    { 0x0F2F931,0x008F210, 0x40,0x00, 0x4,+0,'B' }, // 277: IntP56; hamP56; rickP56; rimshotb
    { 0x0BFFA01,0x0BFDA09, 0x00,0x08, 0x8,+0,'C' }, // 278: IntP57; hamP57; rickP57; rimshot; rimshot.
    { 0x210BA2E,0x2F4B40E, 0x0E,0x00, 0xE,+0,'D' }, // 279: IntP58; IntP59; hamP58; hamP59; rickP58; rickP59; crash.in; crashin
    { 0x210FA2E,0x2F4F40E, 0x0E,0x00, 0xE,+0,'M' }, // 280: IntP60; rickP60; cymbal.i; cymbali
    { 0x2A2B2A4,0x1D49703, 0x02,0x80, 0xE,+0,'M' }, // 281: IntP61; hamP60; hamP61; rickP61; cymbali; cymbals; cymbals.
    { 0x200FF04,0x206FFC3, 0x00,0x00, 0x8,+0,'D' }, // 282: IntP62; hamP62; rickP62; hammer5r
    { 0x200FF04,0x2F5F6C3, 0x00,0x00, 0x8,+0,'D' }, // 283: IntP63; IntP64; hamP63; hamP64; rickP63; rickP64; hammer3; hammer3.
    { 0x0E1C000,0x153951E, 0x80,0x80, 0x6,+0,'M' }, // 284: IntP65; hamP65; rickP65; ride2.in; ride2in
    { 0x200FF03,0x3F6F6C4, 0x00,0x00, 0x8,+0,'M' }, // 285: IntP66; hamP66; rickP66; hammer1; hammer1.
    { 0x202FF4E,0x3F7F701, 0x00,0x00, 0x8,+0,'D' }, // 286: IntP67; IntP68; rickP67; rickP68; tambour; tambour.
    { 0x202FF4E,0x3F6F601, 0x00,0x00, 0x8,+0,'D' }, // 287: IntP69; IntP70; hamP69; hamP70; rickP69; rickP70; tambou2; tambou2.
    { 0x2588A51,0x018A452, 0x00,0x00, 0xC,+0,'D' }, // 288: IntP72; hamP72; rickP72; woodblok
    { 0x0FFFB13,0x0FFE808, 0x40,0x00, 0x8,+0,'D' }, // 289: IntP73; IntP74; rickP73; rickP74; claves.i; clavesi
    { 0x0FFEE03,0x0FFE808, 0x40,0x00, 0xC,+0,'D' }, // 290: IntP75; hamP75; rickP75; claves2; claves2.
    { 0x0FFEE05,0x0FFE808, 0x55,0x00, 0xE,+0,'D' }, // 291: IntP76; hamP76; rickP76; claves3; claves3.
    { 0x0FF0006,0x0FDF715, 0x3F,0x0D, 0x1,+0,'D' }, // 292: IntP77; hamP77; rickP77; clave.in; clavein
    { 0x0F6F80E,0x0F6F80E, 0x00,0x00, 0x0,+0,'D' }, // 293: IntP78; IntP79; hamP78; hamP79; rickP78; rickP79; agogob4; agogob4.
    { 0x060F207,0x072F212, 0x4F,0x09, 0x8,+0,'D' }, // 294: IntP80; hamP80; rickP80; clarion; clarion.
    { 0x061F217,0x074F212, 0x4F,0x08, 0x8,+0,'D' }, // 295: IntP81; hamP81; rickP81; trainbel
    { 0x022FB18,0x012F425, 0x88,0x80, 0x8,+0,'D' }, // 296: IntP82; hamP82; rickP82; gong.ins; gongins
    { 0x0F0FF04,0x0B5F4C1, 0x00,0x00, 0xE,+0,'D' }, // 297: IntP83; hamP83; rickP83; kalimbai
    { 0x02FC811,0x0F5F531, 0x2D,0x00, 0xC,+0,'D' }, // 298: IntP84; IntP85; hamP84; hamP85; rickP84; rickP85; xylo1.in; xylo1in
    { 0x03D6709,0x3FC692C, 0x00,0x00, 0xE,+0,'D' }, // 299: IntP86; hamP86; rickP86; match.in; matchin
    { 0x053D144,0x05642B2, 0x80,0x15, 0xE,+0,'D' }, // 300: IntP87; hamP87; rickP87; breathi; breathi.
    { 0x0F0F007,0x0DC5C00, 0x00,0x00, 0xE,+0,'?' }, // 301: IntP88; rickP88; scratch; scratch.
    { 0x00FFF7E,0x00F3F6E, 0x00,0x00, 0xE,+0,'?' }, // 302: IntP89; hamP89; rickP89; crowd.in; crowdin
    { 0x0B3FA00,0x005D000, 0x00,0x00, 0xC,+0,'?' }, // 303: IntP90; rickP90; taiko.in; taikoin
    { 0x0FFF832,0x07FF511, 0x84,0x00, 0xE,+0,'?' }, // 304: IntP91; hamP91; rickP91; rlog.ins; rlogins
    { 0x0089FD4,0x0089FD4, 0xC0,0xC0, 0x4,+0,'?' }, // 305: IntP92; hamP92; rickP92; knock.in; knockin
    { 0x253B1C4,0x083B1D2, 0x8F,0x84, 0x2,+0,'O' }, // 306: hamM52; rickM94; Fantasy1; fantasy1
    { 0x175F5C2,0x074F2D1, 0x21,0x83, 0xE,+0,'O' }, // 307: hamM53; guitar1
    { 0x1F6FB34,0x04394B1, 0x83,0x00, 0xC,+0,'c' }, // 308: hamM55; hamatmos
    { 0x0658722,0x0186421, 0x46,0x80, 0x0,+0,'T' }, // 309: hamM56; hamcalio
    { 0x0BDF211,0x09BA004, 0x46,0x40, 0x8,+0,'T' }, // 310: hamM59; moonins
    { 0x144F221,0x3457122, 0x8A,0x40, 0x0,+0,'T' }, // 311: hamM62; Polyham3
    { 0x144F221,0x1447122, 0x8A,0x40, 0x0,+0,'T' }, // 312: hamM63; Polyham
    { 0x053F101,0x153F108, 0x40,0x40, 0x0,+0,'X' }, // 313: hamM64; sitar2i
    { 0x102FF00,0x3FFF200, 0x08,0x00, 0x0,+0,'T' }, // 314: hamM70; weird1a
    { 0x144F221,0x345A122, 0x8A,0x40, 0x0,+0,'F' }, // 315: hamM71; Polyham4
    { 0x028F131,0x018F031, 0x0F,0x00, 0xA,+0,'F' }, // 316: hamM72; hamsynbs
    { 0x307D7E1,0x107B6E0, 0x8D,0x00, 0x1,+0,'F' }, // 317: hamM73; Ocasynth
    { 0x03DD500,0x02CD500, 0x11,0x00, 0xA,+0,'G' }, // 318: hamM120; hambass1
    { 0x1199563,0x219C420, 0x46,0x00, 0x8,+0,'X' }, // 319: hamM121; hamguit1
    { 0x044D08C,0x2F4D181, 0xA1,0x80, 0x8,+0,'X' }, // 320: hamM122; hamharm2
    { 0x0022171,0x1035231, 0x93,0x80, 0x0,+0,'X' }, // 321: hamM123; hamvox1
    { 0x1611161,0x01311A2, 0x91,0x80, 0x8,+0,'X' }, // 322: hamM124; hamgob1
    { 0x25666E1,0x02665A1, 0x4C,0x00, 0x0,+0,'X' }, // 323: hamM125; hamblow1
    { 0x144F5C6,0x018F6C1, 0x5C,0x83, 0xE,+0,'?' }, // 324: hamP9; cowbell
    { 0x0D0CCC0,0x028EAC1, 0x10,0x00, 0x0,+0,'?' }, // 325: hamP10; rickP100; conghi.i; conghii
    { 0x038FB00,0x0DAF400, 0x00,0x00, 0x4,+0,'?' }, // 326: hamP12; rickP15; hamkick; kick3.in
    { 0x2BFFB15,0x31FF817, 0x0A,0x00, 0x0,+0,'?' }, // 327: hamP13; rimshot2
    { 0x0F0F01E,0x0B6F70E, 0x00,0x00, 0xE,+0,'?' }, // 328: hamP14; rickP16; hamsnr1; snr1.ins
    { 0x0BFFBC6,0x02FE8C9, 0x00,0x00, 0xE,+0,'?' }, // 329: hamP15; handclap
    { 0x2F0F006,0x2B7F800, 0x00,0x00, 0xE,+0,'?' }, // 330: hamP16; smallsnr
    { 0x0B0900E,0x0BF990E, 0x03,0x03, 0xA,+0,'?' }, // 331: hamP17; rickP95; clsdhhat
    { 0x283E0C4,0x14588C0, 0x81,0x00, 0xE,+0,'?' }, // 332: hamP18; rickP94; openhht2
    { 0x097C802,0x040E000, 0x00,0x00, 0x1,+0,'M' }, // 333: hamP41; hamP42; hamP43; hamP44; tom2ins
    { 0x00FFF2E,0x04AF602, 0x0A,0x1B, 0xE,+0,'C' }, // 334: hamP52; snarein
    { 0x3A5F0EE,0x36786CE, 0x00,0x00, 0xE,+0,'D' }, // 335: hamP67; hamP68; tambour
    { 0x0B0FCD6,0x008BDD6, 0x00,0x05, 0xA,+0,'D' }, // 336: hamP73; hamP74; clavesi
    { 0x0F0F007,0x0DC5C00, 0x08,0x00, 0xE,+0,'?' }, // 337: hamP88; scratch
    { 0x0E7F301,0x078F211, 0x58,0x00, 0xA,+0,'F' }, // 338: rickM76; Bass.ins
    { 0x0EFF230,0x078F521, 0x1E,0x00, 0xE,+0,'F' }, // 339: rickM77; Basnor04
    { 0x019D530,0x01B6171, 0x88,0x80, 0xC,+0,'F' }, // 340: rickM78; Synbass1
    { 0x001F201,0x0B7F211, 0x0D,0x0D, 0xA,+0,'F' }, // 341: rickM79; Synbass2
    { 0x03DD500,0x02CD500, 0x14,0x00, 0xA,+0,'L' }, // 342: rickM80; Pickbass
    { 0x010E032,0x0337D16, 0x87,0x84, 0x8,+0,'L' }, // 343: rickM82; Harpsi1.
    { 0x0F8F161,0x008F062, 0x80,0x80, 0x8,+0,'L' }, // 344: rickM83; Guit_el3
    { 0x0745391,0x0755451, 0x00,0x00, 0xA,+0,'p' }, // 345: rickM88; Orchit2.
    { 0x08E6121,0x09E7231, 0x15,0x00, 0xE,+0,'p' }, // 346: rickM89; Brass11.
    { 0x0BC7321,0x0BC8121, 0x19,0x00, 0xE,+0,'p' }, // 347: rickM90; Brass2.i
    { 0x23C7320,0x0BC8121, 0x19,0x00, 0xE,+0,'p' }, // 348: rickM91; Brass3.i
    { 0x209A060,0x20FF014, 0x02,0x80, 0x1,+0,'p' }, // 349: rickM92; Squ_wave
    { 0x064F207,0x075F612, 0x73,0x00, 0x8,+0,'X' }, // 350: rickM99; Agogo.in
    { 0x2B7F811,0x006F311, 0x46,0x00, 0x8,+0,'?' }, // 351: rickP13; kick1.in
    { 0x218F401,0x008F800, 0x00,0x00, 0xC,+0,'?' }, // 352: rickP17; rickP19; snare1.i; snare4.i
    { 0x0F0F009,0x0F7B700, 0x0E,0x00, 0xE,+0,'?' }, // 353: rickP18; rickP20; snare2.i; snare5.i
    { 0x0FEF812,0x07ED511, 0x47,0x00, 0xE,+0,'?' }, // 354: rickP21; rickP22; rickP23; rickP24; rickP25; rickP26; rocktom.
    { 0x2F4F50E,0x424120CA, 0x00,0x51, 0x3,+0,'?' }, // 355: rickP93; openhht1
    { 0x0DFDCC2,0x026C9C0, 0x17,0x00, 0x0,+0,'?' }, // 356: rickP97; guirol.i
    { 0x0D0ACC0,0x028EAC1, 0x18,0x00, 0x0,+0,'?' }, // 357: rickP101; rickP102; congas2.
    { 0x0A7CDC2,0x028EAC1, 0x2B,0x02, 0x0,+0,'?' }, // 358: rickP103; rickP104; bongos.i
};

const struct adlinsdata adlins[] =
{
    // Amplitude begins at 2487.8, peaks 3203.1 at 0.1s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    {   0,  0,  0, 1,0x04,  1560,  1560 }, // 0: GM0; AcouGrandPiano

    // Amplitude begins at 3912.6,
    // fades to 20% at 2.1s, keyoff fades to 20% in 2.1s.
    {   1,  1,  0, 1,0x04,  2080,  2080 }, // 1: GM1; BrightAcouGrand

    // Amplitude begins at 2850.7, peaks 4216.6 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    {   2,  2,  0, 1,0x04,  1973,  1973 }, // 2: GM2; ElecGrandPiano

    // Amplitude begins at 1714.3, peaks 1785.0 at 0.1s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    {   3,  3,  0, 1,0x04,  2226,  2226 }, // 3: GM3; Honky-tonkPiano

    // Amplitude begins at 4461.0, peaks 6341.0 at 0.0s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    {   4,  4,  0, 1,0x04,  2606,  2606 }, // 4: GM4; Rhodes Piano

    // Amplitude begins at 4781.0, peaks 6329.2 at 0.0s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    {   5,  5,  0, 1,0x04,  2640,  2640 }, // 5: GM5; Chorused Piano

    // Amplitude begins at 1162.2, peaks 1404.5 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    {   6,  6,  0, 1,0x04,  1186,  1186 }, // 6: GM6; Harpsichord

    // Amplitude begins at 1144.6, peaks 1235.5 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    {   7,  7,  0, 1,0x04,  2313,  2313 }, // 7: GM7; Clavinet

    // Amplitude begins at 2803.9, peaks 2829.0 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {   8,  8,  0, 1,0x04,   906,   906 }, // 8: GM8; Celesta

    // Amplitude begins at 3085.2, peaks 3516.8 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    {   9,  9,  0, 1,0x04,  1120,  1120 }, // 9: GM9; Glockenspiel

    // Amplitude begins at 2073.6, peaks 3449.1 at 0.1s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  10, 10,  0, 1,0x04,   453,   453 }, // 10: GM10; Music box

    // Amplitude begins at 2976.7, peaks 3033.0 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    {  11, 11,  0, 1,0x04,  1746,  1746 }, // 11: GM11; Vibraphone

    // Amplitude begins at 3371.2, peaks 3554.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    {  12, 12,  0, 1,0x04,    80,    80 }, // 12: GM12; Marimba

    // Amplitude begins at 2959.7, peaks 3202.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    {  13, 13,  0, 1,0x04,   100,   100 }, // 13: GM13; Xylophone

    // Amplitude begins at 2057.2, peaks 2301.4 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    {  14, 14,  0, 1,0x04,  1306,  1306 }, // 14: GM14; Tubular Bells

    // Amplitude begins at 1672.7, peaks 2154.8 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  15, 15,  0, 1,0x04,   320,   320 }, // 15: GM15; Dulcimer

    // Amplitude begins at 2324.3, peaks 2396.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  16, 16,  0, 1,0x04, 40000,    40 }, // 16: GM16; HMIGM16; Hammond Organ; am016.in

    // Amplitude begins at 2299.6, peaks 2620.0 at 14.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  17, 17,  0, 1,0x04, 40000,    20 }, // 17: GM17; HMIGM17; Percussive Organ; am017.in

    // Amplitude begins at  765.8, peaks 2166.1 at 4.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  18, 18,  0, 1,0x62, 40000,    20 }, // 18: GM18; HMIGM18; Rock Organ; am018.in

    // Amplitude begins at  336.5, peaks 2029.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  19, 19,  0, 1,0x04, 40000,   500 }, // 19: GM19; HMIGM19; Church Organ; am019.in

    // Amplitude begins at  153.2, peaks 4545.1 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  20, 20,  0, 1,0x62, 40000,   280 }, // 20: GM20; HMIGM20; Reed Organ; am020.in

    // Amplitude begins at    0.0, peaks  883.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  21, 21,  0, 1,0x0C, 40000,     6 }, // 21: GM21; HMIGM21; Accordion; am021.in

    // Amplitude begins at  181.4, peaks 3015.8 at 25.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  22, 22,  0, 1,0x04, 40000,    86 }, // 22: GM22; HMIGM22; Harmonica; am022.in

    // Amplitude begins at    3.2, peaks 3113.2 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  23, 23,  0, 1,0x04, 40000,    73 }, // 23: GM23; HMIGM23; Tango Accordion; am023.in

    // Amplitude begins at  955.9, peaks 1147.3 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    {  24, 24,  0, 1,0x04,  1120,  1120 }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in

    // Amplitude begins at 1026.8, peaks 1081.7 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    {  25, 25,  0, 1,0x04,  1973,  1973 }, // 25: GM25; HMIGM25; Acoustic Guitar2; am025.in

    // Amplitude begins at 4157.9, peaks 4433.1 at 0.1s,
    // fades to 20% at 10.3s, keyoff fades to 20% in 10.3s.
    {  26, 26,  0, 1,0x04, 10326, 10326 }, // 26: GM26; HMIGM26; Electric Guitar1; am026.in

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    {  27, 27,  0, 1,0x04,  1266,  1266 }, // 27: GM27; Electric Guitar2

    // Amplitude begins at 3418.1,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  28, 28,  0, 1,0x00, 40000,     0 }, // 28: GM28; HMIGM28; Electric Guitar3; am028.in

    // Amplitude begins at   57.7, peaks 1476.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  29, 29,  0, 1,0x04, 40000,    13 }, // 29: GM29; Overdrive Guitar

    // Amplitude begins at  396.9, peaks 1480.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  30, 30,  0, 1,0x04, 40000,    13 }, // 30: GM30; HMIGM30; Distorton Guitar; am030.in

    // Amplitude begins at 1424.2, peaks 2686.1 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  31, 31,  0, 1,0xEE, 40000,     0 }, // 31: GM31; HMIGM31; Guitar Harmonics; am031.in

    // Amplitude begins at 2281.6, peaks 2475.5 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    {  32, 32,  0, 1,0xEE,  1153,  1153 }, // 32: GM32; HMIGM32; Acoustic Bass; am032.in

    // Amplitude begins at 1212.1, peaks 1233.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  33, 33,  0, 1,0x0C, 40000,    13 }, // 33: GM33; HMIGM39; Electric Bass 1; am039.in

    // Amplitude begins at 3717.2, peaks 4282.2 at 0.1s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    {  34, 34,  0, 1,0x04,  1540,  1540 }, // 34: GM34; HMIGM34; Electric Bass 2; am034.in

    // Amplitude begins at   49.0, peaks 3572.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  35, 35,  0, 1,0x04, 40000,     0 }, // 35: GM35; HMIGM35; Fretless Bass; am035.in

    // Amplitude begins at 1755.7, peaks 2777.7 at 0.0s,
    // fades to 20% at 3.6s, keyoff fades to 20% in 3.6s.
    {  36, 36,  0, 1,0x04,  3566,  3566 }, // 36: GM36; HMIGM36; Slap Bass 1; am036.in

    // Amplitude begins at 1352.0, peaks 2834.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  37, 37,  0, 1,0x04, 40000,    53 }, // 37: GM37; Slap Bass 2

    // Amplitude begins at 3787.6,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  38, 38,  0, 1,0x04,   540,   540 }, // 38: GM38; HMIGM38; Synth Bass 1; am038.in

    // Amplitude begins at 1212.1, peaks 1233.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  33, 33,  0, 1,0x04, 40000,    13 }, // 39: GM39; HMIGM33; Synth Bass 2; am033.in

    // Amplitude begins at 1114.5, peaks 2586.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  39, 39,  0, 1,0x0C, 40000,   106 }, // 40: GM40; HMIGM40; Violin; am040.in

    // Amplitude begins at    0.5, peaks 1262.2 at 16.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  40, 40,  0, 1,0x04, 40000,   126 }, // 41: GM41; HMIGM41; Viola; am041.in

    // Amplitude begins at 1406.9, peaks 2923.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  41, 41,  0, 1,0x04, 40000,     6 }, // 42: GM42; HMIGM42; Cello; am042.in

    // Amplitude begins at    5.0, peaks 2928.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  42, 42,  0, 1,0x04, 40000,   193 }, // 43: GM43; HMIGM43; Contrabass; am043.in

    // Amplitude begins at    0.6, peaks 1972.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  43, 43,  0, 1,0x04, 40000,   106 }, // 44: GM44; HMIGM44; Tremulo Strings; am044.in

    // Amplitude begins at  123.0, peaks 2834.7 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  44, 44,  0, 1,0x3D,   326,   326 }, // 45: GM45; HMIGM45; Pizzicato String; am045.in

    // Amplitude begins at 2458.2, peaks 3405.7 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {  45, 45,  0, 1,0x0C,   853,   853 }, // 46: GM46; HMIGM46; Orchestral Harp; am046.in

    // Amplitude begins at 2061.3, peaks 2787.1 at 0.1s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {  46, 46,  0, 1,0x04,   906,   906 }, // 47: GM47; HMIGM47; Timpany; am047.in

    // Amplitude begins at  845.0, peaks 1851.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  47, 47,  0, 1,0x04, 40000,    66 }, // 48: GM48; HMIGM48; String Ensemble1; am048.in

    // Amplitude begins at    0.0, peaks 1531.6 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  48, 48,  0, 1,0x00, 40000,    40 }, // 49: GM49; HMIGM49; String Ensemble2; am049.in

    // Amplitude begins at 2323.6, peaks 4179.7 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    {  49, 49,  0, 1,0x04, 40000,   773 }, // 50: GM50; HMIGM50; Synth Strings 1; am050.in

    // Amplitude begins at    0.0, peaks 1001.6 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  50, 50,  0, 1,0x04, 40000,   280 }, // 51: GM51; HMIGM51; SynthStrings 2; am051.in

    // Amplitude begins at  889.7, peaks 3770.3 at 20.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  51, 51,  0, 1,0x04, 40000,   133 }, // 52: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in

    // Amplitude begins at    6.6, peaks 3005.9 at 0.2s,
    // fades to 20% at 4.3s, keyoff fades to 20% in 4.3s.
    {  52, 52,  0, 1,0x04,  4346,  4346 }, // 53: GM53; HMIGM53; rickM86; Oohs.ins; Voice Oohs; am053.in

    // Amplitude begins at   48.8, peaks 3994.2 at 19.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  53, 53,  0, 1,0x62, 40000,   106 }, // 54: GM54; HMIGM54; Synth Voice; am054.in

    // Amplitude begins at  899.0, peaks 1546.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  54, 54,  0, 1,0x04,   260,   260 }, // 55: GM55; HMIGM55; Orchestra Hit; am055.in

    // Amplitude begins at   39.3, peaks  930.6 at 39.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  55, 55,  0, 1,0x04, 40000,    20 }, // 56: GM56; HMIGM56; Trumpet; am056.in

    // Amplitude begins at   40.6, peaks  925.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  56, 56,  0, 1,0x04, 40000,    20 }, // 57: GM57; HMIGM57; Trombone; am057.in

    // Amplitude begins at   39.5, peaks 1061.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  57, 57,  0, 1,0x00, 40000,    26 }, // 58: GM58; HMIGM58; Tuba; am058.in

    // Amplitude begins at  886.2, peaks 2598.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  58, 58,  0, 1,0x62, 40000,    20 }, // 59: GM59; HMIGM59; Muted Trumpet; am059.in

    // Amplitude begins at    6.8, peaks 4177.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  59, 59,  0, 1,0x62, 40000,     6 }, // 60: GM60; HMIGM60; French Horn; am060.in

    // Amplitude begins at    4.0, peaks 1724.3 at 21.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  60, 60,  0, 1,0x04, 40000,     0 }, // 61: GM61; HMIGM61; Brass Section; am061.in

    // Amplitude begins at    7.2, peaks 2980.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  61, 61,  0, 1,0x04, 40000,    20 }, // 62: GM62; Synth Brass 1

    // Amplitude begins at  894.6, peaks 4216.8 at 0.1s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    {  62, 62,  0, 1,0x62,   700,   700 }, // 63: GM63; HMIGM63; Synth Brass 2; am063.in

    // Amplitude begins at  674.0, peaks 3322.1 at 1.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  63, 63,  0, 1,0x04, 40000,   126 }, // 64: GM64; HMIGM64; Soprano Sax; am064.in

    // Amplitude begins at    3.5, peaks 1727.6 at 14.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  64, 64,  0, 1,0x00, 40000,    20 }, // 65: GM65; HMIGM65; Alto Sax; am065.in

    // Amplitude begins at  979.4, peaks 3008.4 at 27.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  65, 65,  0, 1,0x00, 40000,   106 }, // 66: GM66; HMIGM66; Tenor Sax; am066.in

    // Amplitude begins at    3.0, peaks 1452.8 at 14.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  66, 66,  0, 1,0x62, 40000,     0 }, // 67: GM67; HMIGM67; Baritone Sax; am067.in

    // Amplitude begins at  486.9, peaks 2127.8 at 39.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  67, 67,  0, 1,0x04, 40000,    66 }, // 68: GM68; HMIGM68; Oboe; am068.in

    // Amplitude begins at    5.0, peaks  784.7 at 34.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  68, 68,  0, 1,0x04, 40000,     6 }, // 69: GM69; HMIGM69; English Horn; am069.in

    // Amplitude begins at   10.1, peaks 3370.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  69, 69,  0, 1,0x04, 40000,     6 }, // 70: GM70; HMIGM70; Bassoon; am070.in

    // Amplitude begins at    4.5, peaks 1934.6 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  70, 70,  0, 1,0x62, 40000,    60 }, // 71: GM71; HMIGM71; Clarinet; am071.in

    // Amplitude begins at   64.0, peaks 4162.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  71, 71,  0, 1,0x04, 40000,     6 }, // 72: GM72; HMIGM72; Piccolo; am072.in

    // Amplitude begins at    0.5, peaks 2752.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  72, 72,  0, 1,0x3D, 40000,     6 }, // 73: GM73; HMIGM73; Flute; am073.in

    // Amplitude begins at    5.6, peaks 2782.3 at 34.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  73, 73,  0, 1,0x62, 40000,     6 }, // 74: GM74; HMIGM74; Recorder; am074.in

    // Amplitude begins at  360.9, peaks 4296.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  74, 74,  0, 1,0x04, 40000,     6 }, // 75: GM75; HMIGM75; Pan Flute; am075.in

    // Amplitude begins at    0.8, peaks 2730.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  75, 75,  0, 1,0x04, 40000,    40 }, // 76: GM76; HMIGM76; Bottle Blow; am076.in

    // Amplitude begins at    7.6, peaks 3112.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  76, 76,  0, 1,0x04, 40000,    46 }, // 77: GM77; HMIGM77; Shakuhachi; am077.in

    // Amplitude begins at    0.0, peaks 2920.2 at 32.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  77, 77,  0, 0,0x04, 40000,    73 }, // 78: GM78; HMIGM78; Whistle; am078.in

    // Amplitude begins at    5.0, peaks 3460.4 at 13.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  78, 78,  0, 0,0x04, 40000,    66 }, // 79: GM79; HMIGM79; Ocarina; am079.in

    // Amplitude begins at 2183.6, peaks 2909.4 at 29.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  79, 79,  0, 1,0x04, 40000,   453 }, // 80: GM80; HMIGM80; Lead 1 squareea; am080.in

    // Amplitude begins at 1288.7, peaks 1362.9 at 35.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  80, 80,  0, 1,0xF4, 40000,     6 }, // 81: GM81; HMIGM81; Lead 2 sawtooth; am081.in

    // Amplitude begins at    0.4, peaks 3053.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  81, 81,  0, 1,0x04, 40000,    26 }, // 82: GM82; HMIGM82; Lead 3 calliope; am082.in

    // Amplitude begins at  867.8, peaks 5910.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  82, 82,  0, 1,0x04, 40000,   300 }, // 83: GM83; HMIGM83; Lead 4 chiff; am083.in

    // Amplitude begins at  991.1, peaks 3848.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  83, 83,  0, 1,0x04, 40000,     6 }, // 84: GM84; HMIGM84; Lead 5 charang; am084.in

    // Amplitude begins at    0.5, peaks 2501.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  84, 84,  0, 1,0x04, 40000,    60 }, // 85: GM85; HMIGM85; rickM87; Lead 6 voice; Solovox.; am085.in

    // Amplitude begins at  113.3, peaks 1090.0 at 29.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  85, 85,  0, 1,0x04, 40000,   126 }, // 86: GM86; HMIGM86; rickM93; Lead 7 fifths; Saw_wave; am086.in

    // Amplitude begins at 2582.4, peaks 3331.5 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  86, 86,  0, 1,0x04, 40000,    13 }, // 87: GM87; HMIGM87; Lead 8 brass; am087.in

    // Amplitude begins at 1504.8, peaks 3734.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  87, 87,  0, 1,0x04, 40000,   160 }, // 88: GM88; HMIGM88; Pad 1 new age; am088.in

    // Amplitude begins at 1679.1, peaks 3279.1 at 24.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.9s.
    {  88, 88,  0, 1,0x04, 40000,  2880 }, // 89: GM89; HMIGM89; Pad 2 warm; am089.in

    // Amplitude begins at  641.8, peaks 4073.9 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  89, 89,  0, 1,0x04, 40000,   106 }, // 90: GM90; HMIGM90; Pad 3 polysynth; am090.in

    // Amplitude begins at    7.2, peaks 4761.3 at 6.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.5s.
    {  90, 90,  0, 1,0x04, 40000,  1526 }, // 91: GM91; HMIGM91; rickM95; Pad 4 choir; Spacevo.; am091.in

    // Amplitude begins at    0.0, peaks 3767.6 at 1.2s,
    // fades to 20% at 4.6s, keyoff fades to 20% in 0.0s.
    {  91, 91,  0, 1,0x04,  4586,     6 }, // 92: GM92; HMIGM92; Pad 5 bowedpad; am092.in

    // Amplitude begins at    0.0, peaks 1692.3 at 0.6s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 2.8s.
    {  92, 92,  0, 1,0x04,  2773,  2773 }, // 93: GM93; HMIGM93; Pad 6 metallic; am093.in

    // Amplitude begins at    0.0, peaks 3007.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  93, 93,  0, 1,0x04, 40000,   193 }, // 94: GM94; HMIGM94; Pad 7 halo; am094.in

    // Amplitude begins at 2050.9, peaks 4177.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  94, 94,  0, 1,0x04, 40000,    53 }, // 95: GM95; HMIGM95; Pad 8 sweep; am095.in

    // Amplitude begins at  852.7, peaks 2460.9 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    {  95, 95,  0, 1,0x04,   993,   993 }, // 96: GM96; HMIGM96; FX 1 rain; am096.in

    // Amplitude begins at    0.0, peaks 4045.6 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    {  96, 96,  0, 1,0x04, 40000,   580 }, // 97: GM97; HMIGM97; FX 2 soundtrack; am097.in

    // Amplitude begins at 1790.2, peaks 3699.2 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  97, 97,  0, 1,0xF4,   540,   540 }, // 98: GM98; HMIGM98; FX 3 crystal; am098.in

    // Amplitude begins at  992.2, peaks 1029.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  98, 98,  0, 1,0x04, 40000,    93 }, // 99: GM99; HMIGM99; FX 4 atmosphere; am099.in

    // Amplitude begins at 3083.3, peaks 3480.3 at 0.0s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    {  99, 99,  0, 1,0xF4,  2233,  2233 }, // 100: GM100; HMIGM100; hamM51; FX 5 brightness; am100.in; am100in

    // Amplitude begins at    0.0, peaks 1686.6 at 2.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 100,100,  0, 1,0x04, 40000,   800 }, // 101: GM101; HMIGM101; FX 6 goblins; am101.in

    // Amplitude begins at    0.0, peaks 1834.1 at 4.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 101,101,  0, 1,0x3D, 40000,   773 }, // 102: GM102; HMIGM102; rickM98; Echodrp1; FX 7 echoes; am102.in

    // Amplitude begins at   88.5, peaks 2197.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 102,102,  0, 1,0x04, 40000,   453 }, // 103: GM103; HMIGM103; FX 8 sci-fi; am103.in

    // Amplitude begins at 2640.7, peaks 2812.3 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 103,103,  0, 1,0x04,  2306,  2306 }, // 104: GM104; HMIGM104; Sitar; am104.in

    // Amplitude begins at 2465.5, peaks 2912.3 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 104,104,  0, 1,0x04,  1053,  1053 }, // 105: GM105; HMIGM105; Banjo; am105.in

    // Amplitude begins at  933.2,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 105,105,  0, 1,0x04,  1160,  1160 }, // 106: GM106; HMIGM106; Shamisen; am106.in

    // Amplitude begins at 2865.6,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 106,106,  0, 1,0x04,   653,   653 }, // 107: GM107; HMIGM107; Koto; am107.in

    // Amplitude begins at 2080.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 107,107,  0, 1,0x04,   153,   153 }, // 108: GM108; HMIGM108; Kalimba; am108.in

    // Amplitude begins at    6.6, peaks 2652.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 108,108,  0, 1,0xF4, 40000,     0 }, // 109: GM109; HMIGM109; Bagpipe; am109.in

    // Amplitude begins at  533.2, peaks 1795.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 109,109,  0, 1,0xF4, 40000,   106 }, // 110: GM110; HMIGM110; Fiddle; am110.in

    // Amplitude begins at   66.0, peaks 1441.9 at 16.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 110,110,  0, 1,0x04, 40000,     6 }, // 111: GM111; HMIGM111; Shanai; am111.in

    // Amplitude begins at 1669.3, peaks 1691.7 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 111,111,  0, 1,0x04,  1226,  1226 }, // 112: GM112; HMIGM112; Tinkle Bell; am112.in

    // Amplitude begins at 1905.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 112,112,  0, 1,0x04,   146,   146 }, // 113: GM113; HMIGM113; Agogo Bells; am113.in

    // Amplitude begins at 1008.1, peaks 3001.4 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 113,113,  0, 1,0x04,   226,   226 }, // 114: GM114; HMIGM114; Steel Drums; am114.in

    // Amplitude begins at  894.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 114,114,  0, 1,0x04,    26,    26 }, // 115: GM115; HMIGM115; Woodblock; am115.in

    // Amplitude begins at 1571.0, peaks 1764.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,115,  0, 1,0x04,   146,   146 }, // 116: GM116; HMIGM116; Taiko Drum; am116.in

    // Amplitude begins at 1088.6, peaks 1805.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 116,116,  0, 1,0x04,    40,    40 }, // 117: GM117; HMIGM117; Melodic Tom; am117.in

    // Amplitude begins at  781.7, peaks  845.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 117,117,  0, 1,0x62,   386,   386 }, // 118: GM118; HMIGM118; Synth Drum; am118.in

    // Amplitude begins at    0.0, peaks  452.7 at 2.2s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 118,118,  0, 1,0xF4,  2333,  2333 }, // 119: GM119; HMIGM119; Reverse Cymbal; am119.in

    // Amplitude begins at    0.0, peaks  363.9 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 119,119,  0, 1,0x04,   313,   313 }, // 120: GM120; HMIGM120; rickM101; Fretnos.; Guitar FretNoise; am120.in

    // Amplitude begins at    0.0, peaks  472.1 at 0.3s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 120,120,  0, 1,0x04,   586,   586 }, // 121: GM121; HMIGM121; Breath Noise; am121.in

    // Amplitude begins at    0.0, peaks  449.3 at 2.3s,
    // fades to 20% at 4.4s, keyoff fades to 20% in 4.4s.
    { 121,121,  0, 1,0x62,  4380,  4380 }, // 122: GM122; HMIGM122; Seashore; am122.in

    // Amplitude begins at    0.6, peaks 2634.5 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 122,122,  0, 1,0x0C,   320,   320 }, // 123: GM123; HMIGM123; Bird Tweet; am123.in

    // Amplitude begins at 1196.7,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 123,123,  0, 1,0x04,   186,   186 }, // 124: GM124; HMIGM124; Telephone; am124.in

    // Amplitude begins at    0.0, peaks  389.6 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 124,124,  0, 1,0x62,   146,   146 }, // 125: GM125; HMIGM125; Helicopter; am125.in

    // Amplitude begins at    0.0, peaks  459.9 at 2.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 125,125,  0, 1,0x04, 40000,   113 }, // 126: GM126; HMIGM126; Applause/Noise; am126.in

    // Amplitude begins at  361.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 126,126,  0, 1,0x04,   160,   160 }, // 127: GM127; HMIGM127; Gunshot; am127.in

    // Amplitude begins at 1345.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 35, 1,0x04,    53,    53 }, // 128: GP35; GP36; Ac Bass Drum; Bass Drum 1

    // Amplitude begins at 2509.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 128,128, 52, 1,0x04,    20,    20 }, // 129: GP37; Side Stick

    // Amplitude begins at  615.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,129, 48, 1,0x04,    80,    80 }, // 130: GP38; Acoustic Snare

    // Amplitude begins at 2088.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 130,130, 58, 1,0x04,    40,    40 }, // 131: GP39; Hand Clap

    // Amplitude begins at  606.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,129, 60, 1,0x04,    80,    80 }, // 132: GP40; Electric Snare

    // Amplitude begins at 1552.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 47, 1,0x04,   106,   106 }, // 133: GP41; hamP1; Low Floor Tom; aps041i

    // Amplitude begins at  256.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 43, 1,0x04,    26,    26 }, // 134: GP42; Closed High Hat

    // Amplitude begins at 1387.8,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 49, 1,0x04,   153,   153 }, // 135: GP43; hamP2; High Floor Tom; aps041i

    // Amplitude begins at   43.8, peaks  493.1 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 133,133, 43, 1,0x04,    26,    26 }, // 136: GP44; Pedal High Hat

    // Amplitude begins at 1571.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 51, 1,0x04,   153,   153 }, // 137: GP45; hamP3; Low Tom; aps041i

    // Amplitude begins at  319.5,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 134,134, 43, 1,0x04,   253,   253 }, // 138: GP46; Open High Hat

    // Amplitude begins at 1793.9, peaks 1833.4 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 54, 1,0x04,   113,   113 }, // 139: GP47; hamP4; Low-Mid Tom; aps041i

    // Amplitude begins at 1590.0, peaks 1898.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 57, 1,0x04,   140,   140 }, // 140: GP48; hamP5; High-Mid Tom; aps041i

    // Amplitude begins at  380.1, peaks  387.8 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 135,135, 72, 1,0x62,   573,   573 }, // 141: GP49; Crash Cymbal 1

    // Amplitude begins at 1690.3, peaks 1753.1 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 60, 1,0x04,   160,   160 }, // 142: GP50; hamP6; High Tom; aps041i

    // Amplitude begins at  473.8,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 136,136, 76, 1,0x04,   506,   506 }, // 143: GP51; Ride Cymbal 1

    // Amplitude begins at  648.1, peaks  699.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 137,137, 84, 1,0x04,   406,   406 }, // 144: GP52; hamP19; Chinese Cymbal; aps052i

    // Amplitude begins at  931.9,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 138,138, 36, 1,0x04,   386,   386 }, // 145: GP53; Ride Bell

    // Amplitude begins at 1074.7, peaks 1460.5 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 139,139, 65, 1,0x04,   553,   553 }, // 146: GP54; Tambourine

    // Amplitude begins at  367.4, peaks  378.6 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 140,140, 84, 1,0x04,   420,   420 }, // 147: GP55; Splash Cymbal

    // Amplitude begins at 1444.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 141,141, 83, 1,0x04,    53,    53 }, // 148: GP56; Cow Bell

    // Amplitude begins at  388.5,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 135,135, 84, 1,0x04,   400,   400 }, // 149: GP57; Crash Cymbal 2

    // Amplitude begins at  691.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 142,142, 24, 1,0x04,    13,    13 }, // 150: GP58; Vibraslap

    // Amplitude begins at  468.1,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 136,136, 77, 1,0x04,   513,   513 }, // 151: GP59; Ride Cymbal 2

    // Amplitude begins at 1809.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 143,143, 60, 1,0x04,    40,    40 }, // 152: GP60; High Bongo

    // Amplitude begins at 1360.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 144,144, 65, 1,0x04,    40,    40 }, // 153: GP61; Low Bongo

    // Amplitude begins at 2805.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 145,145, 59, 1,0x04,    13,    13 }, // 154: GP62; Mute High Conga

    // Amplitude begins at 1551.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 146,146, 51, 1,0x04,    40,    40 }, // 155: GP63; Open High Conga

    // Amplitude begins at 2245.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 147,147, 45, 1,0x04,    33,    33 }, // 156: GP64; Low Conga

    // Amplitude begins at  839.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 148,148, 71, 1,0x62,   133,   133 }, // 157: GP65; hamP8; rickP98; High Timbale; timbale; timbale.

    // Amplitude begins at  888.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 149,149, 60, 1,0x04,   140,   140 }, // 158: GP66; Low Timbale

    // Amplitude begins at  780.2,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 150,150, 58, 1,0x04,   153,   153 }, // 159: GP67; High Agogo

    // Amplitude begins at 1327.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 151,151, 53, 1,0x04,   100,   100 }, // 160: GP68; Low Agogo

    // Amplitude begins at    0.9, peaks  358.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 152,152, 64, 1,0x04,    86,    86 }, // 161: GP69; Cabasa

    // Amplitude begins at  304.0, peaks  315.3 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 153,153, 71, 1,0x04,    13,    13 }, // 162: GP70; Maracas

    // Amplitude begins at   37.5, peaks  789.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 154,154, 61, 1,0x04,   326,   326 }, // 163: GP71; Short Whistle

    // Amplitude begins at   34.3, peaks  858.7 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 155,155, 61, 1,0x04,   606,   606 }, // 164: GP72; Long Whistle

    // Amplitude begins at    1.8, peaks  475.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 156,156, 44, 1,0x04,    80,    80 }, // 165: GP73; rickP96; Short Guiro; guiros.i

    // Amplitude begins at    0.0, peaks  502.1 at 0.2s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 157,157, 40, 1,0x04,   300,   300 }, // 166: GP74; Long Guiro

    // Amplitude begins at 2076.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 158,158, 69, 1,0x04,    20,    20 }, // 167: GP75; Claves

    // Amplitude begins at 2255.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 159,159, 68, 1,0x04,    20,    20 }, // 168: GP76; High Wood Block

    // Amplitude begins at 2214.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 160,160, 63, 1,0x04,    33,    33 }, // 169: GP77; Low Wood Block

    // Amplitude begins at   15.6, peaks 2724.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 161,161, 74, 1,0x04,    93,    93 }, // 170: GP78; Mute Cuica

    // Amplitude begins at 1240.9, peaks 2880.7 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 162,162, 60, 1,0x04,   346,   346 }, // 171: GP79; Open Cuica

    // Amplitude begins at 1090.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 163,163, 80, 1,0x04,    60,    60 }, // 172: GP80; Mute Triangle

    // Amplitude begins at 1193.8,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 164,164, 64, 1,0x04,   880,   880 }, // 173: GP81; Open Triangle

    // Amplitude begins at    3.9, peaks  349.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 165,165, 72, 1,0x04,    40,    40 }, // 174: GP82; hamP7; aps082i

    // Amplitude begins at    0.0, peaks  937.8 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 166,166, 73, 1,0x04,   293,   293 }, // 175: GP83

    // Amplitude begins at  904.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 167,167, 70, 1,0x04,   180,   180 }, // 176: GP84

    // Amplitude begins at 1135.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 168,168, 68, 1,0x04,    20,    20 }, // 177: GP85

    // Amplitude begins at 2185.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 169,169, 48, 1,0x04,    26,    26 }, // 178: GP86

    // Amplitude begins at 1783.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 53, 1,0x04,   120,   120 }, // 179: GP87

    // Amplitude begins at   56.3, peaks 1484.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 170,170,  0, 1,0x62, 40000,    13 }, // 180: HMIGM0; HMIGM29; am029.in

    // Amplitude begins at 3912.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 0.0s.
    { 171,171,  0, 1,0x04,  1326,     6 }, // 181: HMIGM1; am001.in

    // Amplitude begins at 2850.7, peaks 4216.6 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 172,172,  0, 1,0x62,  1293,  1293 }, // 182: HMIGM2; am002.in

    // Amplitude begins at 1712.7, peaks 2047.5 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 173,173,  0, 1,0x62, 40000,     6 }, // 183: HMIGM3; am003.in

    // Amplitude begins at 4461.0, peaks 6341.0 at 0.0s,
    // fades to 20% at 3.2s, keyoff fades to 20% in 0.0s.
    { 174,174,  0, 1,0x62,  3200,     6 }, // 184: HMIGM4; am004.in

    // Amplitude begins at 4781.0, peaks 6329.2 at 0.0s,
    // fades to 20% at 3.2s, keyoff fades to 20% in 0.0s.
    { 175,175,  0, 1,0x62,  3200,     6 }, // 185: HMIGM5; am005.in

    // Amplitude begins at 1162.2, peaks 1404.5 at 0.0s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    { 176,176,  0, 1,0x3D,  2173,  2173 }, // 186: HMIGM6; am006.in

    // Amplitude begins at 1144.6, peaks 1235.5 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 177,177,  0, 1,0x62,  2313,  2313 }, // 187: HMIGM7; am007.in

    // Amplitude begins at 2803.9, peaks 2829.0 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 178,178,  0, 1,0x62,   960,   960 }, // 188: HMIGM8; am008.in

    // Amplitude begins at 2999.3, peaks 3227.0 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 179,179,  0, 1,0x62,  1380,  1380 }, // 189: HMIGM9; am009.in

    // Amplitude begins at 2073.6, peaks 3450.4 at 0.1s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 180,180,  0, 1,0x04,   453,   453 }, // 190: HMIGM10; am010.in

    // Amplitude begins at 2976.7, peaks 3033.0 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 181,181,  0, 1,0x04,  1746,  1746 }, // 191: HMIGM11; am011.in

    // Amplitude begins at 3343.0, peaks 3632.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 182,182,  0, 1,0x04,   206,   206 }, // 192: HMIGM12; am012.in

    // Amplitude begins at 2959.7, peaks 3202.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 183,183,  0, 1,0x04,   100,   100 }, // 193: HMIGM13; am013.in

    // Amplitude begins at 2057.2, peaks 2301.4 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 184,184,  0, 1,0x00,  1306,  1306 }, // 194: HMIGM14; am014.in

    // Amplitude begins at 1673.4, peaks 2155.0 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 185,185,  0, 1,0x04,   460,   460 }, // 195: HMIGM15; am015.in

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 186,186,  0, 1,0x04,  1266,  1266 }, // 196: HMIGM27; am027.in

    // Amplitude begins at 1957.2, peaks 3738.3 at 0.0s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 187,187,  0, 1,0x3D,  2886,  2886 }, // 197: HMIGM37; am037.in

    // Amplitude begins at    7.2, peaks 3168.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 188,188,  0, 1,0x04, 40000,    20 }, // 198: HMIGM62; am062.in

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 189,189, 60, 1,0x3E,     0,     0 }, // 199: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in

    // Amplitude begins at    4.1, peaks  788.6 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 73, 1,0x06,   473,   473 }, // 200: HMIGP27; Wierd1.i

    // Amplitude begins at    4.2, peaks  817.8 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 74, 1,0x06,   473,   473 }, // 201: HMIGP28; Wierd1.i

    // Amplitude begins at    4.9, peaks  841.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 80, 1,0x06,   473,   473 }, // 202: HMIGP29; Wierd1.i

    // Amplitude begins at    5.3, peaks  778.3 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 84, 1,0x06,   473,   473 }, // 203: HMIGP30; Wierd1.i

    // Amplitude begins at    9.5, peaks  753.0 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 190,190, 92, 1,0x06,   406,   406 }, // 204: HMIGP31; Wierd1.i

    // Amplitude begins at   91.3, peaks  917.0 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 191,191, 81, 1,0x3E,   280,   280 }, // 205: HMIGP32; Wierd2.i

    // Amplitude begins at   89.5, peaks  919.6 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 191,191, 83, 1,0x3E,   280,   280 }, // 206: HMIGP33; Wierd2.i

    // Amplitude begins at  155.5, peaks  913.4 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 191,191, 95, 1,0x3E,   240,   240 }, // 207: HMIGP34; Wierd2.i

    // Amplitude begins at  425.5, peaks  768.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 192,192, 83, 1,0x3E,   273,   273 }, // 208: HMIGP35; Wierd3.i

    // Amplitude begins at 2304.1, peaks 2323.3 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 193,193, 35, 1,0x06,    33,    33 }, // 209: HMIGP36; Kick.ins

    // Amplitude begins at 2056.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 194,194, 36, 1,0x06,    20,    20 }, // 210: HMIGP37; HMIGP86; RimShot.; rimshot.

    // Amplitude begins at  752.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 195,195, 60, 1,0x06,    40,    40 }, // 211: HMIGP38; Snare.in

    // Amplitude begins at  585.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 196,196, 59, 1,0x06,    20,    20 }, // 212: HMIGP39; Clap.ins

    // Amplitude begins at  768.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 195,195, 44, 1,0x06,    40,    40 }, // 213: HMIGP40; Snare.in

    // Amplitude begins at  646.2, peaks  724.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 41, 0,0x06,   160,   160 }, // 214: HMIGP41; Toms.ins

    // Amplitude begins at 2548.3, peaks 2665.5 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 198,198, 97, 1,0x3E,   200,   200 }, // 215: HMIGP42; HMIGP44; clshat97

    // Amplitude begins at  700.8, peaks  776.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 197,197, 44, 0,0x06,   120,   120 }, // 216: HMIGP43; Toms.ins

    // Amplitude begins at  667.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 48, 0,0x06,   160,   160 }, // 217: HMIGP45; Toms.ins

    // Amplitude begins at   11.0, peaks  766.2 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 199,199, 96, 1,0x06,   253,   253 }, // 218: HMIGP46; Opnhat96

    // Amplitude begins at  646.7, peaks  707.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 51, 0,0x06,   166,   166 }, // 219: HMIGP47; Toms.ins

    // Amplitude begins at  753.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 54, 0,0x06,   153,   153 }, // 220: HMIGP48; Toms.ins

    // Amplitude begins at  473.8,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 200,200, 40, 1,0x06,   380,   380 }, // 221: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym

    // Amplitude begins at  775.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 197,197, 57, 0,0x06,   106,   106 }, // 222: HMIGP50; Toms.ins

    // Amplitude begins at  360.8,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 201,201, 58, 1,0x06,   206,   206 }, // 223: HMIGP51; HMIGP53; Ridecym.

    // Amplitude begins at 2347.0, peaks 2714.6 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 202,202, 97, 1,0x06,   186,   186 }, // 224: HMIGP54; Tamb.ins

    // Amplitude begins at 2920.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 203,203, 50, 1,0x06,    73,    73 }, // 225: HMIGP56; Cowbell.

    // Amplitude begins at  556.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 204,204, 28, 1,0x06,    26,    26 }, // 226: HMIGP58; vibrasla

    // Amplitude begins at  365.3,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 201,201, 60, 1,0x06,   186,   186 }, // 227: HMIGP59; ridecym.

    // Amplitude begins at 2172.1, peaks 2678.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 205,205, 53, 1,0x06,    46,    46 }, // 228: HMIGP60; mutecong

    // Amplitude begins at 2346.3, peaks 2455.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 206,206, 46, 1,0x06,    53,    53 }, // 229: HMIGP61; conga.in

    // Amplitude begins at 2247.7, peaks 2709.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 205,205, 57, 1,0x06,    46,    46 }, // 230: HMIGP62; mutecong

    // Amplitude begins at 1529.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 207,207, 42, 1,0x06,   133,   133 }, // 231: HMIGP63; loconga.

    // Amplitude begins at 1495.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 207,207, 37, 1,0x06,   133,   133 }, // 232: HMIGP64; loconga.

    // Amplitude begins at  486.6, peaks  530.9 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 208,208, 41, 1,0x06,   186,   186 }, // 233: HMIGP65; timbale.

    // Amplitude begins at  508.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 208,208, 37, 1,0x06,   193,   193 }, // 234: HMIGP66; timbale.

    // Amplitude begins at  789.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 209,209, 77, 1,0x06,    40,    40 }, // 235: HMIGP67; agogo.in

    // Amplitude begins at  735.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 209,209, 72, 1,0x06,    46,    46 }, // 236: HMIGP68; agogo.in

    // Amplitude begins at    5.1, peaks  818.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 210,210, 70, 1,0x06,    66,    66 }, // 237: HMIGP69; HMIGP82; shaker.i

    // Amplitude begins at    4.4, peaks  758.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 210,210, 90, 1,0x06,    66,    66 }, // 238: HMIGP70; shaker.i

    // Amplitude begins at  474.0, peaks 1257.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 211,211, 39, 1,0x3E,   180,   180 }, // 239: HMIGP71; hiwhist.

    // Amplitude begins at  468.8, peaks 1217.2 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 212,212, 36, 1,0x3E,   513,   513 }, // 240: HMIGP72; lowhist.

    // Amplitude begins at   28.5, peaks  520.5 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 213,213, 46, 1,0x06,    46,    46 }, // 241: HMIGP73; higuiro.

    // Amplitude begins at 1096.9, peaks 2623.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 214,214, 48, 1,0x06,   253,   253 }, // 242: HMIGP74; loguiro.

    // Amplitude begins at 2832.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 215,215, 85, 1,0x4E,    13,    13 }, // 243: HMIGP75; clavecb.

    // Amplitude begins at 2608.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 216,216, 66, 1,0x06,    60,    60 }, // 244: HMIGP76; woodblok

    // Amplitude begins at 2613.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 216,216, 61, 1,0x06,    60,    60 }, // 245: HMIGP77; woodblok

    // Amplitude begins at    2.6, peaks 2733.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 217,217, 41, 1,0x3E,   120,   120 }, // 246: HMIGP78; hicuica.

    // Amplitude begins at    2.4, peaks 2900.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 218,218, 41, 1,0x3E, 40000,    20 }, // 247: HMIGP79; locuica.

    // Amplitude begins at 1572.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 219,219, 81, 1,0x4E,    66,    66 }, // 248: HMIGP80; mutringl

    // Amplitude begins at 1668.0,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 220,220, 81, 1,0x26,   260,   260 }, // 249: HMIGP81; triangle

    // Amplitude begins at 1693.0,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 220,220, 76, 1,0x26,   260,   260 }, // 250: HMIGP83; triangle

    // Amplitude begins at 1677.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 220,220,103, 1,0x26,   220,   220 }, // 251: HMIGP84; triangle

    // Amplitude begins at 1635.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 194,194, 60, 1,0x06,    13,    13 }, // 252: HMIGP85; rimShot.

    // Amplitude begins at  976.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 221,221, 53, 1,0x06,   120,   120 }, // 253: HMIGP87; taiko.in

    // Amplitude begins at   64.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 222,222,  0, 0,0x00,     6,     6 }, // 254: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.

    // Amplitude begins at  947.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 223,223,  0, 0,0x00,    40,    40 }, // 255: hamM1; intM1; rickM1; DBlock.i; DBlocki

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 224,224,  0, 0,0x00,  1213,  1213 }, // 256: hamM2; intM2; rickM2; GClean.i; GCleani

    // Amplitude begins at 3418.1,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  28, 28,  0, 0,0x00, 40000,     0 }, // 257: hamM3; intM3; rickM3; BPerc.in; BPercin

    // Amplitude begins at 1555.8, peaks 1691.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 225,225,  0, 0,0x00,   126,   126 }, // 258: hamM4; intM4; rickM4; DToms.in; DTomsin

    // Amplitude begins at 1424.2, peaks 2686.1 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  31, 31,  0, 0,0x00, 40000,     0 }, // 259: hamM5; intM5; rickM5; GFeedbck

    // Amplitude begins at  396.9, peaks 1480.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  30, 30,  0, 0,0x00, 40000,    13 }, // 260: hamM6; intM6; rickM6; GDist.in; GDistin

    // Amplitude begins at  733.0, peaks  761.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 226,226,  0, 0,0x00, 40000,    20 }, // 261: hamM7; intM7; rickM7; GOverD.i; GOverDi

    // Amplitude begins at 1258.5, peaks 1452.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 227,227,  0, 0,0x00, 40000,     6 }, // 262: hamM8; intM8; rickM8; GMetal.i; GMetali

    // Amplitude begins at 2589.3, peaks 2634.3 at 0.0s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 228,228,  0, 0,0x00,   706,   706 }, // 263: hamM9; intM9; rickM9; BPick.in; BPickin

    // Amplitude begins at 1402.0, peaks 2135.8 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 229,229,  0, 0,0x00,   446,   446 }, // 264: hamM10; intM10; rickM10; BSlap.in; BSlapin

    // Amplitude begins at 3040.5, peaks 3072.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 230,230,  0, 0,0x00,   440,   440 }, // 265: hamM11; intM11; rickM11; BSynth1; BSynth1.

    // Amplitude begins at 2530.9, peaks 2840.4 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 231,231,  0, 0,0x00,   626,   626 }, // 266: hamM12; intM12; rickM12; BSynth2; BSynth2.

    // Amplitude begins at 3787.6,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  38, 38,  0, 0,0x00,   540,   540 }, // 267: hamM13; intM13; rickM13; BSynth3; BSynth3.

    // Amplitude begins at 2061.3, peaks 2787.1 at 0.1s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {  46, 46,  0, 0,0x00,   906,   906 }, // 268: hamM14; intM14; rickM14; BSynth4; BSynth4.

    // Amplitude begins at   39.5, peaks 1089.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 232,232,  0, 0,0x00, 40000,    26 }, // 269: hamM15; intM15; rickM15; PSoft.in; PSoftin

    // Amplitude begins at 2183.6, peaks 2909.4 at 29.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  79, 79,  0, 0,0x00, 40000,   453 }, // 270: hamM16; intM16; rickM16; LSquare; LSquare.

    // Amplitude begins at    0.5, peaks 2501.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  84, 84,  0, 0,0x00, 40000,    60 }, // 271: hamM17; intM17; rickM17; PFlutes; PFlutes.

    // Amplitude begins at    0.0, peaks 1440.0 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 233,233,  0, 0,0x00, 40000,    26 }, // 272: hamM18; intM18; rickM18; PRonStr1

    // Amplitude begins at    0.0, peaks 1417.7 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 234,234,  0, 0,0x00, 40000,    26 }, // 273: hamM19; intM19; rickM19; PRonStr2

    // Amplitude begins at 2323.6, peaks 4179.7 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    {  49, 49,  0, 0,0x00, 40000,   773 }, // 274: hamM20; intM20; rickM20; PMellow; PMellow.

    // Amplitude begins at  641.8, peaks 4073.9 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  89, 89,  0, 0,0x00, 40000,   106 }, // 275: hamM21; intM21; rickM21; LTriang; LTriang.

    // Amplitude begins at    0.0, peaks 1692.3 at 0.6s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 2.8s.
    {  92, 92,  0, 0,0x00,  2773,  2773 }, // 276: hamM22; intM22; rickM22; PSlow.in; PSlowin

    // Amplitude begins at    0.0, peaks 3007.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  93, 93,  0, 0,0x00, 40000,   193 }, // 277: hamM23; intM23; rickM23; PSweep.i; PSweepi

    // Amplitude begins at  933.2,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 105,105,  0, 0,0x00,  1160,  1160 }, // 278: hamM24; intM24; rickM24; LDist.in; LDistin

    // Amplitude begins at   73.3, peaks 1758.3 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 235,235,  0, 0,0x00,  1940,  1940 }, // 279: hamM25; intM25; rickM25; LTrap.in; LTrapin

    // Amplitude begins at 2192.9, peaks 3442.1 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 236,236,  0, 0,0x00,  1886,  1886 }, // 280: hamM26; intM26; rickM26; LSaw.ins; LSawins

    // Amplitude begins at 1119.0, peaks 1254.3 at 0.1s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 237,237,  0, 0,0x00,  1733,  1733 }, // 281: hamM27; intM27; rickM27; PolySyn; PolySyn.

    // Amplitude begins at 2355.1, peaks 3374.2 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 238,238,  0, 0,0x00, 40000,     0 }, // 282: hamM28; intM28; rickM28; Pobo.ins; Poboins

    // Amplitude begins at  679.7, peaks 2757.2 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 239,239,  0, 0,0x00, 40000,    20 }, // 283: hamM29; intM29; rickM29; PSweep2; PSweep2.

    // Amplitude begins at 1971.4, peaks 2094.8 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 240,240,  0, 0,0x00,  1386,  1386 }, // 284: hamM30; intM30; rickM30; LBright; LBright.

    // Amplitude begins at    0.0, peaks  857.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 241,241,  0, 0,0x00, 40000,    73 }, // 285: hamM31; intM31; rickM31; SynStrin

    // Amplitude begins at  901.8, peaks  974.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 242,242,  0, 0,0x00, 40000,   106 }, // 286: hamM32; intM32; rickM32; SynStr2; SynStr2.

    // Amplitude begins at 2098.8,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 243,243,  0, 0,0x00,   560,   560 }, // 287: hamM33; intM33; rickM33; low_blub

    // Amplitude begins at  978.8, peaks 2443.9 at 1.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.4s.
    { 244,244,  0, 0,0x00, 40000,  2446 }, // 288: hamM34; intM34; rickM34; DInsect; DInsect.

    // Amplitude begins at  426.5, peaks  829.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 245,245,  0, 0,0x00,   140,   140 }, // 289: hamM35; intM35; rickM35; hardshak

    // Amplitude begins at    0.0, peaks  363.9 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 119,119,  0, 0,0x00,   313,   313 }, // 290: hamM36; intM36; rickM36; DNoise1; DNoise1.

    // Amplitude begins at  982.9, peaks 2185.9 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 246,246,  0, 0,0x00,   260,   260 }, // 291: hamM37; intM37; rickM37; WUMP.ins; WUMPins

    // Amplitude begins at  557.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 247,247,  0, 0,0x00,   160,   160 }, // 292: hamM38; intM38; rickM38; DSnare.i; DSnarei

    // Amplitude begins at 2110.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 248,248,  0, 0,0x00,    20,    20 }, // 293: hamM39; intM39; rickM39; DTimp.in; DTimpin

    // Amplitude begins at    0.0, peaks  830.0 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 249,249,  0, 0,0x00, 40000,     0 }, // 294: hamM40; intM40; rickM40; DRevCym; DRevCym.

    // Amplitude begins at    0.0, peaks 1668.5 at 0.2s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 250,250,  0, 0,0x00,   780,   780 }, // 295: hamM41; intM41; rickM41; Dorky.in; Dorkyin

    // Amplitude begins at    4.1, peaks 2699.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 251,251,  0, 0,0x00,    73,    73 }, // 296: hamM42; intM42; rickM42; DFlab.in; DFlabin

    // Amplitude begins at 1995.3, peaks 3400.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 252,252,  0, 0,0x00, 40000,    60 }, // 297: hamM43; intM43; rickM43; DInsect2

    // Amplitude begins at    0.0, peaks  792.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 253,253,  0, 0,0x00, 40000,   406 }, // 298: hamM44; intM44; rickM44; DChopper

    // Amplitude begins at  787.3, peaks  848.0 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 254,254,  0, 0,0x00,   306,   306 }, // 299: hamM45; intM45; rickM45; DShot.in; DShotin

    // Amplitude begins at 2940.9, peaks 3003.1 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 255,255,  0, 0,0x00,  1780,  1780 }, // 300: hamM46; intM46; rickM46; KickAss; KickAss.

    // Amplitude begins at 1679.1, peaks 1782.4 at 0.1s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 256,256,  0, 0,0x00,  1693,  1693 }, // 301: hamM47; intM47; rickM47; RVisCool

    // Amplitude begins at 1214.6, peaks 1237.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 257,257,  0, 0,0x00,    80,    80 }, // 302: hamM48; intM48; rickM48; DSpring; DSpring.

    // Amplitude begins at    0.0, peaks 3049.5 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 258,258,  0, 0,0x00, 40000,    20 }, // 303: intM49; Chorar22

    // Amplitude begins at 1401.3, peaks 2203.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 259,259, 36, 0,0x00,    73,    73 }, // 304: IntP27; hamP27; rickP27; timpani; timpani.

    // Amplitude begins at 1410.5, peaks 1600.6 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 260,260, 50, 0,0x00,   580,   580 }, // 305: IntP28; hamP28; rickP28; timpanib

    // Amplitude begins at 1845.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 261,261, 37, 0,0x00,    33,    33 }, // 306: IntP29; rickP29; APS043.i; APS043i

    // Amplitude begins at 1267.8, peaks 1286.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 262,262, 39, 0,0x00,   166,   166 }, // 307: IntP30; rickP30; mgun3.in; mgun3in

    // Amplitude begins at 1247.2, peaks 1331.5 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 263,263, 39, 0,0x00,    53,    53 }, // 308: IntP31; hamP31; rickP31; kick4r.i; kick4ri

    // Amplitude begins at 1432.2, peaks 1442.6 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 264,264, 86, 0,0x00,  1826,  1826 }, // 309: IntP32; hamP32; rickP32; timb1r.i; timb1ri

    // Amplitude begins at  996.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 265,265, 43, 0,0x00,    33,    33 }, // 310: IntP33; hamP33; rickP33; timb2r.i; timb2ri

    // Amplitude begins at 1331.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 127,127, 24, 0,0x00,     6,     6 }, // 311: IntP34; rickP34; apo035.i; apo035i

    // Amplitude begins at 1324.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 29, 0,0x00,    53,    53 }, // 312: IntP35; rickP35; apo035.i; apo035i

    // Amplitude begins at 1604.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 266,266, 50, 0,0x00,   193,   193 }, // 313: IntP36; hamP36; rickP36; hartbeat

    // Amplitude begins at 1443.1, peaks 1537.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 267,267, 30, 0,0x00,   100,   100 }, // 314: IntP37; hamP37; rickP37; tom1r.in; tom1rin

    // Amplitude begins at 1351.4, peaks 1552.4 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 267,267, 33, 0,0x00,   320,   320 }, // 315: IntP38; hamP38; rickP38; tom1r.in; tom1rin

    // Amplitude begins at 1362.2, peaks 1455.5 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 267,267, 38, 0,0x00,  1646,  1646 }, // 316: IntP39; hamP39; rickP39; tom1r.in; tom1rin

    // Amplitude begins at 1292.1, peaks 1445.6 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 267,267, 42, 0,0x00,  1706,  1706 }, // 317: IntP40; hamP40; rickP40; tom1r.in; tom1rin

    // Amplitude begins at 2139.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 268,268, 24, 0,0x00,    46,    46 }, // 318: IntP41; rickP41; tom2.ins; tom2ins

    // Amplitude begins at 2100.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 27, 0,0x00,    53,    53 }, // 319: IntP42; rickP42; tom2.ins; tom2ins

    // Amplitude begins at 2145.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 29, 0,0x00,    53,    53 }, // 320: IntP43; rickP43; tom2.ins; tom2ins

    // Amplitude begins at 2252.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 32, 0,0x00,    53,    53 }, // 321: IntP44; rickP44; tom2.ins; tom2ins

    // Amplitude begins at 1018.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 269,269, 32, 0,0x00,    13,    13 }, // 322: IntP45; rickP45; tom.ins; tomins

    // Amplitude begins at  837.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 270,270, 53, 0,0x00,    80,    80 }, // 323: IntP46; hamP46; rickP46; conga.in; congain

    // Amplitude begins at  763.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 270,270, 57, 0,0x00,    80,    80 }, // 324: IntP47; hamP47; rickP47; conga.in; congain

    // Amplitude begins at  640.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 271,271, 60, 0,0x00,    80,    80 }, // 325: IntP48; hamP48; rickP48; snare01r

    // Amplitude begins at  969.1,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 272,272, 55, 0,0x00,   153,   153 }, // 326: IntP49; hamP49; rickP49; slap.ins; slapins

    // Amplitude begins at  801.4, peaks  829.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 254,254, 85, 0,0x00,   260,   260 }, // 327: IntP50; hamP50; rickP50; shot.ins; shotins

    // Amplitude begins at  882.9, peaks  890.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 273,273, 90, 0,0x00,   473,   473 }, // 328: IntP51; rickP51; snrsust; snrsust.

    // Amplitude begins at  766.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 274,274, 84, 0,0x00,    73,    73 }, // 329: IntP52; rickP52; snare.in; snarein

    // Amplitude begins at  791.4,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 275,275, 48, 0,0x00,   166,   166 }, // 330: IntP53; hamP53; rickP53; synsnar; synsnar.

    // Amplitude begins at  715.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 276,276, 48, 0,0x00,    40,    40 }, // 331: IntP54; rickP54; synsnr1; synsnr1.

    // Amplitude begins at  198.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 72, 0,0x00,    13,    13 }, // 332: IntP55; rickP55; aps042.i; aps042i

    // Amplitude begins at 1290.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 277,277, 72, 0,0x00,    13,    13 }, // 333: IntP56; hamP56; rickP56; rimshotb

    // Amplitude begins at  931.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 278,278, 72, 0,0x00,    13,    13 }, // 334: IntP57; rickP57; rimshot; rimshot.

    // Amplitude begins at  413.6,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 279,279, 63, 0,0x00,   580,   580 }, // 335: IntP58; hamP58; rickP58; crash.in; crashin

    // Amplitude begins at  407.6,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 279,279, 65, 0,0x00,   586,   586 }, // 336: IntP59; hamP59; rickP59; crash.in; crashin

    // Amplitude begins at  377.2, peaks  377.3 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 280,280, 79, 0,0x00,   506,   506 }, // 337: IntP60; rickP60; cymbal.i; cymbali

    // Amplitude begins at  102.7, peaks  423.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 281,281, 38, 0,0x00,   113,   113 }, // 338: IntP61; hamP61; rickP61; cymbals; cymbals.

    // Amplitude begins at  507.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 282,282, 94, 0,0x00,   100,   100 }, // 339: IntP62; hamP62; rickP62; hammer5r

    // Amplitude begins at  640.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 283,283, 87, 0,0x00,   120,   120 }, // 340: IntP63; hamP63; rickP63; hammer3; hammer3.

    // Amplitude begins at  611.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 283,283, 94, 0,0x00,   106,   106 }, // 341: IntP64; hamP64; rickP64; hammer3; hammer3.

    // Amplitude begins at  861.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 284,284, 80, 0,0x00,    60,    60 }, // 342: IntP65; hamP65; rickP65; ride2.in; ride2in

    // Amplitude begins at  753.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 285,285, 47, 0,0x00,   140,   140 }, // 343: IntP66; hamP66; rickP66; hammer1; hammer1.

    // Amplitude begins at  747.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 286,286, 61, 0,0x00,    80,    80 }, // 344: IntP67; rickP67; tambour; tambour.

    // Amplitude begins at  691.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 286,286, 68, 0,0x00,    73,    73 }, // 345: IntP68; rickP68; tambour; tambour.

    // Amplitude begins at  771.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 287,287, 61, 0,0x00,   160,   160 }, // 346: IntP69; hamP69; rickP69; tambou2; tambou2.

    // Amplitude begins at  716.0, peaks  730.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 287,287, 68, 0,0x00,   133,   133 }, // 347: IntP70; hamP70; rickP70; tambou2; tambou2.

    // Amplitude begins at 2362.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 268,268, 60, 0,0x00,    40,    40 }, // 348: IntP71; hamP71; rickP71; woodbloc

    // Amplitude begins at  912.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 288,288, 60, 0,0x00,    60,    60 }, // 349: IntP72; hamP72; rickP72; woodblok

    // Amplitude begins at 1238.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 289,289, 36, 0,0x00,    53,    53 }, // 350: IntP73; rickP73; claves.i; clavesi

    // Amplitude begins at 1289.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 289,289, 60, 0,0x00,    40,    40 }, // 351: IntP74; rickP74; claves.i; clavesi

    // Amplitude begins at  885.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 290,290, 60, 0,0x00,    40,    40 }, // 352: IntP75; hamP75; rickP75; claves2; claves2.

    // Amplitude begins at  931.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 291,291, 60, 0,0x00,    40,    40 }, // 353: IntP76; hamP76; rickP76; claves3; claves3.

    // Amplitude begins at 2083.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 292,292, 68, 0,0x00,    20,    20 }, // 354: IntP77; hamP77; rickP77; clave.in; clavein

    // Amplitude begins at 2344.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 293,293, 71, 0,0x00,    33,    33 }, // 355: IntP78; hamP78; rickP78; agogob4; agogob4.

    // Amplitude begins at 2487.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 293,293, 72, 0,0x00,    33,    33 }, // 356: IntP79; hamP79; rickP79; agogob4; agogob4.

    // Amplitude begins at 1824.6, peaks 1952.1 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 294,294,101, 0,0x00,  1153,  1153 }, // 357: IntP80; hamP80; rickP80; clarion; clarion.

    // Amplitude begins at 2039.7, peaks 2300.6 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 295,295, 36, 0,0x00,  1466,  1466 }, // 358: IntP81; rickP81; trainbel

    // Amplitude begins at 1466.4, peaks 1476.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.3s.
    { 296,296, 25, 0,0x00, 40000,  2280 }, // 359: IntP82; hamP82; rickP82; gong.ins; gongins

    // Amplitude begins at  495.7, peaks  730.8 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 297,297, 37, 0,0x00,   420,   420 }, // 360: IntP83; hamP83; rickP83; kalimbai

    // Amplitude begins at 2307.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 36, 0,0x00,    40,    40 }, // 361: IntP84; rickP84; xylo1.in; xylo1in

    // Amplitude begins at 2435.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 41, 0,0x00,    40,    40 }, // 362: IntP85; rickP85; xylo1.in; xylo1in

    // Amplitude begins at    0.0, peaks  445.7 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 299,299, 84, 0,0x00,    80,    80 }, // 363: IntP86; hamP86; rickP86; match.in; matchin

    // Amplitude begins at    0.0, peaks  840.6 at 0.2s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 300,300, 54, 0,0x00,  1173,  1173 }, // 364: IntP87; hamP87; rickP87; breathi; breathi.

    // Amplitude begins at    0.0, peaks  847.3 at 0.2s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 301,301, 36, 0,0x00,   186,   186 }, // 365: IntP88; rickP88; scratch; scratch.

    // Amplitude begins at    0.0, peaks  870.4 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 302,302, 60, 0,0x00, 40000,     0 }, // 366: IntP89; hamP89; rickP89; crowd.in; crowdin

    // Amplitude begins at  739.8, peaks 1007.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 303,303, 37, 0,0x00,    60,    60 }, // 367: IntP90; rickP90; taiko.in; taikoin

    // Amplitude begins at  844.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 304,304, 36, 0,0x00,    46,    46 }, // 368: IntP91; rickP91; rlog.ins; rlogins

    // Amplitude begins at 1100.9, peaks 1403.9 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 305,305, 32, 0,0x00,    40,    40 }, // 369: IntP92; hamP92; rickP92; knock.in; knockin

    // Amplitude begins at    0.0, peaks  833.9 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 249,249, 48, 0,0x00, 40000,     0 }, // 370: IntP93; drevcym

    // Amplitude begins at 1905.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 112,112,  0, 1,0x62,   146,   146 }, // 371: hamM50; agogoin

    // Amplitude begins at 2453.1, peaks 2577.6 at 0.0s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 306,306,  0, 1,0x62,  2440,  2440 }, // 372: hamM52; rickM94; Fantasy1; fantasy1

    // Amplitude begins at 1016.1, peaks 1517.4 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 307,307,  0, 1,0x62,   573,   573 }, // 373: hamM53; guitar1

    // Amplitude begins at    0.0, peaks 3007.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  93, 93,  0, 1,0x62, 40000,   193 }, // 374: hamM54; rickM96; Halopad.; halopad

    // Amplitude begins at 1040.8, peaks 1086.3 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.0s.
    { 308,308,  0, 1,0x62,   240,     6 }, // 375: hamM55; hamatmos

    // Amplitude begins at    0.4, peaks 3008.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 309,309,  0, 1,0x62, 40000,    26 }, // 376: hamM56; hamcalio

    // Amplitude begins at 2080.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 107,107,  0, 1,0x62,   153,   153 }, // 377: hamM57; kalimba

    // Amplitude begins at 1088.6, peaks 1805.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 116,116,  0, 1,0x62,    40,    40 }, // 378: hamM58; melotom

    // Amplitude begins at 1031.7, peaks 1690.4 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 310,310,  0, 1,0x62, 40000,     6 }, // 379: hamM59; moonins

    // Amplitude begins at 3418.1,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  28, 28,  0, 1,0x62, 40000,     0 }, // 380: hamM60; muteguit

    // Amplitude begins at    5.0, peaks 3460.4 at 13.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  78, 78,  0, 0,0x62, 40000,    66 }, // 381: hamM61; ocarina

    // Amplitude begins at    4.0, peaks 1614.7 at 0.0s,
    // fades to 20% at 4.3s, keyoff fades to 20% in 0.0s.
    { 311,311,  0, 1,0x62,  4260,    13 }, // 382: hamM62; Polyham3

    // Amplitude begins at    7.3, peaks 1683.7 at 0.1s,
    // fades to 20% at 4.1s, keyoff fades to 20% in 4.1s.
    { 312,312,  0, 1,0x62,  4133,  4133 }, // 383: hamM63; Polyham

    // Amplitude begins at 1500.1, peaks 1587.3 at 0.0s,
    // fades to 20% at 4.8s, keyoff fades to 20% in 4.8s.
    { 313,313,  0, 1,0x62,  4826,  4826 }, // 384: hamM64; sitar2i

    // Amplitude begins at 2183.6, peaks 2909.4 at 29.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  79, 79,  0, 1,0x62, 40000,   453 }, // 385: hamM65; squarewv

    // Amplitude begins at 2050.9, peaks 4177.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  94, 94,  0, 1,0x62, 40000,    53 }, // 386: hamM66; rickM97; Sweepad.; sweepad

    // Amplitude begins at 3787.6,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  38, 38,  0, 1,0x62,   540,   540 }, // 387: hamM67; synbass1

    // Amplitude begins at 1212.1, peaks 1233.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  33, 33,  0, 1,0x62, 40000,    13 }, // 388: hamM68; synbass2

    // Amplitude begins at 1571.0, peaks 1764.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,115,  0, 1,0x62,   146,   146 }, // 389: hamM69; Taikoin

    // Amplitude begins at 1572.2, peaks 1680.2 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 314,314,  0, 1,0x62,  1753,  1753 }, // 390: hamM70; weird1a

    // Amplitude begins at 1374.4, peaks 1624.9 at 0.0s,
    // fades to 20% at 4.2s, keyoff fades to 20% in 0.0s.
    { 315,315,  0, 1,0x62,  4180,    13 }, // 391: hamM71; Polyham4

    // Amplitude begins at 3264.2, peaks 3369.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 316,316,  0, 1,0x62, 40000,    13 }, // 392: hamM72; hamsynbs

    // Amplitude begins at 1730.5, peaks 2255.7 at 20.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 317,317,  0, 0,0x62, 40000,    66 }, // 393: hamM73; Ocasynth

    // Amplitude begins at 1728.4, peaks 3093.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 318,318,  0, 1,0x62,   126,   126 }, // 394: hamM120; hambass1

    // Amplitude begins at  647.4, peaks  662.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 319,319,  0, 1,0x62, 40000,    13 }, // 395: hamM121; hamguit1

    // Amplitude begins at 2441.6, peaks 2524.9 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 320,320,  0, 1,0x62, 40000,   560 }, // 396: hamM122; hamharm2

    // Amplitude begins at    0.3, peaks 4366.8 at 20.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.7s.
    { 321,321,  0, 1,0x62, 40000,   666 }, // 397: hamM123; hamvox1

    // Amplitude begins at    0.0, peaks 2319.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 322,322,  0, 1,0x62, 40000,   500 }, // 398: hamM124; hamgob1

    // Amplitude begins at    0.8, peaks 2668.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 323,323,  0, 1,0x62, 40000,    40 }, // 399: hamM125; hamblow1

    // Amplitude begins at  338.4, peaks  390.0 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 135,135, 49, 1,0x62,  1153,  1153 }, // 400: hamP0; crash1i

    // Amplitude begins at  825.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 324,324, 72, 1,0x62,    53,    53 }, // 401: hamP9; cowbell

    // Amplitude begins at 1824.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 325,325, 74, 1,0x62,    33,    33 }, // 402: hamP10; rickP100; conghi.i; conghii

    // Amplitude begins at 1345.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 35, 1,0x3D,    53,    53 }, // 403: hamP11; rickP14; aps035i; kick2.in

    // Amplitude begins at 1672.4, peaks 2077.7 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 326,326, 35, 1,0x62,    40,    40 }, // 404: hamP12; rickP15; hamkick; kick3.in

    // Amplitude begins at 1250.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 327,327, 41, 1,0x62,     6,     6 }, // 405: hamP13; rimshot2

    // Amplitude begins at  845.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 328,328, 38, 1,0x62,   100,   100 }, // 406: hamP14; rickP16; hamsnr1; snr1.ins

    // Amplitude begins at  771.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 329,329, 39, 1,0x62,    26,    26 }, // 407: hamP15; handclap

    // Amplitude begins at  312.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 330,330, 49, 1,0x62,    40,    40 }, // 408: hamP16; smallsnr

    // Amplitude begins at  914.5, peaks 2240.8 at 15.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 331,331, 83, 1,0x62, 40000,     0 }, // 409: hamP17; rickP95; clsdhhat

    // Amplitude begins at 1289.5, peaks 1329.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 332,332, 59, 1,0x62, 40000,  1093 }, // 410: hamP18; openhht2

    // Amplitude begins at 1845.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 261,261, 37, 1,0x62,    33,    33 }, // 411: hamP29; APS043i

    // Amplitude begins at 1267.8, peaks 1286.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 262,262, 39, 1,0x62,   166,   166 }, // 412: hamP30; mgun3in

    // Amplitude begins at 1331.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 127,127, 24, 1,0x3D,     6,     6 }, // 413: hamP34; apo035i

    // Amplitude begins at 1324.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 29, 1,0x3D,    53,    53 }, // 414: hamP35; apo035i

    // Amplitude begins at 2642.4, peaks 2809.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 333,333, 24, 0,0x00,    93,    93 }, // 415: hamP41; tom2ins

    // Amplitude begins at 2645.3, peaks 3034.8 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 333,333, 27, 0,0x00,   133,   133 }, // 416: hamP42; tom2ins

    // Amplitude begins at 2593.0, peaks 2757.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.0s.
    { 333,333, 29, 0,0x00,   126,    13 }, // 417: hamP43; tom2ins

    // Amplitude begins at 2513.8, peaks 3102.7 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.0s.
    { 333,333, 32, 0,0x00,   126,    13 }, // 418: hamP44; tom2ins

    // Amplitude begins at 1018.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 269,269, 32, 1,0x04,    13,    13 }, // 419: hamP45; tomins

    // Amplitude begins at  882.9, peaks  890.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 273,273, 90, 1,0x1A,   473,   473 }, // 420: hamP51; snrsust

    // Amplitude begins at  827.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 334,334, 84, 1,0x07,   113,   113 }, // 421: hamP52; snarein

    // Amplitude begins at  715.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 276,276, 48, 1,0x39,    40,    40 }, // 422: hamP54; synsnr1

    // Amplitude begins at  198.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 72, 1,0x62,    13,    13 }, // 423: hamP55; aps042i

    // Amplitude begins at  931.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 278,278, 72, 1,0x62,    13,    13 }, // 424: hamP57; rimshot

    // Amplitude begins at  252.7, peaks  435.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 281,281, 79, 1,0x07,    73,    73 }, // 425: hamP60; cymbali

    // Amplitude begins at   21.4, peaks  414.1 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 335,335, 61, 1,0x62,   166,   166 }, // 426: hamP67; tambour

    // Amplitude begins at   53.7, peaks  464.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 335,335, 68, 1,0x62,   140,   140 }, // 427: hamP68; tambour

    // Amplitude begins at  901.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 336,336, 36, 1,0x62,    33,    33 }, // 428: hamP73; clavesi

    // Amplitude begins at  729.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 336,336, 60, 1,0x62,    20,    20 }, // 429: hamP74; clavesi

    // Amplitude begins at 2039.7, peaks 2300.6 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 295,295, 36, 1,0x00,  1466,  1466 }, // 430: hamP81; trainbel

    // Amplitude begins at 2307.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 36, 1,0x06,    40,    40 }, // 431: hamP84; xylo1in

    // Amplitude begins at 2435.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 41, 1,0x06,    40,    40 }, // 432: hamP85; xylo1in

    // Amplitude begins at    0.0, peaks  834.2 at 0.2s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 337,337, 36, 1,0x62,   186,   186 }, // 433: hamP88; scratch

    // Amplitude begins at 1400.8, peaks 1786.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,115, 37, 1,0x62,    73,    73 }, // 434: hamP90; taikoin

    // Amplitude begins at  844.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 304,304, 36, 1,0x07,    46,    46 }, // 435: hamP91; rlogins

    // Amplitude begins at 1209.9,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 338,338,  0, 1,0x62,  1126,  1126 }, // 436: rickM76; Bass.ins

    // Amplitude begins at  852.4,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 339,339,  0, 1,0x62,   286,   286 }, // 437: rickM77; Basnor04

    // Amplitude begins at    2.4, peaks 1046.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 340,340,  0, 0,0x00, 40000,     0 }, // 438: rickM78; Synbass1

    // Amplitude begins at 1467.7, peaks 1896.9 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 341,341,  0, 1,0x62,  1973,  1973 }, // 439: rickM79; Synbass2

    // Amplitude begins at 1146.6, peaks 1164.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 342,342,  0, 1,0x62,   133,   133 }, // 440: rickM80; Pickbass

    // Amplitude begins at 3717.2, peaks 4282.2 at 0.1s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    {  34, 34,  0, 1,0x3D,  1540,  1540 }, // 441: rickM81; Slapbass

    // Amplitude begins at 1697.4, peaks 2169.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 343,343,  0, 1,0x62, 40000,     0 }, // 442: rickM82; Harpsi1.

    // Amplitude begins at 1470.4, peaks 1727.4 at 4.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 344,344,  0, 1,0x16, 40000,    33 }, // 443: rickM83; Guit_el3

    // Amplitude begins at  733.0, peaks  761.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 226,226,  0, 1,0x62, 40000,    20 }, // 444: rickM84; Guit_fz2

    // Amplitude begins at    0.3, peaks 1205.9 at 0.1s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 345,345,  0, 1,0x16,   400,   400 }, // 445: rickM88; Orchit2.

    // Amplitude begins at   36.8, peaks 4149.2 at 0.1s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 346,346,  0, 1,0x62,  2920,  2920 }, // 446: rickM89; Brass11.

    // Amplitude begins at   42.9, peaks 3542.1 at 0.1s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 347,347,  0, 1,0x0C,  1520,  1520 }, // 447: rickM90; Brass2.i

    // Amplitude begins at   29.8, peaks 2195.8 at 0.1s,
    // fades to 20% at 3.9s, keyoff fades to 20% in 3.9s.
    { 348,348,  0, 1,0x62,  3853,  3853 }, // 448: rickM91; Brass3.i

    // Amplitude begins at  251.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 349,349,  0, 0,0x00,     6,     6 }, // 449: rickM92; Squ_wave

    // Amplitude begins at 3164.4,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 350,350,  0, 0,0x00,  2313,  2313 }, // 450: rickM99; Agogo.in

    // Amplitude begins at  894.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 114,114,  0, 1,0x62,    26,    26 }, // 451: rickM100; Woodblk.

    // Amplitude begins at 1354.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 351,351, 35, 1,0x62,    66,    66 }, // 452: rickP13; kick1.in

    // Amplitude begins at  879.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 352,352, 38, 1,0x3D,    53,    53 }, // 453: rickP17; snare1.i

    // Amplitude begins at  672.5, peaks  773.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 353,353, 38, 1,0x3D,   106,   106 }, // 454: rickP18; snare2.i

    // Amplitude begins at  879.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 352,352, 38, 1,0x0C,    53,    53 }, // 455: rickP19; snare4.i

    // Amplitude begins at  672.5, peaks  773.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 353,353, 38, 1,0x62,   106,   106 }, // 456: rickP20; snare5.i

    // Amplitude begins at 1555.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 31, 1,0x0C,    26,    26 }, // 457: rickP21; rocktom.

    // Amplitude begins at 2153.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 35, 1,0x0C,    40,    40 }, // 458: rickP22; rocktom.

    // Amplitude begins at 2439.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 38, 1,0x0C,    33,    33 }, // 459: rickP23; rocktom.

    // Amplitude begins at 2350.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 41, 1,0x0C,    40,    40 }, // 460: rickP24; rocktom.

    // Amplitude begins at 2086.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 45, 1,0x0C,    40,    40 }, // 461: rickP25; rocktom.

    // Amplitude begins at 2389.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 354,354, 50, 1,0x0C,    53,    53 }, // 462: rickP26; rocktom.

    // Amplitude begins at    0.0, peaks  984.9 at 34.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 4.6s.
    { 355,355, 50, 0,0x00, 40000,  4553 }, // 463: rickP93; openhht1

    // Amplitude begins at 1272.4, peaks 1316.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 332,332, 50, 1,0x62, 40000,  1100 }, // 464: rickP94; openhht2

    // Amplitude begins at 2276.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 356,356, 72, 1,0x62,    93,    93 }, // 465: rickP97; guirol.i

    // Amplitude begins at  878.1,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 148,148, 59, 1,0x62,   153,   153 }, // 466: rickP99; timbale.

    // Amplitude begins at 1799.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 357,357, 64, 1,0x62,    40,    40 }, // 467: rickP101; congas2.

    // Amplitude begins at 1958.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 357,357, 60, 1,0x62,    33,    33 }, // 468: rickP102; congas2.

    // Amplitude begins at 2056.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 358,358, 72, 1,0x62,    26,    26 }, // 469: rickP103; bongos.i

    // Amplitude begins at 1952.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 358,358, 62, 1,0x62,    33,    33 }, // 470: rickP104; bongos.i

    // Amplitude begins at 1783.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 53, 1,0x37,   120,   120 }, // 471: rickP105; surdu.in
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
