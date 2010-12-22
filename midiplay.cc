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

static const unsigned long PCM_RATE = 48000;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // dbopl limitation
static unsigned AdlBank    = 0;
static unsigned NumFourOps = 7;
static unsigned NumCards   = 2;

extern const struct adldata
{
    unsigned carrier_E862, modulator_E862;  // See below
    unsigned char carrier_40, modulator_40; // KSL/attenuation settings
    unsigned char feedconn; // Feedback/connection bits for the channel
    signed char finetune;
} adl[1129];
extern const struct adlinsdata
{
    unsigned short adlno1, adlno2;
    unsigned char tone;
    long ms_sound_kon;  // Number of milliseconds it produces sound;
    long ms_sound_koff;
} adlins[1085];
extern const unsigned short banks[12][256];

static const unsigned short Operators[18] =
    {0x000,0x001,0x002, 0x008,0x009,0x00A, 0x010,0x011,0x012,
     0x100,0x101,0x102, 0x108,0x109,0x10A, 0x110,0x111,0x112 };
static const unsigned short Channels[18] =
    {0x000,0x001,0x002, 0x003,0x004,0x005, 0x006,0x007,0x008,
     0x100,0x101,0x102, 0x103,0x104,0x105, 0x106,0x107,0x108 };

struct OPL3
{
    unsigned NumChannels;

    std::vector<DBOPL::Handler> cards;
    std::vector<unsigned short> ins, pit, insmeta, midiins;

    std::vector<char> four_op_category; // 1 = master, 2 = slave, 0 = regular

    void Poke(unsigned card, unsigned index, unsigned value)
    {
        cards[card].WriteReg(index, value);
    }
    void NoteOff(unsigned c)
    {
        unsigned card = c/18, cc = c%18;
        Poke(card, 0xB0 + Channels[cc], pit[c] & 0xDF);
    }
    void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
    {
        unsigned card = c/18, cc = c%18;
        unsigned x = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x += 0x400; } // Calculate octave
        x += (int)(hertz + 0.5);
        Poke(card, 0xA0 + Channels[cc], x & 0xFF);
        Poke(card, 0xB0 + Channels[cc], pit[c] = x >> 8);
    }
    void Touch_Real(unsigned c, unsigned volume)
    {
        unsigned card = c/18, cc = c%18;
        unsigned i = ins[c], o = Operators[cc];
        unsigned x = adl[i].carrier_40, y = adl[i].modulator_40;
        Poke(card, 0x40+o, (x|63) - volume + volume*(x&63)/63);
        Poke(card, 0x43+o, (y|63) - volume + volume*(y&63)/63);
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
        unsigned card = c/18, cc = c%18;
        static const unsigned char data[4] = {0x20,0x60,0x80,0xE0};
        ins[c] = i;
        unsigned o1 = Operators[cc], o2 = o1 + 3;
        unsigned x = adl[i].carrier_E862, y = adl[i].modulator_E862;
        for(unsigned a=0; a<4; ++a)
        {
            Poke(card, data[a]+o1, x&0xFF); x>>=8;
            Poke(card, data[a]+o2, y&0xFF); y>>=8;
        }
    }
    void Pan(unsigned c, unsigned value)
    {
        unsigned card = c/18, cc = c%18;
        Poke(card, 0xC0 + Channels[cc], adl[ins[c]].feedconn | value);
    }
    void Silence() // Silence all OPL channels.
    {
        for(unsigned c=0; c<NumChannels; ++c) { NoteOff(c); Touch_Real(c,0); }
    }
    void Reset()
    {
        cards.resize(NumCards);
        NumChannels = NumCards * 18;
        ins.resize(NumChannels,     189);
        pit.resize(NumChannels,       0);
        insmeta.resize(NumChannels, 198);
        midiins.resize(NumChannels,   0);
        four_op_category.resize(NumChannels, 0);
        static const short data[] =
        { 0x004,96, 0x004,128,        // Pulse timer
          0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable
          0x001,32, 0x0BD,0x00,       // Enable wave, melodic mode
          0x105,1                     // Enable OPL3 extensions
        };
        unsigned fours = NumFourOps;
        for(unsigned card=0; card<NumCards; ++card)
        {
            cards[card].Init(PCM_RATE);
            for(unsigned a=0; a< sizeof(data)/sizeof(*data); a+=2)
                Poke(card, data[a], data[a+1]);
            unsigned fours_this_card = fours > 6 ? 6 : fours;
            Poke(card, 0x104, (1 << fours_this_card) - 1);
            //fprintf(stderr, "Card %u: %u four-ops.\n", card, fours_this_card);
            fours -= fours_this_card;
        }

        // Mark all channels that are reserved for four-operator function
        unsigned nextfour = 0;
        for(unsigned a=0; a<NumFourOps; ++a)
        {
            four_op_category[nextfour  ] = 1;
            four_op_category[nextfour+3] = 2;
            if(nextfour%3 == 2)
                nextfour += 9-2;
            else
                ++nextfour;
        }
        /*
        In two-op mode, channels 0..8 go as follows:
                      Op1[port]  Op2[port]
          Channel 0:  00  00     03  03
          Channel 1:  01  01     04  04
          Channel 2:  02  02     05  05
          Channel 3:  06  08     09  0B
          Channel 4:  07  09     10  0C
          Channel 5:  08  0A     11  0D
          Channel 6:  12  10     15  13
          Channel 7:  13  11     16  14
          Channel 8:  14  12     17  15
        In four-op mode, channels 0..8 go as follows:
                      Op1[port]  Op2[port]  Op3[port]  Op4[port]
          Channel 0:  00  00     03  03     06  08     09  0B
          Channel 1:  01  01     04  04     07  09     10  0C
          Channel 2:  02  02     05  05     08  0A     11  0D
          Channel 3:  CHANNEL 0 SLAVE
          Channel 4:  CHANNEL 1 SLAVE
          Channel 5:  CHANNEL 2 SLAVE
          Channel 6:  12  10     15  13
          Channel 7:  13  11     16  14
          Channel 8:  14  12     17  15
         Same goes principally for channels 9-17 respectively.
        */

        Silence();
    }
};

static const char MIDIsymbols[256+1] =
"PPPPPPhcckmvmxbd"
"oooooahoGGGGGGGG"
"BBBBBBBVVVVVHHMS"
"SSSOOOcTTTTTTTTX"
"XXXTTTFFFFFFFFFL"
"LLLLLLLppppppppX"
"XXXXXXXGGGGGTSSb"
"bbbMMMcGXXXXXXXD"
"????????????????"
"????????????????"
"???DDshMhhhCCCbM"
"CBDMMDDDMMDDDDDD"
"DDDDDDDDDDDDDD??"
"????????????????"
"????????????????"
"????????????????";

static class UI
{
public:
    int x, y, color, txtline;
    typedef char row[80];
    char slots[80][1 + 18*MaxCards], background[80][1 + 18*MaxCards];
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

        txtline=1 + (txtline) % (18*NumCards);
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
    {
        HideCursor();
        int notex = 2 + (note+55)%77;
        int notey = 1+adlchn;
        char illustrate_char = background[notex][notey];
        if(pressure > 0)
        {
            illustrate_char = MIDIsymbols[ins];
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
    };
    std::vector<AdlChannel> ch;
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
        ch.clear();
        ch.resize(opl.NumChannels);
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
        int midiins = opl.midiins[c];

        if(props_mask & Upd_Off) // note off
        {
            if(Ch[MidCh].sustain == 0)
            {
                opl.NoteOff(c);
                ch[c].age   = 0;
                ch[c].state = AdlChannel::off;
                UI.IllustrateNote(c, tone, midiins, 0, 0.0);
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                ch[c].state = AdlChannel::sustained;
                UI.IllustrateNote(c, tone, midiins, -1, 0.0);
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
            UI.IllustrateNote(c, tone, midiins, vol, Ch[MidCh].bend);
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
        for(unsigned a = 0; a < opl.NumChannels; ++a)
            if(ch[a].age < 0x70000000)
                ch[a].age += t*1000;
        /*for(unsigned a=0; a < opl.NumChannels; ++a)
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
        if(byte == 0xF7 || byte == 0xF0) // Ignore SysEx
        {
            unsigned length = ReadVarLen(tk);
            //std::string data( length?(const char*) &TrackData[tk][CurrentPosition.track[tk].ptr]:0, length );
            CurrentPosition.track[tk].ptr += length;
            UI.PrintLn("SysEx %02X: %u bytes", byte, length/*, data.c_str()*/);
            return;
        }
        if(byte == 0xFF)
        {
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
        if(byte == 0xF3) { CurrentPosition.track[tk].ptr += 1; return; }
        if(byte == 0xF2) { CurrentPosition.track[tk].ptr += 2; return; }
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
                int i[2] = { adlins[meta].adlno1, adlins[meta].adlno2 };

                // Allocate AdLib channel (the physical sound channel for the note)
                int adlchannel[2] = { -1, -1 };
                for(unsigned ccount = 0; ccount < 2; ++ccount)
                {
                    if(ccount == 1)
                    {
                        if(i[0] == i[1]) break; // No secondary channel
                        if(adlchannel[0] == -1) break; // No secondary if primary failed
                    }

                    int c = -1;
                    long bs = -adlins[meta].ms_sound_kon;
                    if(NumFourOps > 0) bs = -9999999;
                    for(int a = 0; a < (int)opl.NumChannels; ++a)
                    {
                        if(ccount == 1 && a == adlchannel[0]) continue;
                        // ^ Don't use the same channel for primary&secondary

                        if(i[0] == i[1])
                        {
                            // Only use regular channels
                            if(opl.four_op_category[a] != 0)
                                continue;
                        }
                        else
                        {
                            if(ccount == 0)
                            {
                                // Only use four-op master channels
                                if(opl.four_op_category[a] != 1)
                                    continue;
                            }
                            else
                            {
                                // The secondary must be played on a specific channel.
                                if(a != adlchannel[0] + 3)
                                    continue;
                            }
                        }

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
                        if(i[ccount] == opl.ins[a]) s += 50;  // Same instrument = good
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
                        /*UI.PrintLn(
                            "collision @%u: G%c%u[%ld/%ld] <- G%c%u",
                            c,
                            opl.midiins[c]<128?'M':'P', opl.midiins[c]&127,
                            ch[c].age, adlins[opl.insmeta[c]].ms_sound_kon,
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
                }
                if(adlchannel[0] < 0 && adlchannel[1] < 0)
                {
                    // The note could not be played, at all.
                    break;
                }
                //UI.PrintLn("i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);

                // Allocate active note for MIDI channel
                std::pair<MIDIchannel::activenoteiterator,bool>
                    ir = Ch[MidCh].activenotes.insert(
                        std::make_pair(note, MIDIchannel::NoteInfo()));
                ir.first->second.adlchn1 = adlchannel[0];
                ir.first->second.adlchn2 = adlchannel[1];
                ir.first->second.vol     = vol;
                ir.first->second.ins1    = i[0];
                ir.first->second.ins2    = i[1];
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
                            for(unsigned c = 0; c < opl.NumChannels; ++c)
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
                        for(unsigned c = 0; c < opl.NumChannels; ++c)
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
        UI.IllustrateNote(c, ch[c].note, opl.midiins[c], 0, 0.0);
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
            chan[i].Create(PCM_RATE,
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
static void SendStereoAudio(unsigned long count, int* samples)
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
        amplitude_display_counter = (PCM_RATE / count) / 24;
        double amp[2]={0,0};
        const unsigned maxy = NumCards*18;
        for(unsigned w=0; w<2; ++w)
        {
            average[w] /= double(count);
            for(unsigned long p = 0; p < count; ++p)
                amp[w] += std::fabs(samples[p*2+w] - average[w]);
            amp[w] /= double(count);
            // Turn into logarithmic scale
            const double dB = std::log(amp[w]<1 ? 1 : amp[w]) * 4.328085123;
            const double maxdB = 3*16; // = 3 * log2(65536)
            amp[w] = maxy*dB/maxdB;
        }
        const unsigned white_threshold  = maxy/18;
        const unsigned red_threshold    = maxy*4/18;
        const unsigned yellow_threshold = maxy*8/18;
        for(unsigned y=0; y<maxy; ++y)
            for(unsigned w=0; w<2; ++w)
            {
                char c = amp[w] > (maxy-1)-y ? '|' : UI.background[w][y+1];
                if(UI.slots[w][y+1] != c)
                {
                    UI.slots[w][y+1] = c;
                    UI.HideCursor();
                    UI.GotoXY(w,y+1);
                    UI.Color(c=='|' ? y<white_threshold ? 15
                                    : y<red_threshold ? 12
                                    : y<yellow_threshold ? 14
                                    : 10 :
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
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq / Interval;
    spec.callback = SDL_AudioCallback;
    if(SDL_OpenAudio(&spec, 0) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }

    if(argc < 2)
    {
        std::printf(
            "Usage: midiplay <midifilename> [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "     Banks: 0 = Miles (SC3, 1993)\n"
            "            1 = HMI GM\n"
            "            2 = HMI Int\n"
            "            3 = HMI Ham\n"
            "            4 = HMI Rick\n"
            "            5 = DMX Doom\n"
            "            6 = DMX Hexen\n"
            "            7 = Miles (WARCRAFT2, 1995)\n"
            "            8 = Miles SIMFARM [four-op]\n"
            "            9 = Miles (SIMFARM, 1993)\n"
            "           10 = Miles 2.14 OSS\n"
            "           11 = Miles UUW2\n"
            "     Use banks 1-4 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
            );
        return 0;
    }

    if(argc >= 3)
    {
        AdlBank = std::atoi(argv[2]);
        if(AdlBank > 11)
        {
            std::fprintf(stderr, "bank number may only be 0..11.\n");
            return 0;
        }
        std::printf("FM instrument bank %u selected.\n", AdlBank);
    }
    if(argc >= 4)
    {
        NumCards = std::atoi(argv[3]);
        if(NumCards < 1 || NumCards > MaxCards)
        {
            std::fprintf(stderr, "number of cards may only be 1..%u.\n", MaxCards);
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
            return 0;
        }
    }
    else
        NumFourOps =
            (AdlBank == 8) ? NumCards * 6
          : (AdlBank != 5 && AdlBank != 6) ? 0
          : (NumCards==1 ? 1 : (5 + (NumCards-1)*3));

    std::printf(
        "Simulating %u OPL3 cards for a total of %u operators.\n"
        "Setting up the operators as %u four-op channels, %u dual-op channels.\n",
        NumCards, NumCards*36,
        NumFourOps, 18*NumCards - NumFourOps*2);

    MIDIplay player;
    if(!player.LoadMIDI(argv[1]))
        return 2;

    unsigned n_fourop[2] = {0,0};
    for(unsigned a=0; a<256; ++a)
    {
        unsigned insno = banks[AdlBank][a];
        if(insno != 198
        && adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a/128];
    }
    std::printf("This bank has %u four-op melodic instruments and %u percussive ones.\n",
        n_fourop[0], n_fourop[1]);

    if(AdlBank == 8 && NumFourOps == 0)
    {
        std::fprintf(stderr,
            "ERROR: You have selected the Miles four-op bank but no four-op channels.\n"
            "       The results (silence + much cpu load) would be probably\n"
            "       not what you want, therefore ignoring the request.\n");
        return 0;
    }

    SDL_PauseAudio(0);

    const double mindelay = 1 / (double)PCM_RATE;
    const double maxdelay = MaxSamplesAtTime / (double)PCM_RATE;

    UI.GotoXY(0,0); UI.Color(15);
    std::fprintf(stderr, "Hit Ctrl-C to quit\r");

    for(double delay=0; ; )
    {
        const double eat_delay = delay < maxdelay ? delay : maxdelay;
        delay -= eat_delay;

        static double carry = 0.0;
        carry += PCM_RATE * eat_delay;
        const unsigned long n_samples = (unsigned) carry;
        carry -= n_samples;

        if(NumCards == 1)
        {
            player.opl.cards[0].Generate(0, SendStereoAudio, n_samples);
        }
        else
        {
            /* Mix together the audio from different cards */
            std::vector<int> sample_buf(n_samples*2, 0);
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
                player.opl.cards[card].Generate(
                    0,
                    Mix::AddStereoAudio,
                    n_samples);
            }
            /* Process it */
            SendStereoAudio(n_samples, &sample_buf[0]);
        }

        while(AudioBuffer.size() > 3*spec.freq/Interval)
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
 *    MILES SOUND SYSTEM FOR STAR CONTROL III
 *    DESCENT DATA FILES (HUMAN MACHINE INTERFACES & PARALLAX SOFTWARE)
 *    DOOM SOUND DRIVER
 *    HEXEN/HERETIC SOUND DRIVER
 *    MILES SOUND SYSTEM FOR WARCRAFT 2
 *    MILES SOUND SYSTEM FOR SIM FARM (QUAD OPERATOR MODE)
 *    MILES SOUND SYSTEM FOR SIM FARM
 * PREPROCESSED, CONVERTED, AND POSTPROCESSED OFF-SCREEN.
 */
const adldata adl[1129] =
{ //    ,---------+-------- Wave select settings
  //    | ,-------÷-+------ Sustain/release rates
  //    | | ,-----÷-÷-+---- Attack/decay rates
  //    | | | ,---÷-÷-÷-+-- AM/VIB/EG/KSR/Multiple bits
  //    | | | |   | | | |
  //    | | | |   | | | |     ,----+-- KSL/attenuation settings
  //    | | | |   | | | |     |    |    ,----- Feedback/connection bits
  //    | | | |   | | | |     |    |    |
    { 0x0F4F201,0x0F7F201, 0x8F,0x06, 0x8,+0 }, // 0: GM0; sGM0; AcouGrandPiano
    { 0x0F4F201,0x0F7F201, 0x4B,0x00, 0x8,+0 }, // 1: GM1; sGM1; BrightAcouGrand
    { 0x0F4F201,0x0F6F201, 0x49,0x00, 0x8,+0 }, // 2: GM2; sGM2; ElecGrandPiano
    { 0x0F7F281,0x0F7F241, 0x12,0x00, 0x6,+0 }, // 3: GM3; sGM3; Honky-tonkPiano
    { 0x0F7F101,0x0F7F201, 0x57,0x00, 0x0,+0 }, // 4: GM4; sGM4; Rhodes Piano
    { 0x0F7F101,0x0F7F201, 0x93,0x00, 0x0,+0 }, // 5: GM5; sGM5; Chorused Piano
    { 0x0F2A101,0x0F5F216, 0x80,0x0E, 0x8,+0 }, // 6: GM6; Harpsichord
    { 0x0F8C201,0x0F8C201, 0x92,0x00, 0xA,+0 }, // 7: GM7; sGM7; Clavinet
    { 0x0F4F60C,0x0F5F381, 0x5C,0x00, 0x0,+0 }, // 8: GM8; sGM8; Celesta
    { 0x0F2F307,0x0F1F211, 0x97,0x80, 0x2,+0 }, // 9: GM9; Glockenspiel
    { 0x0F45417,0x0F4F401, 0x21,0x00, 0x2,+0 }, // 10: GM10; sGM10; Music box
    { 0x0F6F398,0x0F6F281, 0x62,0x00, 0x0,+0 }, // 11: GM11; sGM11; Vibraphone
    { 0x0F6F618,0x0F7E701, 0x23,0x00, 0x0,+0 }, // 12: GM12; sGM12; Marimba
    { 0x0F6F615,0x0F6F601, 0x91,0x00, 0x4,+0 }, // 13: GM13; sGM13; Xylophone
    { 0x0F3D345,0x0F3A381, 0x59,0x80, 0xC,+0 }, // 14: GM14; Tubular Bells
    { 0x1F57503,0x0F5B581, 0x49,0x80, 0x4,+0 }, // 15: GM15; sGM15; Dulcimer
    { 0x014F671,0x007F131, 0x92,0x00, 0x2,+0 }, // 16: GM16; HMIGM16; sGM16; Hammond Organ; am016.in
    { 0x058C772,0x008C730, 0x14,0x00, 0x2,+0 }, // 17: GM17; HMIGM17; sGM17; Percussive Organ; am017.in
    { 0x018AA70,0x0088AB1, 0x44,0x00, 0x4,+0 }, // 18: GM18; HMIGM18; sGM18; Rock Organ; am018.in
    { 0x1239723,0x01455B1, 0x93,0x00, 0x4,+0 }, // 19: GM19; HMIGM19; Church Organ; am019.in
    { 0x1049761,0x00455B1, 0x13,0x80, 0x0,+0 }, // 20: GM20; HMIGM20; sGM20; Reed Organ; am020.in
    { 0x12A9824,0x01A46B1, 0x48,0x00, 0xC,+0 }, // 21: GM21; HMIGM21; sGM21; Accordion; am021.in
    { 0x1069161,0x0076121, 0x13,0x00, 0xA,+0 }, // 22: GM22; HMIGM22; sGM22; Harmonica; am022.in
    { 0x0067121,0x00761A1, 0x13,0x89, 0x6,+0 }, // 23: GM23; HMIGM23; sGM23; Tango Accordion; am023.in
    { 0x194F302,0x0C8F341, 0x9C,0x80, 0xC,+0 }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in
    { 0x19AF303,0x0E7F111, 0x54,0x00, 0xC,+0 }, // 25: GM25; HMIGM25; mGM25; sGM25; Acoustic Guitar2; am025.in
    { 0x03AF123,0x0F8F221, 0x5F,0x00, 0x0,+0 }, // 26: GM26; HMIGM26; mGM26; sGM26; Electric Guitar1; am026.in
    { 0x122F603,0x0F8F321, 0x87,0x80, 0x6,+0 }, // 27: GM27; sGM27; Electric Guitar2
    { 0x054F903,0x03AF621, 0x47,0x00, 0x0,+0 }, // 28: GM28; HMIGM28; hamM3; hamM60; intM3; mGM28; rickM3; sGM28; BPerc; BPerc.in; Electric Guitar3; am028.in; muteguit
    { 0x1419123,0x0198421, 0x4A,0x05, 0x8,+0 }, // 29: GM29; sGM29; Overdrive Guitar
    { 0x1199523,0x0199421, 0x4A,0x00, 0x8,+0 }, // 30: GM30; HMIGM30; hamM6; intM6; mGM30; rickM6; sGM30; Distorton Guitar; GDist; GDist.in; am030.in
    { 0x04F2009,0x0F8D184, 0xA1,0x80, 0x8,+0 }, // 31: GM31; HMIGM31; hamM5; intM5; rickM5; sGM31; GFeedbck; Guitar Harmonics; am031.in
    { 0x0069421,0x0A6C3A2, 0x1E,0x00, 0x2,+0 }, // 32: GM32; HMIGM32; sGM32; Acoustic Bass; am032.in
    { 0x028F131,0x018F131, 0x12,0x00, 0xA,+0 }, // 33: GM33; GM39; HMIGM33; HMIGM39; hamM68; mGM33; mGM39; sGM33; sGM39; Electric Bass 1; Synth Bass 2; am033.in; am039.in; synbass2
    { 0x0E8F131,0x078F131, 0x8D,0x00, 0xA,+0 }, // 34: GM34; HMIGM34; mGM34; rickM81; sGM34; Electric Bass 2; Slapbass; am034.in
    { 0x0285131,0x0487132, 0x5B,0x00, 0xC,+0 }, // 35: GM35; HMIGM35; mGM35; qGM35; sGM35; Fretless Bass; am035.in
    { 0x09AA101,0x0DFF221, 0x8B,0x40, 0x8,+0 }, // 36: GM36; HMIGM36; mGM36; qGM36; sGM36; Slap Bass 1; am036.in
    { 0x016A221,0x0DFA121, 0x8B,0x08, 0x8,+0 }, // 37: GM37; sGM37; Slap Bass 2
    { 0x0E8F431,0x078F131, 0x8B,0x00, 0xA,+0 }, // 38: GM38; HMIGM38; hamM13; hamM67; intM13; mGM38; rickM13; sGM38; BSynth3; BSynth3.; Synth Bass 1; am038.in; synbass1
    { 0x113DD31,0x0265621, 0x15,0x00, 0x8,+0 }, // 39: GM40; HMIGM40; mGM40; qGM40; sGM40; Violin; am040.in
    { 0x113DD31,0x0066621, 0x16,0x00, 0x8,+0 }, // 40: GM41; HMIGM41; mGM41; sGM41; Viola; am041.in
    { 0x11CD171,0x00C6131, 0x49,0x00, 0x8,+0 }, // 41: GM42; HMIGM42; sGM42; Cello; am042.in
    { 0x1127121,0x0067223, 0x4D,0x80, 0x2,+0 }, // 42: GM43; HMIGM43; mGM43; sGM43; Contrabass; am043.in
    { 0x121F1F1,0x0166FE1, 0x40,0x00, 0x2,+0 }, // 43: GM44; HMIGM44; mGM44; Tremulo Strings; am044.in
    { 0x175F502,0x0358501, 0x1A,0x80, 0x0,+0 }, // 44: GM45; HMIGM45; mGM45; Pizzicato String; am045.in
    { 0x175F502,0x0F4F301, 0x1D,0x80, 0x0,+0 }, // 45: GM46; HMIGM46; mGM46; oGM57; oGM58; Orchestral Harp; Trombone; Tuba; am046.in
    { 0x105F510,0x0C3F211, 0x41,0x00, 0x2,+0 }, // 46: GM47; HMIGM47; hamM14; intM14; mGM47; qGM47; rickM14; BSynth4; BSynth4.; Timpany; am047.in
    { 0x125B121,0x00872A2, 0x9B,0x01, 0xE,+0 }, // 47: GM48; HMIGM48; String Ensemble1; am048.in
    { 0x1037FA1,0x1073F21, 0x98,0x00, 0x0,+0 }, // 48: GM49; HMIGM49; String Ensemble2; am049.in
    { 0x012C1A1,0x0054F61, 0x93,0x00, 0xA,+0 }, // 49: GM50; HMIGM50; hamM20; intM20; rickM20; PMellow; PMellow.; Synth Strings 1; am050.in
    { 0x022C121,0x0054F61, 0x18,0x00, 0xC,+0 }, // 50: GM51; HMIGM51; qGM51; sGM51; SynthStrings 2; am051.in
    { 0x015F431,0x0058A72, 0x5B,0x83, 0x0,+0 }, // 51: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in
    { 0x03974A1,0x0677161, 0x90,0x00, 0x0,+0 }, // 52: GM53; HMIGM53; rickM86; sGM53; Oohs.ins; Voice Oohs; am053.in
    { 0x0055471,0x0057A72, 0x57,0x00, 0xC,+0 }, // 53: GM54; HMIGM54; sGM54; Synth Voice; am054.in
    { 0x0635490,0x045A541, 0x00,0x00, 0x8,+0 }, // 54: GM55; HMIGM55; Orchestra Hit; am055.in
    { 0x0178521,0x0098F21, 0x92,0x01, 0xC,+0 }, // 55: GM56; HMIGM56; mGM56; qGM56; Trumpet; am056.in
    { 0x0177521,0x0098F21, 0x94,0x05, 0xC,+0 }, // 56: GM57; HMIGM57; mGM57; Trombone; am057.in
    { 0x0157621,0x0378261, 0x94,0x00, 0xC,+0 }, // 57: GM58; HMIGM58; Tuba; am058.in
    { 0x1179E31,0x12C6221, 0x43,0x00, 0x2,+0 }, // 58: GM59; HMIGM59; mGM59; sGM59; Muted Trumpet; am059.in
    { 0x06A6121,0x00A7F21, 0x9B,0x00, 0x2,+0 }, // 59: GM60; HMIGM60; mGM60; French Horn; am060.in
    { 0x01F7561,0x00F7422, 0x8A,0x06, 0x8,+0 }, // 60: GM61; HMIGM61; Brass Section; am061.in
    { 0x15572A1,0x0187121, 0x86,0x83, 0x0,+0 }, // 61: GM62; sGM62; Synth Brass 1
    { 0x03C5421,0x01CA621, 0x4D,0x00, 0x8,+0 }, // 62: GM63; HMIGM63; mGM63; sGM63; Synth Brass 2; am063.in
    { 0x1029331,0x00B7261, 0x8F,0x00, 0x8,+0 }, // 63: GM64; HMIGM64; sGM64; Soprano Sax; am064.in
    { 0x1039331,0x0097261, 0x8E,0x00, 0x8,+0 }, // 64: GM65; HMIGM65; sGM65; Alto Sax; am065.in
    { 0x1039331,0x0098261, 0x91,0x00, 0xA,+0 }, // 65: GM66; HMIGM66; sGM66; Tenor Sax; am066.in
    { 0x10F9331,0x00F7261, 0x8E,0x00, 0xA,+0 }, // 66: GM67; HMIGM67; sGM67; Baritone Sax; am067.in
    { 0x116AA21,0x00A8F21, 0x4B,0x00, 0x8,+0 }, // 67: GM68; HMIGM68; mGM68; Oboe; am068.in
    { 0x1177E31,0x10C8B21, 0x90,0x00, 0x6,+0 }, // 68: GM69; HMIGM69; mGM69; sGM69; English Horn; am069.in
    { 0x1197531,0x0196132, 0x81,0x00, 0x0,+0 }, // 69: GM70; HMIGM70; mGM70; Bassoon; am070.in
    { 0x0219B32,0x0177221, 0x90,0x00, 0x4,+0 }, // 70: GM71; HMIGM71; mGM71; Clarinet; am071.in
    { 0x05F85E1,0x01A65E1, 0x1F,0x00, 0x0,+0 }, // 71: GM72; HMIGM72; mGM72; Piccolo; am072.in
    { 0x05F88E1,0x01A65E1, 0x46,0x00, 0x0,+0 }, // 72: GM73; HMIGM73; mGM73; Flute; am073.in
    { 0x01F75A1,0x00A7521, 0x9C,0x00, 0x2,+0 }, // 73: GM74; HMIGM74; sGM74; Recorder; am074.in
    { 0x0588431,0x01A6521, 0x8B,0x00, 0x0,+0 }, // 74: GM75; HMIGM75; mGM75; sGM75; Pan Flute; am075.in
    { 0x05666E1,0x02665A1, 0x4C,0x00, 0x0,+0 }, // 75: GM76; HMIGM76; sGM76; Bottle Blow; am076.in
    { 0x0467662,0x03655A1, 0xCB,0x00, 0x0,+0 }, // 76: GM77; HMIGM77; sGM77; Shakuhachi; am077.in
    { 0x0075762,0x00756A1, 0x99,0x00, 0xB,+0 }, // 77: GM78; HMIGM78; sGM78; Whistle; am078.in
    { 0x0077762,0x00776A1, 0x93,0x00, 0xB,+0 }, // 78: GM79; HMIGM79; hamM61; sGM79; Ocarina; am079.in; ocarina
    { 0x203FF22,0x00FFF21, 0x59,0x00, 0x0,+0 }, // 79: GM80; HMIGM80; hamM16; hamM65; intM16; mGM80; rickM16; sGM80; LSquare; LSquare.; Lead 1 squareea; am080.in; squarewv
    { 0x10FFF21,0x10FFF21, 0x0E,0x00, 0x0,+0 }, // 80: GM81; HMIGM81; mGM81; sGM81; Lead 2 sawtooth; am081.in
    { 0x0558622,0x0186421, 0x46,0x80, 0x0,+0 }, // 81: GM82; HMIGM82; mGM82; sGM82; Lead 3 calliope; am082.in
    { 0x0126621,0x00A96A1, 0x45,0x00, 0x0,+0 }, // 82: GM83; HMIGM83; sGM83; Lead 4 chiff; am083.in
    { 0x12A9221,0x02A9122, 0x8B,0x00, 0x0,+0 }, // 83: GM84; HMIGM84; mGM84; sGM84; Lead 5 charang; am084.in
    { 0x005DFA2,0x0076F61, 0x9E,0x40, 0x2,+0 }, // 84: GM85; HMIGM85; hamM17; intM17; rickM17; rickM87; sGM85; Lead 6 voice; PFlutes; PFlutes.; Solovox.; am085.in
    { 0x001EF20,0x2068F60, 0x1A,0x00, 0x0,+0 }, // 85: GM86; HMIGM86; rickM93; sGM86; Lead 7 fifths; Saw_wave; am086.in
    { 0x029F121,0x009F421, 0x8F,0x80, 0xA,+0 }, // 86: GM87; HMIGM87; mGM87; qGM87; sGM87; Lead 8 brass; am087.in
    { 0x0945377,0x005A0A1, 0xA5,0x00, 0x2,+0 }, // 87: GM88; HMIGM88; sGM88; Pad 1 new age; am088.in
    { 0x011A861,0x00325B1, 0x1F,0x80, 0xA,+0 }, // 88: GM89; HMIGM89; qGM89; sGM89; Pad 2 warm; am089.in
    { 0x0349161,0x0165561, 0x17,0x00, 0xC,+0 }, // 89: GM90; HMIGM90; hamM21; intM21; rickM21; sGM90; LTriang; LTriang.; Pad 3 polysynth; am090.in
    { 0x0015471,0x0036A72, 0x5D,0x00, 0x0,+0 }, // 90: GM91; HMIGM91; rickM95; sGM91; Pad 4 choir; Spacevo.; am091.in
    { 0x0432121,0x03542A2, 0x97,0x00, 0x8,+0 }, // 91: GM92; HMIGM92; sGM92; Pad 5 bowedpad; am092.in
    { 0x177A1A1,0x1473121, 0x1C,0x00, 0x0,+0 }, // 92: GM93; HMIGM93; hamM22; intM22; rickM22; sGM93; PSlow; PSlow.in; Pad 6 metallic; am093.in
    { 0x0331121,0x0254261, 0x89,0x03, 0xA,+0 }, // 93: GM94; HMIGM94; hamM23; hamM54; intM23; rickM23; rickM96; sGM94; Halopad.; PSweep; PSweep.i; Pad 7 halo; am094.in; halopad
    { 0x14711A1,0x007CF21, 0x15,0x00, 0x0,+0 }, // 94: GM95; HMIGM95; hamM66; rickM97; Pad 8 sweep; Sweepad.; am095.in; sweepad
    { 0x0F6F83A,0x0028651, 0xCE,0x00, 0x2,+0 }, // 95: GM96; HMIGM96; sGM96; FX 1 rain; am096.in
    { 0x1232121,0x0134121, 0x15,0x00, 0x0,+0 }, // 96: GM97; HMIGM97; mGM97; sGM97; FX 2 soundtrack; am097.in
    { 0x0957406,0x072A501, 0x5B,0x00, 0x0,+0 }, // 97: GM98; HMIGM98; mGM98; sGM98; FX 3 crystal; am098.in
    { 0x081B122,0x026F261, 0x92,0x83, 0xC,+0 }, // 98: GM99; HMIGM99; sGM99; FX 4 atmosphere; am099.in
    { 0x151F141,0x0F5F242, 0x4D,0x00, 0x0,+0 }, // 99: GM100; HMIGM100; hamM51; sGM100; FX 5 brightness; am100; am100.in
    { 0x1511161,0x01311A3, 0x94,0x80, 0x6,+0 }, // 100: GM101; HMIGM101; sGM101; FX 6 goblins; am101.in
    { 0x0311161,0x0031DA1, 0x8C,0x80, 0x6,+0 }, // 101: GM102; HMIGM102; rickM98; sGM102; Echodrp1; FX 7 echoes; am102.in
    { 0x173F3A4,0x0238161, 0x4C,0x00, 0x4,+0 }, // 102: GM103; HMIGM103; sGM103; FX 8 sci-fi; am103.in
    { 0x053D202,0x1F6F207, 0x85,0x03, 0x0,+0 }, // 103: GM104; HMIGM104; mGM104; sGM104; Sitar; am104.in
    { 0x111A311,0x0E5A213, 0x0C,0x80, 0x0,+0 }, // 104: GM105; HMIGM105; mGM105; sGM105; Banjo; am105.in
    { 0x141F611,0x2E6F211, 0x06,0x00, 0x4,+0 }, // 105: GM106; HMIGM106; hamM24; intM24; mGM106; rickM24; sGM106; LDist; LDist.in; Shamisen; am106.in
    { 0x032D493,0x111EB91, 0x91,0x00, 0x8,+0 }, // 106: GM107; HMIGM107; sGM107; Koto; am107.in
    { 0x056FA04,0x005C201, 0x4F,0x00, 0xC,+0 }, // 107: GM108; HMIGM108; hamM57; mGM108; qGM108; sGM108; Kalimba; am108.in; kalimba
    { 0x0207C21,0x10C6F22, 0x49,0x00, 0x6,+0 }, // 108: GM109; HMIGM109; mGM109; qGM109; sGM109; Bagpipe; am109.in
    { 0x133DD31,0x0165621, 0x85,0x00, 0xA,+0 }, // 109: GM110; HMIGM110; mGM110; qGM110; sGM110; Fiddle; am110.in
    { 0x205DA20,0x00B8F21, 0x04,0x81, 0x6,+0 }, // 110: GM111; HMIGM111; mGM111; sGM111; Shanai; am111.in
    { 0x0E5F105,0x0E5C303, 0x6A,0x80, 0x6,+0 }, // 111: GM112; HMIGM112; mGM112; qGM112; sGM112; Tinkle Bell; am112.in
    { 0x026EC07,0x016F802, 0x15,0x00, 0xA,+0 }, // 112: GM113; HMIGM113; hamM50; mGM113; sGM113; Agogo Bells; agogo; am113.in
    { 0x0356705,0x005DF01, 0x9D,0x00, 0x8,+0 }, // 113: GM114; HMIGM114; mGM114; sGM114; Steel Drums; am114.in
    { 0x028FA18,0x0E5F812, 0x96,0x00, 0xA,+0 }, // 114: GM115; HMIGM115; mGM115; rickM100; sGM115; Woodblk.; Woodblock; am115.in
    { 0x007A810,0x003FA00, 0x86,0x03, 0x6,+0 }, // 115: GM116; HMIGM116; hamM69; hamP90; mGM116; qGM116; Taiko; Taiko Drum; am116.in; taiko
    { 0x247F811,0x003F310, 0x41,0x03, 0x4,+0 }, // 116: GM117; HMIGM117; hamM58; mGM117; sGM117; Melodic Tom; am117.in; melotom
    { 0x206F101,0x002F310, 0x8E,0x00, 0xE,+0 }, // 117: GM118; HMIGM118; mGM118; Synth Drum; am118.in
    { 0x0001F0E,0x3FF1FC0, 0x00,0x00, 0xE,+0 }, // 118: GM119; HMIGM119; mGM119; Reverse Cymbal; am119.in
    { 0x024F806,0x2845603, 0x80,0x88, 0xE,+0 }, // 119: GM120; HMIGM120; hamM36; intM36; mGM120; rickM101; rickM36; sGM120; DNoise1; DNoise1.; Fretnos.; Guitar FretNoise; am120.in
    { 0x000F80E,0x30434D0, 0x00,0x05, 0xE,+0 }, // 120: GM121; HMIGM121; mGM121; sGM121; Breath Noise; am121.in
    { 0x000F60E,0x3021FC0, 0x00,0x00, 0xE,+0 }, // 121: GM122; HMIGM122; mGM122; sGM122; Seashore; am122.in
    { 0x0A337D5,0x03756DA, 0x95,0x40, 0x0,+0 }, // 122: GM123; HMIGM123; mGM123; sGM123; Bird Tweet; am123.in
    { 0x261B235,0x015F414, 0x5C,0x08, 0xA,+0 }, // 123: GM124; HMIGM124; mGM124; sGM124; Telephone; am124.in
    { 0x000F60E,0x3F54FD0, 0x00,0x00, 0xE,+0 }, // 124: GM125; HMIGM125; mGM125; sGM125; Helicopter; am125.in
    { 0x001FF26,0x11612E4, 0x00,0x00, 0xE,+0 }, // 125: GM126; HMIGM126; mGM126; sGM126; Applause/Noise; am126.in
    { 0x0F0F300,0x2C9F600, 0x00,0x00, 0xE,+0 }, // 126: GM127; HMIGM127; mGM127; sGM127; Gunshot; am127.in
    { 0x277F810,0x006F311, 0x44,0x00, 0x8,+0 }, // 127: GP35; GP36; hamP11; hamP34; hamP35; intP34; intP35; mGP35; mGP36; qGP35; qGP36; rickP14; rickP34; rickP35; Ac Bass Drum; Bass Drum 1; apo035; apo035.i; aps035; kick2.in
    { 0x0FFF902,0x0FFF811, 0x07,0x00, 0x8,+0 }, // 128: GP37; mGP37; Side Stick
    { 0x205FC00,0x017FA00, 0x00,0x00, 0xE,+0 }, // 129: GP38; GP40; mGP38; mGP40; qGP38; qGP40; Acoustic Snare; Electric Snare
    { 0x007FF00,0x008FF01, 0x02,0x00, 0x0,+0 }, // 130: GP39; mGP39; Hand Clap
    { 0x00CF600,0x006F600, 0x00,0x00, 0x4,+0 }, // 131: GP41; GP43; GP45; GP47; GP48; GP50; GP87; hamP1; hamP2; hamP3; hamP4; hamP5; hamP6; mGP41; mGP43; mGP45; mGP47; mGP48; mGP50; mGP87; rickP105; sGP87; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; Open Surdu; aps041; surdu.in
    { 0x008F60C,0x247FB12, 0x00,0x00, 0xA,+0 }, // 132: GP42; hamP55; intP55; mGP42; qGP42; rickP55; Closed High Hat; aps042; aps042.i
    { 0x008F60C,0x2477B12, 0x00,0x05, 0xA,+0 }, // 133: GP44; mGP44; Pedal High Hat
    { 0x002F60C,0x243CB12, 0x00,0x00, 0xA,+0 }, // 134: GP46; mGP46; qGP46; Open High Hat
    { 0x000F60E,0x3029FD0, 0x00,0x00, 0xE,+0 }, // 135: GP49; GP57; hamP0; mGP49; mGP57; oGP49; Crash Cymbal 1; Crash Cymbal 2; crash1
    { 0x042F80E,0x3E4F407, 0x08,0x4A, 0xE,+0 }, // 136: GP51; GP59; mGP51; mGP59; sGP51; sGP59; Ride Cymbal 1; Ride Cymbal 2
    { 0x030F50E,0x0029FD0, 0x00,0x0A, 0xE,+0 }, // 137: GP52; hamP19; mGP52; Chinese Cymbal; aps052
    { 0x3E4E40E,0x1E5F507, 0x0A,0x5D, 0x6,+0 }, // 138: GP53; mGP53; sGP53; Ride Bell
    { 0x004B402,0x0F79705, 0x03,0x0A, 0xE,+0 }, // 139: GP54; mGP54; Tambourine
    { 0x000F64E,0x3029F9E, 0x00,0x00, 0xE,+0 }, // 140: GP55; Splash Cymbal
    { 0x237F811,0x005F310, 0x45,0x08, 0x8,+0 }, // 141: GP56; mGP56; sGP56; Cow Bell
    { 0x303FF80,0x014FF10, 0x00,0x0D, 0xC,+0 }, // 142: GP58; Vibraslap
    { 0x00CF506,0x008F502, 0x0B,0x00, 0x6,+0 }, // 143: GP60; mGP60; sGP60; High Bongo
    { 0x0BFFA01,0x097C802, 0x00,0x00, 0x7,+0 }, // 144: GP61; mGP61; oGP61; sGP61; Low Bongo
    { 0x087FA01,0x0B7FA01, 0x51,0x00, 0x6,+0 }, // 145: GP62; mGP62; qGP62; sGP62; Mute High Conga
    { 0x08DFA01,0x0B8F802, 0x54,0x00, 0x6,+0 }, // 146: GP63; mGP63; sGP63; Open High Conga
    { 0x088FA01,0x0B6F802, 0x59,0x00, 0x6,+0 }, // 147: GP64; mGP64; sGP64; Low Conga
    { 0x30AF901,0x006FA00, 0x00,0x00, 0xE,+0 }, // 148: GP65; hamP8; mGP65; rickP98; rickP99; sGP65; High Timbale; timbale; timbale.
    { 0x389F900,0x06CF600, 0x80,0x00, 0xE,+0 }, // 149: GP66; mGP66; qGP66; sGP66; Low Timbale
    { 0x388F803,0x0B6F60C, 0x80,0x08, 0xF,+0 }, // 150: GP67; mGP67; sGP67; High Agogo
    { 0x388F803,0x0B6F60C, 0x85,0x00, 0xF,+0 }, // 151: GP68; mGP68; sGP68; Low Agogo
    { 0x04F760E,0x2187700, 0x40,0x08, 0xE,+0 }, // 152: GP69; mGP69; Cabasa
    { 0x049C80E,0x2699B03, 0x40,0x00, 0xE,+0 }, // 153: GP70; mGP70; sGP70; Maracas
    { 0x305ADD7,0x0058DC7, 0xDC,0x00, 0xE,+0 }, // 154: GP71; mGP71; sGP71; Short Whistle
    { 0x304A8D7,0x00488C7, 0xDC,0x00, 0xE,+0 }, // 155: GP72; mGP72; sGP72; Long Whistle
    { 0x306F680,0x3176711, 0x00,0x00, 0xE,+0 }, // 156: GP73; rickP96; sGP73; Short Guiro; guiros.i
    { 0x205F580,0x3164611, 0x00,0x09, 0xE,+0 }, // 157: GP74; sGP74; Long Guiro
    { 0x0F40006,0x0F5F715, 0x3F,0x00, 0x1,+0 }, // 158: GP75; mGP75; oGP75; sGP75; Claves
    { 0x3F40006,0x0F5F712, 0x3F,0x00, 0x0,+0 }, // 159: GP76; mGP76; qGP76; qGP77; High Wood Block; Low Wood Block
    { 0x0F40006,0x0F5F712, 0x3F,0x00, 0x1,+0 }, // 160: GP77; mGP77; sGP77; Low Wood Block
    { 0x0E76701,0x0077502, 0x58,0x00, 0x0,+0 }, // 161: GP78; mGP78; sGP78; Mute Cuica
    { 0x048F841,0x0057542, 0x45,0x08, 0x0,+0 }, // 162: GP79; sGP79; Open Cuica
    { 0x3F0E00A,0x005FF1E, 0x40,0x4E, 0x8,+0 }, // 163: GP80; mGP80; sGP80; Mute Triangle
    { 0x3F0E00A,0x002FF1E, 0x7C,0x52, 0x8,+0 }, // 164: GP81; mGP81; sGP81; Open Triangle
    { 0x04A7A0E,0x21B7B00, 0x40,0x08, 0xE,+0 }, // 165: GP82; hamP7; mGP82; sGP82; Shaker; aps082
    { 0x3E4E40E,0x1395507, 0x0A,0x40, 0x6,+0 }, // 166: GP83; mGP83; sGP83; Jingle Bell
    { 0x332F905,0x0A5D604, 0x05,0x40, 0xE,+0 }, // 167: GP84; mGP84; sGP84; Bell Tree
    { 0x3F30002,0x0F5F715, 0x3F,0x00, 0x8,+0 }, // 168: GP85; mGP85; sGP85; Castanets
    { 0x08DFA01,0x0B5F802, 0x4F,0x00, 0x7,+0 }, // 169: GP86; mGP86; sGP86; Mute Surdu
    { 0x1199523,0x0198421, 0x48,0x00, 0x8,+0 }, // 170: HMIGM0; HMIGM29; mGM29; Overdrive Guitar; am029.in
    { 0x054F231,0x056F221, 0x4B,0x00, 0x8,+0 }, // 171: HMIGM1; mGM1; BrightAcouGrand; am001.in
    { 0x055F231,0x076F221, 0x49,0x00, 0x8,+0 }, // 172: HMIGM2; mGM2; ElecGrandPiano; am002.in
    { 0x03BF2B1,0x00BF361, 0x0E,0x00, 0x6,+0 }, // 173: HMIGM3; am003.in
    { 0x038F101,0x028F121, 0x57,0x00, 0x0,+0 }, // 174: HMIGM4; mGM4; Rhodes Piano; am004.in
    { 0x038F101,0x028F121, 0x93,0x00, 0x0,+0 }, // 175: HMIGM5; mGM5; Chorused Piano; am005.in
    { 0x001A221,0x0D5F136, 0x80,0x0E, 0x8,+0 }, // 176: HMIGM6; mGM6; Harpsichord; am006.in
    { 0x0A8C201,0x058C201, 0x92,0x00, 0xA,+0 }, // 177: HMIGM7; mGM7; Clavinet; am007.in
    { 0x054F60C,0x0B5F381, 0x5C,0x00, 0x0,+0 }, // 178: HMIGM8; am008.in
    { 0x032F607,0x011F511, 0x97,0x80, 0x2,+0 }, // 179: HMIGM9; mGM9; Glockenspiel; am009.in
    { 0x0045617,0x004F601, 0x21,0x00, 0x2,+0 }, // 180: HMIGM10; mGM10; qGM10; Music box; am010.in
    { 0x0E6F318,0x0F6F281, 0x62,0x00, 0x0,+0 }, // 181: HMIGM11; am011.in
    { 0x055F718,0x0D8E521, 0x23,0x00, 0x0,+0 }, // 182: HMIGM12; mGM12; Marimba; am012.in
    { 0x0A6F615,0x0E6F601, 0x91,0x00, 0x4,+0 }, // 183: HMIGM13; mGM13; Xylophone; am013.in
    { 0x082D345,0x0E3A381, 0x59,0x80, 0xC,+0 }, // 184: HMIGM14; am014.in
    { 0x1557403,0x005B381, 0x49,0x80, 0x4,+0 }, // 185: HMIGM15; am015.in
    { 0x122F603,0x0F3F321, 0x87,0x80, 0x6,+0 }, // 186: HMIGM27; mGM27; Electric Guitar2; am027.in
    { 0x09AA101,0x0DFF221, 0x89,0x40, 0x8,+0 }, // 187: HMIGM37; mGM37; Slap Bass 2; am037.in
    { 0x15572A1,0x0187121, 0x86,0x0D, 0x0,+0 }, // 188: HMIGM62; am062.in
    { 0x0F00010,0x0F00010, 0x3F,0x3F, 0x0,+0 }, // 189: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in
    { 0x0F1F02E,0x3487407, 0x00,0x07, 0x8,+0 }, // 190: HMIGP27; HMIGP28; HMIGP29; HMIGP30; HMIGP31; Wierd1.i
    { 0x0FE5229,0x3D9850E, 0x00,0x07, 0x6,+0 }, // 191: HMIGP32; HMIGP33; HMIGP34; Wierd2.i
    { 0x0FE6227,0x3D9950A, 0x00,0x07, 0x8,+0 }, // 192: HMIGP35; Wierd3.i
    { 0x0FDF800,0x0C7F601, 0x0B,0x00, 0x8,+0 }, // 193: HMIGP36; Kick.ins
    { 0x0FBF116,0x069F911, 0x08,0x02, 0x0,+0 }, // 194: HMIGP37; HMIGP85; HMIGP86; RimShot.; rimShot.; rimshot.
    { 0x000FF26,0x0A7F802, 0x00,0x02, 0xE,+0 }, // 195: HMIGP38; HMIGP40; Snare.in
    { 0x00F9F30,0x0FAE83A, 0x00,0x00, 0xE,+0 }, // 196: HMIGP39; Clap.ins
    { 0x01FFA06,0x0F5F511, 0x0A,0x00, 0xF,+0 }, // 197: HMIGP41; HMIGP43; HMIGP45; HMIGP47; HMIGP48; HMIGP50; Toms.ins
    { 0x0F1F52E,0x3F99906, 0x05,0x02, 0x0,+0 }, // 198: HMIGP42; HMIGP44; clshat97
    { 0x0F89227,0x3D8750A, 0x00,0x03, 0x8,+0 }, // 199: HMIGP46; Opnhat96
    { 0x2009F2C,0x3A4C50E, 0x00,0x09, 0xE,+0 }, // 200: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym
    { 0x0009429,0x344F904, 0x10,0x0C, 0xE,+0 }, // 201: HMIGP51; HMIGP53; HMIGP59; Ridecym.; ridecym.
    { 0x0F1F52E,0x3F78706, 0x09,0x02, 0x0,+0 }, // 202: HMIGP54; Tamb.ins
    { 0x2F1F535,0x028F703, 0x19,0x02, 0x0,+0 }, // 203: HMIGP56; Cowbell.
    { 0x100FF80,0x1F7F500, 0x00,0x00, 0xC,+0 }, // 204: HMIGP58; vibrasla
    { 0x0FAFA25,0x0F99803, 0xCD,0x00, 0x0,+0 }, // 205: HMIGP60; HMIGP62; mutecong
    { 0x1FAF825,0x0F7A803, 0x1B,0x00, 0x0,+0 }, // 206: HMIGP61; conga.in
    { 0x1FAF825,0x0F69603, 0x21,0x00, 0xE,+0 }, // 207: HMIGP63; HMIGP64; loconga.
    { 0x2F5F504,0x236F603, 0x16,0x03, 0xA,+0 }, // 208: HMIGP65; HMIGP66; timbale.
    { 0x091F015,0x0E8A617, 0x1E,0x04, 0xE,+0 }, // 209: HMIGP67; HMIGP68; agogo.in
    { 0x001FF0E,0x077780E, 0x06,0x04, 0xE,+0 }, // 210: HMIGP69; HMIGP70; HMIGP82; shaker.i
    { 0x2079F20,0x22B950E, 0x1C,0x00, 0x0,+0 }, // 211: HMIGP71; hiwhist.
    { 0x2079F20,0x23B940E, 0x1E,0x00, 0x0,+0 }, // 212: HMIGP72; lowhist.
    { 0x0F7F020,0x33B8809, 0x00,0x00, 0xC,+0 }, // 213: HMIGP73; higuiro.
    { 0x0F7F420,0x33B560A, 0x03,0x00, 0x0,+0 }, // 214: HMIGP74; loguiro.
    { 0x05BF714,0x089F712, 0x4B,0x00, 0x0,+0 }, // 215: HMIGP75; clavecb.
    { 0x0F2FA27,0x09AF612, 0x22,0x00, 0x0,+0 }, // 216: HMIGP76; HMIGP77; woodblok
    { 0x1F75020,0x03B7708, 0x09,0x05, 0x0,+0 }, // 217: HMIGP78; hicuica.
    { 0x1077F26,0x06B7703, 0x29,0x05, 0x0,+0 }, // 218: HMIGP79; locuica.
    { 0x0F0F126,0x0FCF727, 0x44,0x40, 0x6,+0 }, // 219: HMIGP80; mutringl
    { 0x0F0F126,0x0F5F527, 0x44,0x40, 0x6,+0 }, // 220: HMIGP81; HMIGP83; HMIGP84; triangle
    { 0x0F3F821,0x0ADC620, 0x1C,0x00, 0xC,+0 }, // 221: HMIGP87; taiko.in
    { 0x0FFFF01,0x0FFFF01, 0x3F,0x3F, 0x0,+0 }, // 222: hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; intP0; intP1; intP10; intP100; intP101; intP102; intP103; intP104; intP105; intP106; intP107; intP108; intP109; intP11; intP110; intP111; intP112; intP113; intP114; intP115; intP116; intP117; intP118; intP119; intP12; intP120; intP121; intP122; intP123; intP124; intP125; intP126; intP127; intP13; intP14; intP15; intP16; intP17; intP18; intP19; intP2; intP20; intP21; intP22; intP23; intP24; intP25; intP26; intP3; intP4; intP5; intP6; intP7; intP8; intP9; intP94; intP95; intP96; intP97; intP98; intP99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.
    { 0x4FFEE03,0x0FFE804, 0x80,0x00, 0xC,+0 }, // 223: hamM1; intM1; rickM1; DBlock; DBlock.i
    { 0x122F603,0x0F8F3A1, 0x87,0x80, 0x6,+0 }, // 224: hamM2; intM2; rickM2; GClean; GClean.i
    { 0x007A810,0x005FA00, 0x86,0x03, 0x6,+0 }, // 225: hamM4; intM4; rickM4; DToms; DToms.in
    { 0x053F131,0x227F232, 0x48,0x00, 0x6,+0 }, // 226: hamM7; intM7; rickM7; rickM84; GOverD; GOverD.i; Guit_fz2
    { 0x01A9161,0x01AC1E6, 0x40,0x03, 0x8,+0 }, // 227: hamM8; intM8; rickM8; GMetal; GMetal.i
    { 0x071FB11,0x0B9F301, 0x00,0x00, 0x0,+0 }, // 228: hamM9; intM9; rickM9; BPick; BPick.in
    { 0x1B57231,0x098D523, 0x0B,0x00, 0x8,+0 }, // 229: hamM10; intM10; rickM10; BSlap; BSlap.in
    { 0x024D501,0x0228511, 0x0F,0x00, 0xA,+0 }, // 230: hamM11; intM11; rickM11; BSynth1; BSynth1.
    { 0x025F911,0x034F131, 0x05,0x00, 0xA,+0 }, // 231: hamM12; intM12; rickM12; BSynth2; BSynth2.
    { 0x01576A1,0x0378261, 0x94,0x00, 0xC,+0 }, // 232: hamM15; intM15; rickM15; PSoft; PSoft.in
    { 0x1362261,0x0084F22, 0x10,0x40, 0x8,+0 }, // 233: hamM18; intM18; rickM18; PRonStr1
    { 0x2363360,0x0084F22, 0x15,0x40, 0xC,+0 }, // 234: hamM19; intM19; rickM19; PRonStr2
    { 0x007F804,0x0748201, 0x0E,0x05, 0x6,+0 }, // 235: hamM25; intM25; rickM25; LTrap; LTrap.in
    { 0x0E5F131,0x174F131, 0x89,0x00, 0xC,+0 }, // 236: hamM26; intM26; rickM26; LSaw; LSaw.ins
    { 0x0E3F131,0x073F172, 0x8A,0x00, 0xA,+0 }, // 237: hamM27; intM27; rickM27; PolySyn; PolySyn.
    { 0x0FFF101,0x0FF5091, 0x0D,0x80, 0x6,+0 }, // 238: hamM28; intM28; rickM28; Pobo; Pobo.ins
    { 0x1473161,0x007AF61, 0x0F,0x00, 0xA,+0 }, // 239: hamM29; intM29; rickM29; PSweep2; PSweep2.
    { 0x0D3B303,0x024F204, 0x40,0x80, 0x4,+0 }, // 240: hamM30; intM30; rickM30; LBright; LBright.
    { 0x1037531,0x0445462, 0x1A,0x40, 0xE,+0 }, // 241: hamM31; intM31; rickM31; SynStrin
    { 0x021A1A1,0x116C261, 0x92,0x40, 0x6,+0 }, // 242: hamM32; intM32; rickM32; SynStr2; SynStr2.
    { 0x0F0F240,0x0F4F440, 0x00,0x00, 0x4,+0 }, // 243: hamM33; intM33; rickM33; low_blub
    { 0x003F1C0,0x001107E, 0x4F,0x0C, 0x2,+0 }, // 244: hamM34; intM34; rickM34; DInsect; DInsect.
    { 0x0459BC0,0x015F9C1, 0x05,0x00, 0xE,+0 }, // 245: hamM35; intM35; rickM35; hardshak
    { 0x0064F50,0x003FF50, 0x10,0x00, 0x0,+0 }, // 246: hamM37; intM37; rickM37; WUMP; WUMP.ins
    { 0x2F0F005,0x1B4F600, 0x08,0x00, 0xC,+0 }, // 247: hamM38; intM38; rickM38; DSnare; DSnare.i
    { 0x0F2F931,0x042F210, 0x40,0x00, 0x4,+0 }, // 248: hamM39; intM39; rickM39; DTimp; DTimp.in
    { 0x00FFF7E,0x00F2F6E, 0x00,0x00, 0xE,+0 }, // 249: hamM40; intM40; intP93; rickM40; DRevCym; DRevCym.; drevcym
    { 0x2F95401,0x2FB5401, 0x19,0x00, 0x8,+0 }, // 250: hamM41; intM41; rickM41; Dorky; Dorky.in
    { 0x0665F53,0x0077F00, 0x05,0x00, 0x6,+0 }, // 251: hamM42; intM42; rickM42; DFlab; DFlab.in
    { 0x003F1C0,0x006707E, 0x4F,0x03, 0x2,+0 }, // 252: hamM43; intM43; rickM43; DInsect2
    { 0x1111EF0,0x11411E2, 0x00,0xC0, 0x8,+0 }, // 253: hamM44; intM44; rickM44; DChopper
    { 0x0F0A006,0x075C584, 0x00,0x00, 0xE,+0 }, // 254: hamM45; hamP50; intM45; intP50; rickM45; rickP50; DShot; DShot.in; shot; shot.ins
    { 0x1F5F213,0x0F5F111, 0xC6,0x05, 0x0,+0 }, // 255: hamM46; intM46; rickM46; KickAss; KickAss.
    { 0x153F101,0x274F111, 0x49,0x02, 0x6,+0 }, // 256: hamM47; intM47; rickM47; RVisCool
    { 0x0E4F4D0,0x006A29E, 0x80,0x00, 0x8,+0 }, // 257: hamM48; intM48; rickM48; DSpring; DSpring.
    { 0x0871321,0x0084221, 0xCD,0x80, 0x8,+0 }, // 258: intM49; Chorar22
    { 0x005F010,0x004D011, 0x25,0x80, 0xE,+0 }, // 259: hamP27; intP27; rickP27; timpani; timpani.
    { 0x065B400,0x075B400, 0x00,0x00, 0x7,+0 }, // 260: hamP28; intP28; rickP28; timpanib
    { 0x02AF800,0x145F600, 0x03,0x00, 0x4,+0 }, // 261: hamP29; intP29; rickP29; APS043; APS043.i
    { 0x0FFF830,0x07FF511, 0x44,0x00, 0x8,+0 }, // 262: hamP30; intP30; rickP30; mgun3; mgun3.in
    { 0x0F9F900,0x023F110, 0x08,0x00, 0xA,+0 }, // 263: hamP31; intP31; rickP31; kick4r; kick4r.i
    { 0x0F9F900,0x026F180, 0x04,0x00, 0x8,+0 }, // 264: hamP32; intP32; rickP32; timb1r; timb1r.i
    { 0x1FDF800,0x059F800, 0xC4,0x00, 0xA,+0 }, // 265: hamP33; intP33; rickP33; timb2r; timb2r.i
    { 0x06FFA00,0x08FF600, 0x0B,0x00, 0x0,+0 }, // 266: hamP36; intP36; rickP36; hartbeat
    { 0x0F9F900,0x023F191, 0x04,0x00, 0x8,+0 }, // 267: hamP37; hamP38; hamP39; hamP40; intP37; intP38; intP39; intP40; rickP37; rickP38; rickP39; rickP40; tom1r; tom1r.in
    { 0x097C802,0x097C802, 0x00,0x00, 0x1,+0 }, // 268: hamP71; intP41; intP42; intP43; intP44; intP71; rickP41; rickP42; rickP43; rickP44; rickP71; tom2; tom2.ins; woodbloc
    { 0x0BFFA01,0x0BFDA02, 0x00,0x00, 0x8,+0 }, // 269: MGM113; MGP37; MGP41; MGP43; MGP45; MGP47; MGP48; MGP50; MGP56; MGP60; MGP61; MGP62; MGP63; MGP64; MGP65; MGP66; hamP45; intP45; rickP45; Agogo Bells; Cow Bell; High Bongo; High Floor Tom; High Timbale; High Tom; High-Mid Tom; Low Bongo; Low Conga; Low Floor Tom; Low Timbale; Low Tom; Low-Mid Tom; Mute High Conga; Open High Conga; Side Stick; tom; tom.ins
    { 0x2F0FB01,0x096F701, 0x10,0x00, 0xE,+0 }, // 270: hamP46; hamP47; intP46; intP47; rickP46; rickP47; conga; conga.in
    { 0x002FF04,0x007FF00, 0x00,0x00, 0xE,+0 }, // 271: hamP48; intP48; rickP48; snare01r
    { 0x0F0F006,0x0B7F600, 0x00,0x00, 0xC,+0 }, // 272: hamP49; intP49; rickP49; slap; slap.ins
    { 0x0F0F006,0x034C4C4, 0x00,0x03, 0xE,+0 }, // 273: hamP51; intP51; rickP51; snrsust; snrsust.
    { 0x0F0F019,0x0F7B720, 0x0E,0x0A, 0xE,+0 }, // 274: intP52; rickP52; snare; snare.in
    { 0x0F0F006,0x0B4F600, 0x00,0x00, 0xE,+0 }, // 275: MGM114; MGM117; MGM118; hamP53; intP53; oGM118; rickP53; Melodic Tom; Steel Drums; Synth Drum; synsnar; synsnar.
    { 0x0F0F006,0x0B6F800, 0x00,0x00, 0xE,+0 }, // 276: MGM115; MGM116; hamP54; intP54; rickP54; Taiko Drum; Woodblock; synsnr1; synsnr1.
    { 0x0F2F931,0x008F210, 0x40,0x00, 0x4,+0 }, // 277: hamP56; intP56; rickP56; rimshotb
    { 0x0BFFA01,0x0BFDA09, 0x00,0x08, 0x8,+0 }, // 278: hamP57; intP57; rickP57; rimshot; rimshot.
    { 0x210BA2E,0x2F4B40E, 0x0E,0x00, 0xE,+0 }, // 279: hamP58; hamP59; intP58; intP59; rickP58; rickP59; crash; crash.in
    { 0x210FA2E,0x2F4F40E, 0x0E,0x00, 0xE,+0 }, // 280: intP60; rickP60; cymbal; cymbal.i
    { 0x2A2B2A4,0x1D49703, 0x02,0x80, 0xE,+0 }, // 281: hamP60; hamP61; intP61; rickP61; cymbal; cymbals; cymbals.
    { 0x200FF04,0x206FFC3, 0x00,0x00, 0x8,+0 }, // 282: hamP62; intP62; rickP62; hammer5r
    { 0x200FF04,0x2F5F6C3, 0x00,0x00, 0x8,+0 }, // 283: hamP63; hamP64; intP63; intP64; rickP63; rickP64; hammer3; hammer3.
    { 0x0E1C000,0x153951E, 0x80,0x80, 0x6,+0 }, // 284: hamP65; intP65; rickP65; ride2; ride2.in
    { 0x200FF03,0x3F6F6C4, 0x00,0x00, 0x8,+0 }, // 285: hamP66; intP66; rickP66; hammer1; hammer1.
    { 0x202FF4E,0x3F7F701, 0x00,0x00, 0x8,+0 }, // 286: intP67; intP68; rickP67; rickP68; tambour; tambour.
    { 0x202FF4E,0x3F6F601, 0x00,0x00, 0x8,+0 }, // 287: hamP69; hamP70; intP69; intP70; rickP69; rickP70; tambou2; tambou2.
    { 0x2588A51,0x018A452, 0x00,0x00, 0xC,+0 }, // 288: hamP72; intP72; rickP72; woodblok
    { 0x0FFFB13,0x0FFE808, 0x40,0x00, 0x8,+0 }, // 289: intP73; intP74; rickP73; rickP74; claves; claves.i
    { 0x0FFEE03,0x0FFE808, 0x40,0x00, 0xC,+0 }, // 290: hamP75; intP75; rickP75; claves2; claves2.
    { 0x0FFEE05,0x0FFE808, 0x55,0x00, 0xE,+0 }, // 291: hamP76; intP76; rickP76; claves3; claves3.
    { 0x0FF0006,0x0FDF715, 0x3F,0x0D, 0x1,+0 }, // 292: hamP77; intP77; rickP77; clave; clave.in
    { 0x0F6F80E,0x0F6F80E, 0x00,0x00, 0x0,+0 }, // 293: hamP78; hamP79; intP78; intP79; rickP78; rickP79; agogob4; agogob4.
    { 0x060F207,0x072F212, 0x4F,0x09, 0x8,+0 }, // 294: hamP80; intP80; rickP80; clarion; clarion.
    { 0x061F217,0x074F212, 0x4F,0x08, 0x8,+0 }, // 295: MGM38; hamP81; intP81; rickP81; Synth Bass 1; trainbel
    { 0x022FB18,0x012F425, 0x88,0x80, 0x8,+0 }, // 296: hamP82; intP82; rickP82; gong; gong.ins
    { 0x0F0FF04,0x0B5F4C1, 0x00,0x00, 0xE,+0 }, // 297: hamP83; intP83; rickP83; kalimbai
    { 0x02FC811,0x0F5F531, 0x2D,0x00, 0xC,+0 }, // 298: MGM103; hamP84; hamP85; intP84; intP85; oGM103; rickP84; rickP85; FX 8 sci-fi; xylo1; xylo1.in
    { 0x03D6709,0x3FC692C, 0x00,0x00, 0xE,+0 }, // 299: hamP86; intP86; rickP86; match; match.in
    { 0x053D144,0x05642B2, 0x80,0x15, 0xE,+0 }, // 300: hamP87; intP87; rickP87; breathi; breathi.
    { 0x0F0F007,0x0DC5C00, 0x00,0x00, 0xE,+0 }, // 301: intP88; rickP88; scratch; scratch.
    { 0x00FFF7E,0x00F3F6E, 0x00,0x00, 0xE,+0 }, // 302: hamP89; intP89; rickP89; crowd; crowd.in
    { 0x0B3FA00,0x005D000, 0x00,0x00, 0xC,+0 }, // 303: intP90; rickP90; taiko; taiko.in
    { 0x0FFF832,0x07FF511, 0x84,0x00, 0xE,+0 }, // 304: hamP91; intP91; rickP91; rlog; rlog.ins
    { 0x0089FD4,0x0089FD4, 0xC0,0xC0, 0x4,+0 }, // 305: hamP92; intP92; rickP92; knock; knock.in
    { 0x253B1C4,0x083B1D2, 0x8F,0x84, 0x2,+0 }, // 306: hamM52; rickM94; Fantasy1; fantasy1
    { 0x175F5C2,0x074F2D1, 0x21,0x83, 0xE,+0 }, // 307: hamM53; guitar1
    { 0x1F6FB34,0x04394B1, 0x83,0x00, 0xC,+0 }, // 308: hamM55; hamatmos
    { 0x0658722,0x0186421, 0x46,0x80, 0x0,+0 }, // 309: hamM56; hamcalio
    { 0x0BDF211,0x09BA004, 0x46,0x40, 0x8,+0 }, // 310: MGM37; hamM59; oGM37; Slap Bass 2; moon
    { 0x144F221,0x3457122, 0x8A,0x40, 0x0,+0 }, // 311: hamM62; Polyham3
    { 0x144F221,0x1447122, 0x8A,0x40, 0x0,+0 }, // 312: hamM63; Polyham
    { 0x053F101,0x153F108, 0x40,0x40, 0x0,+0 }, // 313: hamM64; sitar2
    { 0x102FF00,0x3FFF200, 0x08,0x00, 0x0,+0 }, // 314: hamM70; weird1a
    { 0x144F221,0x345A122, 0x8A,0x40, 0x0,+0 }, // 315: hamM71; Polyham4
    { 0x028F131,0x018F031, 0x0F,0x00, 0xA,+0 }, // 316: hamM72; hamsynbs
    { 0x307D7E1,0x107B6E0, 0x8D,0x00, 0x1,+0 }, // 317: hamM73; Ocasynth
    { 0x03DD500,0x02CD500, 0x11,0x00, 0xA,+0 }, // 318: hamM120; hambass1
    { 0x1199563,0x219C420, 0x46,0x00, 0x8,+0 }, // 319: hamM121; hamguit1
    { 0x044D08C,0x2F4D181, 0xA1,0x80, 0x8,+0 }, // 320: hamM122; hamharm2
    { 0x0022171,0x1035231, 0x93,0x80, 0x0,+0 }, // 321: hamM123; hamvox1
    { 0x1611161,0x01311A2, 0x91,0x80, 0x8,+0 }, // 322: hamM124; hamgob1
    { 0x25666E1,0x02665A1, 0x4C,0x00, 0x0,+0 }, // 323: hamM125; hamblow1
    { 0x144F5C6,0x018F6C1, 0x5C,0x83, 0xE,+0 }, // 324: hamP9; cowbell
    { 0x0D0CCC0,0x028EAC1, 0x10,0x00, 0x0,+0 }, // 325: hamP10; rickP100; conghi; conghi.i
    { 0x038FB00,0x0DAF400, 0x00,0x00, 0x4,+0 }, // 326: hamP12; rickP15; hamkick; kick3.in
    { 0x2BFFB15,0x31FF817, 0x0A,0x00, 0x0,+0 }, // 327: hamP13; rimshot2
    { 0x0F0F01E,0x0B6F70E, 0x00,0x00, 0xE,+0 }, // 328: hamP14; rickP16; hamsnr1; snr1.ins
    { 0x0BFFBC6,0x02FE8C9, 0x00,0x00, 0xE,+0 }, // 329: hamP15; handclap
    { 0x2F0F006,0x2B7F800, 0x00,0x00, 0xE,+0 }, // 330: hamP16; smallsnr
    { 0x0B0900E,0x0BF990E, 0x03,0x03, 0xA,+0 }, // 331: hamP17; rickP95; clsdhhat
    { 0x283E0C4,0x14588C0, 0x81,0x00, 0xE,+0 }, // 332: hamP18; rickP94; openhht2
    { 0x097C802,0x040E000, 0x00,0x00, 0x1,+0 }, // 333: hamP41; hamP42; hamP43; hamP44; tom2
    { 0x00FFF2E,0x04AF602, 0x0A,0x1B, 0xE,+0 }, // 334: hamP52; snare
    { 0x3A5F0EE,0x36786CE, 0x00,0x00, 0xE,+0 }, // 335: hamP67; hamP68; tambour
    { 0x0B0FCD6,0x008BDD6, 0x00,0x05, 0xA,+0 }, // 336: hamP73; hamP74; claves
    { 0x0F0F007,0x0DC5C00, 0x08,0x00, 0xE,+0 }, // 337: hamP88; scratch
    { 0x0E7F301,0x078F211, 0x58,0x00, 0xA,+0 }, // 338: rickM76; Bass.ins
    { 0x0EFF230,0x078F521, 0x1E,0x00, 0xE,+0 }, // 339: rickM77; Basnor04
    { 0x019D530,0x01B6171, 0x88,0x80, 0xC,+0 }, // 340: rickM78; Synbass1
    { 0x001F201,0x0B7F211, 0x0D,0x0D, 0xA,+0 }, // 341: rickM79; Synbass2
    { 0x03DD500,0x02CD500, 0x14,0x00, 0xA,+0 }, // 342: rickM80; Pickbass
    { 0x010E032,0x0337D16, 0x87,0x84, 0x8,+0 }, // 343: rickM82; Harpsi1.
    { 0x0F8F161,0x008F062, 0x80,0x80, 0x8,+0 }, // 344: rickM83; Guit_el3
    { 0x0745391,0x0755451, 0x00,0x00, 0xA,+0 }, // 345: rickM88; Orchit2.
    { 0x08E6121,0x09E7231, 0x15,0x00, 0xE,+0 }, // 346: rickM89; Brass11.
    { 0x0BC7321,0x0BC8121, 0x19,0x00, 0xE,+0 }, // 347: rickM90; Brass2.i
    { 0x23C7320,0x0BC8121, 0x19,0x00, 0xE,+0 }, // 348: rickM91; Brass3.i
    { 0x209A060,0x20FF014, 0x02,0x80, 0x1,+0 }, // 349: rickM92; Squ_wave
    { 0x064F207,0x075F612, 0x73,0x00, 0x8,+0 }, // 350: rickM99; Agogo.in
    { 0x2B7F811,0x006F311, 0x46,0x00, 0x8,+0 }, // 351: oGM113; oGP35; oGP36; rickP13; Ac Bass Drum; Agogo Bells; Bass Drum 1; kick1.in
    { 0x218F401,0x008F800, 0x00,0x00, 0xC,+0 }, // 352: rickP17; rickP19; snare1.i; snare4.i
    { 0x0F0F009,0x0F7B700, 0x0E,0x00, 0xE,+0 }, // 353: rickP18; rickP20; snare2.i; snare5.i
    { 0x0FEF812,0x07ED511, 0x47,0x00, 0xE,+0 }, // 354: rickP21; rickP22; rickP23; rickP24; rickP25; rickP26; rocktom.
    { 0x2F4F50E,0x424120CA, 0x00,0x51, 0x3,+0 }, // 355: rickP93; openhht1
    { 0x0DFDCC2,0x026C9C0, 0x17,0x00, 0x0,+0 }, // 356: rickP97; guirol.i
    { 0x0D0ACC0,0x028EAC1, 0x18,0x00, 0x0,+0 }, // 357: rickP101; rickP102; congas2.
    { 0x0A7CDC2,0x028EAC1, 0x2B,0x02, 0x0,+0 }, // 358: rickP103; rickP104; bongos.i
    { 0x1F3F030,0x1F4F130, 0x54,0x00, 0xA,+12 }, // 359: dMM0; hxMM0; Acoustic Grand Piano
    { 0x0F3F030,0x1F4F130, 0x52,0x00, 0xA,+12 }, // 360: dMM1; hxMM1; Bright Acoustic Piano
    { 0x1F3E130,0x0F4F130, 0x4E,0x00, 0x8,+12 }, // 361: dMM2; hxMM2; Electric Grand Piano
    { 0x015E811,0x014F712, 0x00,0x00, 0x1,+12 }, // 362: dMM2; hxMM2; Electric Grand Piano
    { 0x153F110,0x0F4D110, 0x4F,0x00, 0x6,+12 }, // 363: dMM3; hxMM3; Honky-tonk Piano
    { 0x053F111,0x0F4D111, 0x4F,0x00, 0x6,+12 }, // 364: dMM3; hxMM3; Honky-tonk Piano
    { 0x051F121,0x0E5D231, 0x66,0x00, 0x6,+0 }, // 365: dMM4; hxMM4; Rhodes Paino
    { 0x0E6F130,0x0E5F1B0, 0x51,0x40, 0x6,+12 }, // 366: dMM5; hxMM5; Chorused Piano
    { 0x079F212,0x099F110, 0x43,0x40, 0x9,+12 }, // 367: dMM5; hxMM5; Chorused Piano
    { 0x201F230,0x1F4C130, 0x87,0x00, 0x6,+12 }, // 368: dMM6; hxMM6; Harpsichord
    { 0x162A190,0x1A79110, 0x8E,0x00, 0xC,+12 }, // 369: dMM7; hxMM7; Clavinet
    { 0x164F228,0x0E4F231, 0x4F,0x00, 0x8,+0 }, // 370: dMM8; hxMM8; Celesta
    { 0x0119113,0x0347D14, 0x0E,0x00, 0x9,+0 }, // 371: dMM9; hxMM9; * Glockenspiel
    { 0x041F6B2,0x092D290, 0x0F,0x00, 0x0,+12 }, // 372: dMM10; hxMM10; * Music Box
    { 0x0F3F1F0,0x0F4F1F2, 0x02,0x00, 0x1,+12 }, // 373: dMM11; hxMM11; Vibraphone
    { 0x0157980,0x275F883, 0x00,0x00, 0x1,+12 }, // 374: dMM12; hxMM12; Marimba
    { 0x093F614,0x053F610, 0x1F,0x00, 0x8,+12 }, // 375: dMM13; hxMM13; Xylophone
    { 0x113B681,0x013FF02, 0x99,0x00, 0xA,+0 }, // 376: dMM14; hxMM14; * Tubular-bell
    { 0x0119130,0x0535211, 0x47,0x80, 0x8,+12 }, // 377: dMM15; hxMM15; * Dulcimer
    { 0x016B1A0,0x117D161, 0x88,0x80, 0x7,+12 }, // 378: dMM16; hxMM16; Hammond Organ
    { 0x105F130,0x036F494, 0x00,0x00, 0x7,+0 }, // 379: dMM17; hxMM17; Percussive Organ
    { 0x017F2E2,0x107FF60, 0x9E,0x80, 0x0,+0 }, // 380: dMM18; hxMM18; Rock Organ
    { 0x117F2E0,0x007FFA0, 0x9E,0x80, 0x0,+12 }, // 381: dMM18; hxMM18; Rock Organ
    { 0x0043030,0x1145431, 0x92,0x80, 0x9,+12 }, // 382: dMM19; hxMM19; Church Organ
    { 0x0178000,0x1176081, 0x49,0x80, 0x6,+12 }, // 383: dMM20; hxMM20; Reed Organ
    { 0x015A220,0x1264131, 0x48,0x00, 0xA,+12 }, // 384: dMM21; hxMM21; Accordion
    { 0x0158220,0x1264631, 0x4A,0x00, 0xA,+12 }, // 385: dMM21; hxMM21; Accordion
    { 0x03460B0,0x01642B2, 0x0C,0x80, 0x8,+12 }, // 386: dMM22; hxMM22; Harmonica
    { 0x105F020,0x2055231, 0x92,0x00, 0x8,+12 }, // 387: dMM23; hxMM23; Tango Accordion
    { 0x105F020,0x2055231, 0x92,0x00, 0x0,+12 }, // 388: dMM23; hxMM23; Tango Accordion
    { 0x0F5F120,0x0F6F120, 0x8D,0x00, 0x0,+12 }, // 389: dMM24; hxMM24; Acoustic Guitar (nylon)
    { 0x1E4E130,0x0E3F230, 0x0D,0x00, 0xA,+12 }, // 390: dMM25; hxMM25; Acoustic Guitar (steel)
    { 0x21FF100,0x088F400, 0x21,0x00, 0xA,+12 }, // 391: dMM26; hxMM26; Electric Guitar (jazz)
    { 0x132EA10,0x2E7D210, 0x87,0x00, 0x2,+12 }, // 392: dMM27; hxMM27; * Electric Guitar (clean)
    { 0x0F4E030,0x0F5F230, 0x92,0x80, 0x0,+12 }, // 393: dMM28; hxMM28; Electric Guitar (muted)
    { 0x0FFF100,0x1FFF051, 0x10,0x00, 0xA,+12 }, // 394: dMM29; Overdriven Guitar
    { 0x0FFF110,0x1FFF051, 0x0D,0x00, 0xC,+12 }, // 395: dMM30; Distortion Guitar
    { 0x297A110,0x0E7E111, 0x43,0x00, 0x0,+12 }, // 396: dMM31; hxMM31; * Guitar Harmonics
    { 0x020C420,0x0F6C3B0, 0x0E,0x00, 0x0,+12 }, // 397: dMM32; Acoustic Bass
    { 0x0FFF030,0x0F8F131, 0x96,0x00, 0xA,+12 }, // 398: dMM33; hxMM33; Electric Bass (finger)
    { 0x014E020,0x0D6E130, 0x8F,0x80, 0x8,+12 }, // 399: dMM34; hxMM34; Electric Bass (pick)
    { 0x14551E1,0x14691A0, 0x4D,0x00, 0x0,+0 }, // 400: dMM35; Fretless Bass
    { 0x14551A1,0x14681A0, 0x4D,0x00, 0x0,+12 }, // 401: dMM35; dMM94; hxMM94; Fretless Bass
    { 0x2E7F030,0x047F131, 0x00,0x00, 0x0,+0 }, // 402: dMM36; * Slap Bass 1
    { 0x0E5F030,0x0F5F131, 0x90,0x80, 0x8,+12 }, // 403: dMM37; hxMM37; Slap Bass 2
    { 0x1F5F430,0x0F6F330, 0x0A,0x00, 0xA,+12 }, // 404: dMM38; hxMM38; Synth Bass 1
    { 0x1468330,0x017D231, 0x15,0x00, 0xA,+12 }, // 405: dMM39; hxMM39; Synth Bass 2
    { 0x1455060,0x14661A1, 0x17,0x00, 0x6,+12 }, // 406: dMM40; hxMM40; Violin
    { 0x04460F0,0x0154171, 0x8F,0x00, 0x2,+12 }, // 407: dMM41; hxMM41; Viola
    { 0x214D0B0,0x1176261, 0x0F,0x80, 0x6,+0 }, // 408: dMM42; hxMM42; Cello
    { 0x211B1F0,0x115A020, 0x8A,0x80, 0x6,+12 }, // 409: dMM43; hxMM43; Contrabass
    { 0x201C3F0,0x0058361, 0x89,0x40, 0x6,+0 }, // 410: dMM44; hxMM44; Tremolo Strings
    { 0x201B370,0x1059360, 0x89,0x40, 0x6,+12 }, // 411: dMM44; hxMM44; Tremolo Strings
    { 0x2F9F830,0x0E67620, 0x97,0x00, 0xE,+12 }, // 412: dMM45; hxMM45; Pizzicato Strings
    { 0x035F131,0x0B3F320, 0x24,0x00, 0x0,+12 }, // 413: dMM46; hxMM46; Orchestral Harp
    { 0x0C8AA00,0x0B3D210, 0x04,0x00, 0xA,+12 }, // 414: dMM47; hxMM47; * Timpani
    { 0x104C060,0x10455B1, 0x51,0x80, 0x4,+12 }, // 415: dMM48; hxMM48; String Ensemble 1
    { 0x10490A0,0x1045531, 0x52,0x80, 0x6,+12 }, // 416: dMM48; hxMM48; String Ensemble 1
    { 0x1059020,0x10535A1, 0x51,0x80, 0x4,+12 }, // 417: dMM49; hxMM49; String Ensemble 2
    { 0x10590A0,0x1053521, 0x52,0x80, 0x6,+12 }, // 418: dMM49; hxMM49; String Ensemble 2
    { 0x20569A1,0x20266F1, 0x93,0x00, 0xA,+0 }, // 419: dMM50; hxMM50; Synth Strings 1
    { 0x0031121,0x1043120, 0x4D,0x80, 0x0,+12 }, // 420: dMM51; hxMM51; Synth Strings 2
    { 0x2331100,0x1363100, 0x82,0x80, 0x8,+12 }, // 421: dMM51; hxMM51; Synth Strings 2
    { 0x0549060,0x0047060, 0x56,0x40, 0x0,+12 }, // 422: dMM52; hxMM52; Choir Aahs
    { 0x0549020,0x0047060, 0x92,0xC0, 0x0,+12 }, // 423: dMM52; hxMM52; Choir Aahs
    { 0x0B7B1A0,0x08572A0, 0x99,0x80, 0x0,+12 }, // 424: dMM53; hxMM53; Voice Oohs
    { 0x05460B0,0x07430B0, 0x5A,0x80, 0x0,+12 }, // 425: dMM54; hxMM54; Synth Voice
    { 0x0433010,0x0146410, 0x90,0x00, 0x2,-12 }, // 426: dMM55; hxMM55; Orchestra Hit
    { 0x0425090,0x0455411, 0x8F,0x00, 0x2,+0 }, // 427: dMM55; hxMM55; Orchestra Hit
    { 0x1158020,0x0365130, 0x8E,0x00, 0xA,+12 }, // 428: dMM56; hxMM56; Trumpet
    { 0x01F71B0,0x03B7220, 0x1A,0x80, 0xE,+12 }, // 429: dMM57; hxMM57; Trombone
    { 0x0468020,0x1569220, 0x16,0x00, 0xC,+12 }, // 430: dMM58; Tuba
    { 0x1E68080,0x1F65190, 0x8D,0x00, 0xC,+12 }, // 431: dMM59; hxMM59; Muted Trumpet
    { 0x0B87020,0x0966120, 0x22,0x80, 0xE,+12 }, // 432: dMM60; hxMM60; French Horn
    { 0x0B87020,0x0966120, 0x23,0x80, 0xE,+12 }, // 433: dMM60; hxMM60; French Horn
    { 0x1156020,0x0365130, 0x8E,0x00, 0xA,+12 }, // 434: dMM61; hxMM61; Brass Section
    { 0x1177030,0x1366130, 0x92,0x00, 0xE,+12 }, // 435: dMM61; hxMM61; Brass Section
    { 0x2A69120,0x1978120, 0x4D,0x00, 0xC,+12 }, // 436: dMM62; hxMM62; Synth Brass 1
    { 0x2A69120,0x1979120, 0x8C,0x00, 0xC,+12 }, // 437: dMM62; hxMM62; Synth Brass 1
    { 0x2A68130,0x1976130, 0x50,0x00, 0xC,+12 }, // 438: dMM63; hxMM63; Synth Bass 2
    { 0x2A68130,0x1976130, 0x4A,0x00, 0xA,+12 }, // 439: dMM63; hxMM63; Synth Bass 2
    { 0x00560A0,0x11652B1, 0x96,0x00, 0x6,+12 }, // 440: dMM64; hxMM64; Soprano Sax
    { 0x10670A0,0x11662B0, 0x89,0x00, 0x6,+12 }, // 441: dMM65; hxMM65; Alto Sax
    { 0x00B98A0,0x10B73B0, 0x4A,0x00, 0xA,+12 }, // 442: dMM66; hxMM66; Tenor Sax
    { 0x10B90A0,0x11B63B0, 0x85,0x00, 0xA,+12 }, // 443: dMM67; hxMM67; Baritone Sax
    { 0x0167070,0x0085CA2, 0x90,0x80, 0x6,+12 }, // 444: dMM68; hxMM68; Oboe
    { 0x007C820,0x1077331, 0x4F,0x00, 0xA,+12 }, // 445: dMM69; hxMM69; English Horn
    { 0x0199030,0x01B6131, 0x91,0x80, 0xA,+12 }, // 446: dMM70; hxMM70; Bassoon
    { 0x017A530,0x01763B0, 0x8D,0x80, 0x8,+12 }, // 447: dMM71; hxMM71; Clarinet
    { 0x08F6EF0,0x02A3570, 0x80,0x00, 0xE,+12 }, // 448: dMM72; hxMM72; Piccolo
    { 0x08850A0,0x02A5560, 0x93,0x80, 0x8,+12 }, // 449: dMM73; hxMM73; Flute
    { 0x0176520,0x02774A0, 0x0A,0x00, 0xB,+12 }, // 450: dMM74; hxMM74; Recorder
    { 0x12724B0,0x01745B0, 0x84,0x00, 0x9,+12 }, // 451: dMM75; hxMM75; Pan Flute
    { 0x00457E1,0x0375760, 0xAD,0x00, 0xE,+12 }, // 452: dMM76; hxMM76; Bottle Blow
    { 0x33457F1,0x05D67E1, 0x28,0x00, 0xE,+0 }, // 453: dMM77; hxMM77; * Shakuhachi
    { 0x00F31D0,0x0053270, 0xC7,0x00, 0xB,+12 }, // 454: dMM78; hxMM78; Whistle
    { 0x00551B0,0x0294230, 0xC7,0x00, 0xB,+12 }, // 455: dMM79; hxMM79; Ocarina
    { 0x15B5122,0x1256030, 0x52,0x00, 0x0,+12 }, // 456: dMM80; hxMM80; Lead 1 (square)
    { 0x15B9122,0x125F030, 0x4D,0x00, 0x0,+12 }, // 457: dMM80; hxMM80; Lead 1 (square)
    { 0x19BC120,0x165C031, 0x43,0x00, 0x8,+12 }, // 458: dMM81; hxMM81; Lead 2 (sawtooth)
    { 0x1ABB160,0x005F131, 0x41,0x00, 0x8,+12 }, // 459: dMM81; hxMM81; Lead 2 (sawtooth)
    { 0x33357F0,0x00767E0, 0x28,0x00, 0xE,+12 }, // 460: dMM82; hxMM82; Lead 3 (calliope)
    { 0x30457E0,0x04D67E0, 0x23,0x00, 0xE,+12 }, // 461: dMM83; hxMM83; Lead 4 (chiffer)
    { 0x304F7E0,0x04D87E0, 0x23,0x00, 0xE,+12 }, // 462: dMM83; hxMM83; Lead 4 (chiffer)
    { 0x10B78A1,0x12BF130, 0x42,0x00, 0x8,+12 }, // 463: dMM84; hxMM84; Lead 5 (charang)
    { 0x0558060,0x014F2E0, 0x21,0x00, 0x8,+12 }, // 464: dMM85; hxMM85; Lead 6 (voice)
    { 0x0559020,0x014A2A0, 0x21,0x00, 0x8,+12 }, // 465: dMM85; hxMM85; Lead 6 (voice)
    { 0x195C120,0x16370B0, 0x43,0x80, 0xA,+12 }, // 466: dMM86; hxMM86; Lead 7 (5th sawtooth)
    { 0x19591A0,0x1636131, 0x49,0x00, 0xA,+7 }, // 467: dMM86; hxMM86; Lead 7 (5th sawtooth)
    { 0x1075124,0x229FDA0, 0x40,0x00, 0x9,+0 }, // 468: dMM87; dMM88; hxMM87; hxMM88; * Lead 8 (bass & lead)
    { 0x0053280,0x0053360, 0xC0,0x00, 0x9,+12 }, // 469: dMM89; hxMM89; Pad 2 (warm)
    { 0x0053240,0x00533E0, 0x40,0x00, 0x9,+12 }, // 470: dMM89; hxMM89; Pad 2 (warm)
    { 0x2A5A1A0,0x196A1A0, 0x8F,0x00, 0xC,+12 }, // 471: dMM90; hxMM90; Pad 3 (polysynth)
    { 0x005F0E0,0x0548160, 0x44,0x00, 0x1,+12 }, // 472: dMM91; hxMM91; Pad 4 (choir)
    { 0x105F0E0,0x0547160, 0x44,0x80, 0x1,+12 }, // 473: dMM91; hxMM91; Pad 4 (choir)
    { 0x033A180,0x05452E0, 0x8A,0x00, 0x7,+12 }, // 474: dMM92; hxMM92; Pad 5 (bowed glass)
    { 0x1528081,0x1532340, 0x9D,0x80, 0xE,+12 }, // 475: dMM93; hxMM93; Pad 6 (metal)
    { 0x14551E1,0x14691A0, 0x4D,0x00, 0x0,+12 }, // 476: dMM94; hxMM94; Pad 7 (halo)
    { 0x15211E1,0x17380E0, 0x8C,0x80, 0x8,+12 }, // 477: dMM95; hxMM95; Pad 8 (sweep)
    { 0x0477220,0x019F883, 0x40,0x00, 0xB,+12 }, // 478: dMM96; hxMM96; FX 1 (rain)
    { 0x1028500,0x11245C1, 0xD2,0x00, 0xA,+0 }, // 479: dMM97; hxMM97; FX 2 (soundtrack)
    { 0x0034522,0x23535E3, 0xD2,0x00, 0xA,+7 }, // 480: dMM97; hxMM97; FX 2 (soundtrack)
    { 0x074F604,0x024A302, 0xC0,0x00, 0x0,-12 }, // 481: dMM98; hxMM98; * FX 3 (crystal)
    { 0x0D2C090,0x0D2D130, 0x8E,0x00, 0x0,+12 }, // 482: dMM99; hxMM99; FX 4 (atmosphere)
    { 0x0D2D090,0x0D2F130, 0x8E,0x00, 0x0,+12 }, // 483: dMM99; hxMM99; FX 4 (atmosphere)
    { 0x0F390D0,0x0F3C2C0, 0x12,0x00, 0x0,+12 }, // 484: dMM100; hxMM100; FX 5 (brightness)
    { 0x0F390D0,0x0F2C2C0, 0x12,0x80, 0x0,+12 }, // 485: dMM100; hxMM100; FX 5 (brightness)
    { 0x15213E0,0x21333F1, 0x1A,0x80, 0x0,+0 }, // 486: dMM101; hxMM101; FX 6 (goblin)
    { 0x0BA45E0,0x19132F0, 0x1A,0x00, 0x0,+12 }, // 487: dMM102; hxMM102; FX 7 (echo drops)
    { 0x1025810,0x0724202, 0x18,0x00, 0xA,+12 }, // 488: dMM103; hxMM103; * FX 8 (star-theme)
    { 0x0B36320,0x0B36324, 0x08,0x00, 0x2,+12 }, // 489: dMM104; hxMM104; Sitar
    { 0x0127730,0x1F4F310, 0x0D,0x00, 0x4,+12 }, // 490: dMM105; hxMM105; Banjo
    { 0x033F900,0x273F400, 0x80,0x80, 0x0,+12 }, // 491: dMM106; hxMM106; Shamisen
    { 0x2ACF907,0x229F90F, 0x1A,0x00, 0x0,+12 }, // 492: dMM106; hxMM106; Shamisen
    { 0x153F220,0x0E49122, 0x21,0x00, 0x8,+12 }, // 493: dMM107; hxMM107; Koto
    { 0x339F103,0x074D615, 0x4F,0x00, 0x6,+0 }, // 494: dMM108; hxMM108; Kalimba
    { 0x1158930,0x2076B21, 0x42,0x00, 0xA,+12 }, // 495: dMM109; hxMM109; Bag Pipe
    { 0x003A130,0x0265221, 0x1F,0x00, 0xE,+12 }, // 496: dMM110; hxMM110; Fiddle
    { 0x0134030,0x1166130, 0x13,0x80, 0x8,+12 }, // 497: dMM111; hxMM111; Shanai
    { 0x032A113,0x172B212, 0x00,0x80, 0x1,+5 }, // 498: dMM112; hxMM112; Tinkle Bell
    { 0x001E795,0x0679616, 0x81,0x00, 0x4,+12 }, // 499: dMM113; hxMM113; Agogo
    { 0x104F003,0x0058220, 0x49,0x00, 0x6,+12 }, // 500: dMM114; hxMM114; Steel Drums
    { 0x0D1F813,0x078F512, 0x44,0x00, 0x6,+12 }, // 501: dMM115; hxMM115; Woodblock
    { 0x0ECA710,0x0F5D510, 0x0B,0x00, 0x0,+0 }, // 502: dMM116; hxMM116; Taiko Drum
    { 0x0C8A820,0x0B7D601, 0x0B,0x00, 0x0,+0 }, // 503: dMM117; hxMM117; Melodic Tom
    { 0x0C4F800,0x0B7D300, 0x0B,0x00, 0x0,+12 }, // 504: dMM118; hxMM118; Synth Drum
    { 0x031410C,0x31D2110, 0x8F,0x80, 0xE,+0 }, // 505: dMM119; hxMM119; Reverse Cymbal
    { 0x1B33432,0x3F75431, 0x21,0x00, 0xE,+12 }, // 506: dMM120; hxMM120; Guitar Fret Noise
    { 0x00437D1,0x0343750, 0xAD,0x00, 0xE,+12 }, // 507: dMM121; hxMM121; Breath Noise
    { 0x2013E02,0x2F31408, 0x00,0x00, 0xE,+0 }, // 508: dMM122; hxMM122; Seashore
    { 0x003EBF5,0x06845F6, 0xD4,0x00, 0x7,+0 }, // 509: dMM123; hxMM123; Bird Tweet
    { 0x171DAF0,0x117B0CA, 0x00,0xC0, 0x8,+0 }, // 510: dMM124; hxMM124; Telephone Ring
    { 0x1111EF0,0x11121E2, 0x00,0xC0, 0x8,-12 }, // 511: dMM125; hxMM125; Helicopter
    { 0x20053EF,0x30210EF, 0x86,0xC0, 0xE,+12 }, // 512: dMM126; hxMM126; Applause
    { 0x2F0F00C,0x0E6F604, 0x00,0x00, 0xE,+0 }, // 513: dMM127; hxMM127; Gun Shot
    { 0x257F900,0x046FB00, 0x00,0x00, 0x0,+12 }, // 514: dMP35; hxMP35; Acoustic Bass Drum
    { 0x047FA00,0x006F900, 0x00,0x00, 0x6,+12 }, // 515: dMP36; hxMP36; Acoustic Bass Drum
    { 0x067FD02,0x078F703, 0x80,0x00, 0x6,+12 }, // 516: dMP37; hxMP37; Slide Stick
    { 0x214F70F,0x247F900, 0x05,0x00, 0xE,+12 }, // 517: dMP38; hxMP38; Acoustic Snare
    { 0x3FB88E1,0x2A8A6FF, 0x00,0x00, 0xF,+12 }, // 518: dMP39; hxMP39; Hand Clap
    { 0x0FFAA06,0x0FAF700, 0x00,0x00, 0xE,+12 }, // 519: dMP40; Electric Snare
    { 0x0F00000,0x0F00000, 0x3F,0x00, 0x0,+54 }, // 520: dMP40; Electric Snare
    { 0x06CF502,0x138F703, 0x00,0x00, 0x7,+12 }, // 521: dMP41; hxMP41; Low Floor Tom
    { 0x25E980C,0x306FB0F, 0x00,0x00, 0xF,+12 }, // 522: dMP42; hxMP42; Closed High-Hat
    { 0x078F502,0x137F700, 0x00,0x00, 0x7,+12 }, // 523: dMP43; hxMP43; High Floor Tom
    { 0x25E780C,0x32B8A0A, 0x00,0x80, 0xF,+12 }, // 524: dMP44; dMP69; hxMP44; hxMP69; Cabasa
    { 0x037F502,0x137F702, 0x00,0x00, 0x3,+12 }, // 525: dMP45; dMP47; dMP48; dMP50; hxMP45; hxMP47; hxMP48; hxMP50; High Tom
    { 0x201C700,0x233F90B, 0x45,0x00, 0xE,+12 }, // 526: dMP46; hxMP46; Open High Hat
    { 0x0E6C204,0x343E800, 0x10,0x00, 0xE,+12 }, // 527: dMP49; dMP57; hxMP49; hxMP57; Crash Cymbal 1
    { 0x212FD03,0x205FD02, 0x80,0x80, 0xA,+12 }, // 528: dMP51; dMP59; hxMP51; hxMP59; Ride Cymbal 1
    { 0x085E400,0x234D7C0, 0x80,0x80, 0xE,+12 }, // 529: dMP52; hxMP52; Chinses Cymbal
    { 0x0E6E204,0x144B801, 0x90,0x00, 0xE,+12 }, // 530: dMP53; hxMP53; Ride Bell
    { 0x2777602,0x3679801, 0x87,0x00, 0xF,+12 }, // 531: dMP54; hxMP54; Tambourine
    { 0x270F604,0x3A3C607, 0x81,0x00, 0xE,+12 }, // 532: dMP55; Splash Cymbal
    { 0x067FD00,0x098F601, 0x00,0x00, 0x6,+12 }, // 533: dMP56; hxMP56; Cowbell
    { 0x0B5F901,0x050D4BF, 0x07,0xC0, 0xB,+12 }, // 534: dMP58; hxMP58; Vibraslap
    { 0x256FB00,0x026FA00, 0x00,0x00, 0x4,+12 }, // 535: dMP60; dMP61; hxMP60; hxMP61; High Bongo
    { 0x256FB00,0x017F700, 0x80,0x00, 0x0,+12 }, // 536: dMP62; dMP63; dMP64; hxMP62; hxMP63; hxMP64; Low Conga
    { 0x056FB03,0x017F700, 0x81,0x00, 0x0,+12 }, // 537: dMP65; dMP66; hxMP65; hxMP66; High Timbale
    { 0x367FD01,0x098F601, 0x00,0x00, 0x8,+12 }, // 538: dMP67; dMP68; hxMP67; hxMP68; High Agogo
    { 0x2D65A00,0x0FFFFBF, 0x0E,0xC0, 0xA,+12 }, // 539: dMP70; hxMP70; Maracas
    { 0x1C7F900,0x0FFFF80, 0x07,0xC0, 0xA,+12 }, // 540: dMP71; dMP72; dMP73; dMP74; dMP79; hxMP71; hxMP72; hxMP73; hxMP74; hxMP79; Long Guiro
    { 0x1D1F813,0x078F512, 0x44,0x00, 0x6,+12 }, // 541: dMP75; dMP76; dMP77; hxMP75; hxMP76; hxMP77; Claves
    { 0x1DC5E01,0x0FFFFBF, 0x0B,0xC0, 0xA,+12 }, // 542: dMP78; hxMP78; Mute Cuica
    { 0x060F2C5,0x07AF4D4, 0x4F,0x80, 0x8,+12 }, // 543: dMP80; hxMP80; Mute Triangle
    { 0x160F285,0x0B7F294, 0x4F,0x80, 0x8,+12 }, // 544: dMP81; hxMP81; Open Triangle
    { 0x113F020,0x027E322, 0x8C,0x80, 0xA,+12 }, // 545: hxMM29; Overdriven Guitar
    { 0x125A020,0x136B220, 0x86,0x00, 0x6,+12 }, // 546: hxMM30; Distortion Guitar
    { 0x015C520,0x0A6D221, 0x28,0x00, 0xC,+12 }, // 547: hxMM32; Acoustic Bass
    { 0x1006010,0x0F68110, 0x1A,0x00, 0x8,+12 }, // 548: hxMM35; Fretless Bass
    { 0x2E7F030,0x047F131, 0x12,0x00, 0x0,+0 }, // 549: hxMM36; * Slap Bass 1
    { 0x1E7F510,0x2E7F610, 0x0D,0x00, 0xD,+12 }, // 550: hxMM36; * Slap Bass 1
    { 0x0465020,0x1569220, 0x96,0x80, 0xC,+12 }, // 551: hxMM58; Tuba
    { 0x075FC01,0x037F800, 0x00,0x00, 0x0,+12 }, // 552: hxMP40; Electric Snare
    { 0x175F701,0x336FC00, 0xC0,0x00, 0xC,+54 }, // 553: hxMP40; Electric Snare
    { 0x2709404,0x3A3C607, 0x81,0x00, 0xE,+12 }, // 554: hxMP55; Splash Cymbal
    { 0x132FA13,0x1F9F211, 0x80,0x0A, 0x8,+0 }, // 555: sGM6; Harpsichord
    { 0x0F2F409,0x0E2F211, 0x1B,0x80, 0x2,+0 }, // 556: sGM9; Glockenspiel
    { 0x0F3D403,0x0F3A340, 0x94,0x40, 0x6,+0 }, // 557: sGM14; Tubular Bells
    { 0x1058761,0x0058730, 0x80,0x03, 0x7,+0 }, // 558: sGM19; Church Organ
    { 0x174A423,0x0F8F271, 0x9D,0x80, 0xC,+0 }, // 559: sGM24; Acoustic Guitar1
    { 0x0007FF1,0x1167F21, 0x8D,0x00, 0x0,+0 }, // 560: sGM44; Tremulo Strings
    { 0x0759511,0x1F5C501, 0x0D,0x80, 0x0,+0 }, // 561: sGM45; Pizzicato String
    { 0x073F222,0x0F3F331, 0x97,0x80, 0x2,+0 }, // 562: sGM46; Orchestral Harp
    { 0x105F510,0x0C3F411, 0x41,0x00, 0x6,+0 }, // 563: sGM47; Timpany
    { 0x01096C1,0x1166221, 0x8B,0x00, 0x6,+0 }, // 564: sGM48; String Ensemble1
    { 0x01096C1,0x1153221, 0x8E,0x00, 0x6,+0 }, // 565: sGM49; String Ensemble2
    { 0x012C4A1,0x0065F61, 0x97,0x00, 0xE,+0 }, // 566: sGM50; Synth Strings 1
    { 0x010E4B1,0x0056A62, 0xCD,0x83, 0x0,+0 }, // 567: sGM52; Choir Aahs
    { 0x0F57591,0x144A440, 0x0D,0x00, 0xE,+0 }, // 568: sGM55; Orchestra Hit
    { 0x0256421,0x0088F21, 0x92,0x01, 0xC,+0 }, // 569: sGM56; Trumpet
    { 0x0167421,0x0078F21, 0x93,0x00, 0xC,+0 }, // 570: sGM57; Trombone
    { 0x0176421,0x0378261, 0x94,0x00, 0xC,+0 }, // 571: sGM58; Tuba
    { 0x0195361,0x0077F21, 0x94,0x04, 0xA,+0 }, // 572: sGM60; French Horn
    { 0x0187461,0x0088422, 0x8F,0x00, 0xA,+0 }, // 573: sGM61; Brass Section
    { 0x016A571,0x00A8F21, 0x4A,0x00, 0x8,+0 }, // 574: sGM68; Oboe
    { 0x00A8871,0x1198131, 0x4A,0x00, 0x0,+0 }, // 575: sGM70; Bassoon
    { 0x0219632,0x0187261, 0x4A,0x00, 0x4,+0 }, // 576: sGM71; Clarinet
    { 0x04A85E2,0x01A85E1, 0x59,0x00, 0x0,+0 }, // 577: sGM72; Piccolo
    { 0x02887E1,0x01975E1, 0x48,0x00, 0x0,+0 }, // 578: sGM73; Flute
    { 0x0451261,0x1045F21, 0x8E,0x84, 0x8,+0 }, // 579: sGM95; Pad 8 sweep
    { 0x106A510,0x004FA00, 0x86,0x03, 0x6,+0 }, // 580: sGM116; Taiko Drum
    { 0x202A50E,0x017A700, 0x09,0x00, 0xE,+0 }, // 581: sGM118; sGP38; sGP40; Acoustic Snare; Electric Snare; Synth Drum
    { 0x0001E0E,0x3FE1800, 0x00,0x00, 0xE,+0 }, // 582: sGM119; Reverse Cymbal
    { 0x0F6B710,0x005F011, 0x40,0x00, 0x6,+0 }, // 583: sGP35; sGP36; Ac Bass Drum; Bass Drum 1
    { 0x00BF506,0x008F602, 0x07,0x00, 0xA,+0 }, // 584: sGP37; Side Stick
    { 0x001FF0E,0x008FF0E, 0x00,0x00, 0xE,+0 }, // 585: sGP39; Hand Clap
    { 0x209F300,0x005F600, 0x06,0x00, 0x4,+0 }, // 586: sGP41; sGP43; sGP45; sGP47; sGP48; sGP50; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom
    { 0x006F60C,0x247FB12, 0x00,0x00, 0xE,+0 }, // 587: sGP42; Closed High Hat
    { 0x004F60C,0x244CB12, 0x00,0x05, 0xE,+0 }, // 588: sGP44; Pedal High Hat
    { 0x001F60C,0x242CB12, 0x00,0x00, 0xA,+0 }, // 589: sGP46; Open High Hat
    { 0x000F00E,0x3049F40, 0x00,0x00, 0xE,+0 }, // 590: sGP49; Crash Cymbal 1
    { 0x030F50E,0x0039F50, 0x00,0x04, 0xE,+0 }, // 591: sGP52; Chinese Cymbal
    { 0x204940E,0x0F78700, 0x02,0x0A, 0xA,+0 }, // 592: sGP54; Tambourine
    { 0x000F64E,0x2039F1E, 0x00,0x00, 0xE,+0 }, // 593: sGP55; Splash Cymbal
    { 0x000F60E,0x3029F50, 0x00,0x00, 0xE,+0 }, // 594: sGP57; Crash Cymbal 2
    { 0x100FF00,0x014FF10, 0x00,0x00, 0xC,+0 }, // 595: sGP58; Vibraslap
    { 0x04F760E,0x2187700, 0x40,0x03, 0xE,+0 }, // 596: sGP69; Cabasa
    { 0x1F4FC02,0x0F4F712, 0x00,0x05, 0x6,+0 }, // 597: sGP76; High Wood Block
    { 0x0B2F131,0x0AFF111, 0x8F,0x83, 0x8,+0 }, // 598: qGM0; AcouGrandPiano
    { 0x0B2F131,0x0D5C131, 0x19,0x01, 0x9,+0 }, // 599: qGM0; AcouGrandPiano
    { 0x0D2F111,0x0E6F211, 0x4C,0x83, 0xA,+0 }, // 600: qGM1; BrightAcouGrand
    { 0x0D5C111,0x0E6C231, 0x15,0x00, 0xB,+0 }, // 601: qGM1; BrightAcouGrand
    { 0x0D4F315,0x0E4B115, 0x5F,0x61, 0xE,+0 }, // 602: qGM2; ElecGrandPiano
    { 0x0E4B111,0x0B5B111, 0x5C,0x00, 0xE,+0 }, // 603: qGM2; ElecGrandPiano
    { 0x0D4F111,0x0E4C302, 0x89,0x5F, 0xD,+12 }, // 604: qGM3; Honky-tonkPiano
    { 0x035C100,0x0D5C111, 0x9B,0x00, 0xC,+0 }, // 605: qGM3; Honky-tonkPiano
    { 0x0E7F21C,0x0B8F201, 0x6F,0x80, 0xC,+0 }, // 606: qGM4; Rhodes Piano
    { 0x0E5B111,0x0B8F211, 0x9C,0x80, 0xD,+0 }, // 607: qGM4; Rhodes Piano
    { 0x0E7C21C,0x0B8F301, 0x3A,0x80, 0x0,+0 }, // 608: qGM5; Chorused Piano
    { 0x0F5B111,0x0D8F211, 0x1B,0x80, 0x1,+0 }, // 609: qGM5; Chorused Piano
    { 0x031F031,0x037F234, 0x90,0x9F, 0x8,+0 }, // 610: qGM6; Harpsichord
    { 0x451F324,0x497F211, 0x1C,0x00, 0x8,+0 }, // 611: qGM6; Harpsichord
    { 0x050F210,0x0F0E131, 0x60,0x5D, 0x4,+12 }, // 612: qGM7; Clavinet
    { 0x040B230,0x5E9F111, 0xA2,0x80, 0x4,+0 }, // 613: qGM7; Clavinet
    { 0x0E6CE02,0x0E6F401, 0x25,0x00, 0x0,+0 }, // 614: qGM8; Celesta
    { 0x0E6F507,0x0E5F341, 0xA1,0x00, 0x1,+0 }, // 615: qGM8; Celesta
    { 0x0E3F217,0x0E2C211, 0x54,0x06, 0xA,+0 }, // 616: qGM9; Glockenspiel
    { 0x0C3F219,0x0D2F291, 0x2B,0x07, 0xB,+0 }, // 617: qGM9; Glockenspiel
    { 0x004A61A,0x004F600, 0x27,0x0A, 0x3,+0 }, // 618: qGM10; Music box
    { 0x0790824,0x0E6E384, 0x9A,0x5B, 0xA,+12 }, // 619: qGM11; Vibraphone
    { 0x0E6F314,0x0E6F280, 0x62,0x00, 0xB,+0 }, // 620: qGM11; Vibraphone
    { 0x055F71C,0x0D88520, 0xA3,0x0D, 0x6,+0 }, // 621: qGM12; Marimba
    { 0x055F718,0x0D8E521, 0x23,0x00, 0x7,+0 }, // 622: qGM12; Marimba
    { 0x0D6F90A,0x0D6F784, 0x53,0x80, 0xA,+0 }, // 623: qGM13; Xylophone
    { 0x0A6F615,0x0E6F601, 0x91,0x00, 0xB,+0 }, // 624: qGM13; Xylophone
    { 0x0B3D441,0x0B4C280, 0x8A,0x13, 0x4,+0 }, // 625: qGM14; Tubular Bells
    { 0x082D345,0x0E3A381, 0x59,0x80, 0x5,+0 }, // 626: qGM14; Tubular Bells
    { 0x0F7E701,0x1557403, 0x84,0x49, 0xD,+0 }, // 627: qGM15; Dulcimer
    { 0x005B301,0x0F77601, 0x80,0x80, 0xD,+0 }, // 628: qGM15; Dulcimer
    { 0x02AA2A0,0x02AA522, 0x85,0x9E, 0x7,+0 }, // 629: qGM16; Hammond Organ
    { 0x02AA5A2,0x02AA128, 0x83,0x95, 0x7,+0 }, // 630: qGM16; Hammond Organ
    { 0x02A91A0,0x03AC821, 0x85,0x0B, 0x7,+0 }, // 631: qGM17; Percussive Organ
    { 0x038C620,0x057F621, 0x81,0x80, 0x7,+0 }, // 632: qGM17; Percussive Organ
    { 0x12AA6E3,0x00AAF61, 0x56,0x83, 0x8,-12 }, // 633: qGM18; Rock Organ
    { 0x00AAFE1,0x00AAF62, 0x91,0x83, 0x9,+0 }, // 634: qGM18; Rock Organ
    { 0x002B025,0x0057030, 0x5F,0x40, 0xC,+0 }, // 635: qGM19; Church Organ
    { 0x002C031,0x0056031, 0x46,0x80, 0xD,+0 }, // 636: qGM19; Church Organ
    { 0x015C821,0x0056F31, 0x93,0x00, 0xC,+0 }, // 637: qGM20; Reed Organ
    { 0x005CF31,0x0057F32, 0x16,0x87, 0xD,+0 }, // 638: qGM20; Reed Organ
    { 0x71A7223,0x02A7221, 0xAC,0x83, 0x0,+0 }, // 639: qGM21; Accordion
    { 0x41A6223,0x02A62A1, 0x22,0x00, 0x1,+0 }, // 640: qGM21; Accordion
    { 0x006FF25,0x005FF23, 0xA1,0x2F, 0xA,+0 }, // 641: qGM22; Harmonica
    { 0x405FFA1,0x0096F22, 0x1F,0x80, 0xA,+0 }, // 642: qGM22; Harmonica
    { 0x11A6223,0x02A7221, 0x19,0x80, 0xC,+0 }, // 643: qGM23; Tango Accordion
    { 0x41A6223,0x02A7222, 0x1E,0x83, 0xD,+0 }, // 644: qGM23; Tango Accordion
    { 0x074F302,0x0B8F341, 0x9C,0x80, 0xA,+0 }, // 645: qGM24; Acoustic Guitar1
    { 0x274D302,0x0B8D382, 0xA5,0x40, 0xB,+0 }, // 646: qGM24; Acoustic Guitar1
    { 0x2F6F234,0x0F7F231, 0x5B,0x9E, 0xC,+0 }, // 647: qGM25; Acoustic Guitar2
    { 0x0F7F223,0x0E7F111, 0xAB,0x00, 0xC,+0 }, // 648: qGM25; Acoustic Guitar2
    { 0x0FAF322,0x0FAF223, 0x53,0x66, 0xA,+0 }, // 649: qGM26; Electric Guitar1
    { 0x0FAC221,0x0F7C221, 0xA7,0x00, 0xA,+0 }, // 650: qGM26; Electric Guitar1
    { 0x022FA02,0x0F3F301, 0x4C,0x97, 0x8,+0 }, // 651: qGM27; Electric Guitar2
    { 0x1F3C204,0x0F7C111, 0x9D,0x00, 0x8,+0 }, // 652: qGM27; Electric Guitar2
    { 0x0AFC711,0x0F8F501, 0x87,0x00, 0x8,+0 }, // 653: qGM28; Electric Guitar3
    { 0x098C301,0x0F8C302, 0x18,0x00, 0x9,+0 }, // 654: qGM28; Electric Guitar3
    { 0x4F2B913,0x0119102, 0x0D,0x1A, 0xA,+0 }, // 655: qGM29; Overdrive Guitar
    { 0x14A9221,0x02A9122, 0x99,0x00, 0xA,+0 }, // 656: qGM29; Overdrive Guitar
    { 0x242F823,0x2FA9122, 0x96,0x1A, 0x0,+0 }, // 657: qGM30; Distorton Guitar
    { 0x0BA9221,0x04A9122, 0x99,0x00, 0x0,+0 }, // 658: qGM30; Distorton Guitar
    { 0x04F2009,0x0F8D104, 0xA1,0x80, 0x8,+0 }, // 659: qGM31; Guitar Harmonics
    { 0x2F8F802,0x0F8F602, 0x87,0x00, 0x9,+0 }, // 660: qGM31; Guitar Harmonics
    { 0x015A701,0x0C8A301, 0x4D,0x00, 0x2,+0 }, // 661: qGM32; Acoustic Bass
    { 0x0317101,0x0C87301, 0x93,0x00, 0x3,+0 }, // 662: qGM32; Acoustic Bass
    { 0x0E5F111,0x0E5F312, 0xA8,0x57, 0x4,+0 }, // 663: qGM33; Electric Bass 1
    { 0x0E5E111,0x0E6E111, 0x97,0x00, 0x4,+0 }, // 664: qGM33; Electric Bass 1
    { 0x0C7F001,0x027F101, 0xB3,0x16, 0x6,+0 }, // 665: qGM34; Electric Bass 2
    { 0x027F101,0x028F101, 0x16,0x00, 0x6,+0 }, // 666: qGM34; Electric Bass 2
    { 0x0487131,0x0487131, 0x19,0x00, 0xD,+0 }, // 667: qGM35; Fretless Bass
    { 0x0DAF904,0x0DFF701, 0x0B,0x80, 0x9,+0 }, // 668: qGM36; Slap Bass 1
    { 0x09AA101,0x0DFF221, 0x89,0x40, 0x6,+0 }, // 669: qGM37; Slap Bass 2
    { 0x0DAF904,0x0DFF701, 0x0B,0x80, 0x7,+0 }, // 670: qGM37; Slap Bass 2
    { 0x0C8F621,0x0C8F101, 0x1C,0x1F, 0xA,+0 }, // 671: qGM38; Synth Bass 1
    { 0x0C8F101,0x0C8F201, 0xD8,0x00, 0xA,+0 }, // 672: qGM38; Synth Bass 1
    { 0x1C8F621,0x0C8F101, 0x1C,0x1F, 0xA,+0 }, // 673: qGM39; Synth Bass 2
    { 0x0425401,0x0C8F201, 0x12,0x00, 0xA,+0 }, // 674: qGM39; Synth Bass 2
    { 0x1038D12,0x0866503, 0x95,0x8B, 0x9,+0 }, // 675: qGM40; qGM41; Viola; Violin
    { 0x113DD31,0x0265621, 0x17,0x00, 0x8,+0 }, // 676: qGM41; Viola
    { 0x513DD31,0x0265621, 0x95,0x00, 0x8,+0 }, // 677: qGM42; Cello
    { 0x1038D13,0x0866605, 0x95,0x8C, 0x9,+0 }, // 678: qGM42; Cello
    { 0x243CC70,0x21774A0, 0x92,0x03, 0xE,+0 }, // 679: qGM43; Contrabass
    { 0x007BF21,0x1076F21, 0x95,0x00, 0xF,+0 }, // 680: qGM43; Contrabass
    { 0x515C261,0x0056FA1, 0x97,0x00, 0x6,+0 }, // 681: qGM44; Tremulo Strings
    { 0x08FB563,0x08FB5A5, 0x13,0x94, 0x7,+0 }, // 682: qGM44; Tremulo Strings
    { 0x0848523,0x0748212, 0xA7,0xA4, 0xE,+0 }, // 683: qGM45; qGM46; Orchestral Harp; Pizzicato String
    { 0x0748202,0x0358511, 0x27,0x00, 0xE,+0 }, // 684: qGM45; Pizzicato String
    { 0x0748202,0x0338411, 0x27,0x00, 0xE,+0 }, // 685: qGM46; Orchestral Harp
    { 0x005F511,0x0C3F212, 0x01,0x1E, 0x3,+0 }, // 686: qGM47; Timpany
    { 0x2036130,0x21764A0, 0x98,0x03, 0xE,+0 }, // 687: qGM48; String Ensemble1
    { 0x1176561,0x0176521, 0x92,0x00, 0xF,+0 }, // 688: qGM48; String Ensemble1
    { 0x2234130,0x2174460, 0x98,0x01, 0xE,+0 }, // 689: qGM49; String Ensemble2
    { 0x1037FA1,0x1073F21, 0x98,0x00, 0xF,+0 }, // 690: qGM49; String Ensemble2
    { 0x012C121,0x0054F61, 0x1A,0x00, 0xC,+0 }, // 691: qGM50; Synth Strings 1
    { 0x012C1A1,0x0054F21, 0x93,0x00, 0xD,+0 }, // 692: qGM50; Synth Strings 1
    { 0x022C122,0x0054F22, 0x0B,0x1C, 0xD,+0 }, // 693: qGM51; SynthStrings 2
    { 0x0F5A006,0x035A3E4, 0x03,0x23, 0xE,+0 }, // 694: qGM52; Choir Aahs
    { 0x0077FA1,0x0077F61, 0x51,0x00, 0xF,+0 }, // 695: qGM52; Choir Aahs
    { 0x0578402,0x074A7E4, 0x05,0x16, 0xE,+0 }, // 696: qGM53; Voice Oohs
    { 0x03974A1,0x0677161, 0x90,0x00, 0xF,+0 }, // 697: qGM53; Voice Oohs
    { 0x054990A,0x0639707, 0x65,0x60, 0x8,+0 }, // 698: qGM54; Synth Voice
    { 0x1045FA1,0x0066F61, 0x59,0x00, 0x8,+0 }, // 699: qGM54; Synth Voice
    { 0x2686500,0x613C500, 0x00,0x00, 0xB,+0 }, // 700: qGM55; Orchestra Hit
    { 0x606C800,0x3077400, 0x00,0x00, 0xB,+0 }, // 701: qGM55; Orchestra Hit
    { 0x0178421,0x008AF61, 0x15,0x0B, 0xD,+0 }, // 702: qGM56; Trumpet
    { 0x0178521,0x0097F21, 0x94,0x05, 0xC,+0 }, // 703: qGM57; Trombone
    { 0x0178421,0x008AF61, 0x15,0x0D, 0xD,+0 }, // 704: qGM57; Trombone
    { 0x0157620,0x0378261, 0x94,0x00, 0xC,+12 }, // 705: qGM58; Tuba
    { 0x02661B1,0x0266171, 0xD3,0x80, 0xD,+0 }, // 706: qGM58; Tuba
    { 0x1277131,0x0499161, 0x15,0x83, 0xC,+0 }, // 707: qGM59; Muted Trumpet
    { 0x0277DB1,0x0297A21, 0x10,0x08, 0xD,+0 }, // 708: qGM59; Muted Trumpet
    { 0x00A6321,0x00B7F21, 0x9F,0x00, 0xE,+0 }, // 709: qGM60; French Horn
    { 0x00A65A1,0x00B7F61, 0xA2,0x00, 0xF,+0 }, // 710: qGM60; French Horn
    { 0x0257221,0x00A7F21, 0x16,0x05, 0xC,+0 }, // 711: qGM61; Brass Section
    { 0x0357A21,0x03A7A21, 0x1D,0x09, 0xD,+0 }, // 712: qGM61; Brass Section
    { 0x035C221,0x00ACF61, 0x16,0x09, 0xE,+0 }, // 713: qGM62; Synth Brass 1
    { 0x04574A1,0x0087F21, 0x8A,0x00, 0xF,+0 }, // 714: qGM62; Synth Brass 1
    { 0x01A52A1,0x01B8F61, 0x97,0x00, 0xC,+0 }, // 715: qGM63; Synth Brass 2
    { 0x01A7521,0x01B8F21, 0xA1,0x00, 0xD,+0 }, // 716: qGM63; Synth Brass 2
    { 0x20F9331,0x00F72A1, 0x96,0x00, 0x8,+0 }, // 717: qGM64; Soprano Sax
    { 0x0078521,0x1278431, 0x96,0x00, 0x9,+0 }, // 718: qGM64; Soprano Sax
    { 0x1039331,0x00972A1, 0x8E,0x00, 0x8,+0 }, // 719: mGM65; qGM65; Alto Sax
    { 0x006C524,0x1276431, 0xA1,0x00, 0x9,+0 }, // 720: qGM65; Alto Sax
    { 0x10693B1,0x0067271, 0x8E,0x00, 0xA,+0 }, // 721: qGM66; Tenor Sax
    { 0x0088521,0x02884B1, 0x5D,0x00, 0xB,+0 }, // 722: qGM66; Tenor Sax
    { 0x10F9331,0x00F7272, 0x93,0x00, 0xC,+0 }, // 723: qGM67; Baritone Sax
    { 0x0068522,0x01684B1, 0x61,0x00, 0xD,+0 }, // 724: qGM67; Baritone Sax
    { 0x02AA961,0x036A823, 0xA3,0x52, 0x8,+0 }, // 725: qGM68; Oboe
    { 0x016AAA1,0x00A8F21, 0x94,0x80, 0x8,+0 }, // 726: qGM68; Oboe
    { 0x0297721,0x1267A33, 0x21,0x55, 0x2,+0 }, // 727: qGM69; English Horn
    { 0x0167AA1,0x0197A22, 0x93,0x00, 0x2,+0 }, // 728: qGM69; English Horn
    { 0x1077B21,0x0007F22, 0x2B,0x57, 0xA,+0 }, // 729: qGM70; Bassoon
    { 0x0197531,0x0196172, 0x51,0x00, 0xA,+0 }, // 730: qGM70; Bassoon
    { 0x0219B32,0x0177221, 0x90,0x00, 0x8,+0 }, // 731: qGM71; Clarinet
    { 0x0219B32,0x0177221, 0x90,0x13, 0x9,+0 }, // 732: qGM71; Clarinet
    { 0x011DA25,0x068A6E3, 0x00,0x2B, 0xC,+0 }, // 733: qGM72; qGM73; Flute; Piccolo
    { 0x05F85E1,0x01A65E1, 0x1F,0x00, 0xD,+0 }, // 734: qGM72; Piccolo
    { 0x05F88E1,0x01A65E1, 0x46,0x00, 0xD,+0 }, // 735: qGM73; Flute
    { 0x029C9A4,0x0086F21, 0xA2,0x80, 0xC,+0 }, // 736: qGM74; Recorder
    { 0x015CAA2,0x0086F21, 0xAA,0x00, 0xD,+0 }, // 737: qGM74; Recorder
    { 0x011DA25,0x068A623, 0x00,0x1E, 0xC,+0 }, // 738: qGM75; Pan Flute
    { 0x0588821,0x01A6521, 0x8C,0x00, 0xD,+0 }, // 739: qGM75; Pan Flute
    { 0x0C676A1,0x0868726, 0x0D,0x59, 0xF,+0 }, // 740: qGM76; Bottle Blow
    { 0x0566622,0x02665A1, 0x56,0x00, 0xE,+0 }, // 741: qGM76; Bottle Blow
    { 0x0019F26,0x0487664, 0x00,0x25, 0xE,+0 }, // 742: qGM77; Shakuhachi
    { 0x0465622,0x03645A1, 0xCB,0x00, 0xF,+0 }, // 743: qGM77; Shakuhachi
    { 0x11467E1,0x0175461, 0x67,0x00, 0xC,+0 }, // 744: qGM78; Whistle
    { 0x1146721,0x0164421, 0x6D,0x00, 0xD,+0 }, // 745: qGM78; Whistle
    { 0x001DF26,0x03876E4, 0x00,0x2B, 0xC,+0 }, // 746: qGM79; Ocarina
    { 0x0369522,0x00776E1, 0xD8,0x00, 0xD,+0 }, // 747: qGM79; Ocarina
    { 0x00FFF21,0x00FFF21, 0x35,0xB7, 0x4,+0 }, // 748: qGM80; Lead 1 squareea
    { 0x00FFF21,0x60FFF21, 0xB9,0x80, 0x4,+0 }, // 749: qGM80; Lead 1 squareea
    { 0x00FFF21,0x00FFF21, 0x36,0x1B, 0xA,+0 }, // 750: qGM81; Lead 2 sawtooth
    { 0x00FFF21,0x409CF61, 0x1D,0x00, 0xA,+0 }, // 751: qGM81; Lead 2 sawtooth
    { 0x087C4A3,0x076C626, 0x00,0x57, 0xE,+0 }, // 752: qGM82; Lead 3 calliope
    { 0x0558622,0x0186421, 0x46,0x80, 0xF,+0 }, // 753: qGM82; Lead 3 calliope
    { 0x04AA321,0x00A8621, 0x48,0x00, 0x8,+0 }, // 754: qGM83; Lead 4 chiff
    { 0x0126621,0x00A9621, 0x45,0x00, 0x9,+0 }, // 755: qGM83; Lead 4 chiff
    { 0x4F2B912,0x0119101, 0x0D,0x1A, 0xA,+0 }, // 756: qGM84; Lead 5 charang
    { 0x12A9221,0x02A9122, 0x99,0x00, 0xA,+0 }, // 757: qGM84; Lead 5 charang
    { 0x0157D61,0x01572B1, 0x40,0xA3, 0xE,+0 }, // 758: qGM85; Lead 6 voice
    { 0x005DFA2,0x0077F61, 0x5D,0x40, 0xF,+0 }, // 759: qGM85; Lead 6 voice
    { 0x001FF20,0x4068F61, 0x36,0x00, 0x8,+0 }, // 760: qGM86; Lead 7 fifths
    { 0x00FFF21,0x4078F61, 0x27,0x00, 0x9,+0 }, // 761: qGM86; Lead 7 fifths
    { 0x109F121,0x109F121, 0x1D,0x80, 0xB,+0 }, // 762: qGM87; Lead 8 brass
    { 0x1035317,0x004F608, 0x1A,0x0D, 0x2,+0 }, // 763: qGM88; Pad 1 new age
    { 0x03241A1,0x0156161, 0x9D,0x00, 0x3,+0 }, // 764: qGM88; Pad 1 new age
    { 0x031A181,0x0032571, 0xA1,0x00, 0xB,+0 }, // 765: qGM89; Pad 2 warm
    { 0x0141161,0x0165561, 0x17,0x00, 0xC,+0 }, // 766: qGM90; Pad 3 polysynth
    { 0x445C361,0x025C361, 0x14,0x00, 0xD,+0 }, // 767: qGM90; Pad 3 polysynth
    { 0x021542A,0x0136A27, 0x80,0xA6, 0xE,+0 }, // 768: qGM91; Pad 4 choir
    { 0x0015431,0x0036A72, 0x5D,0x00, 0xF,+0 }, // 769: qGM91; Pad 4 choir
    { 0x0332121,0x0454222, 0x97,0x03, 0x8,+0 }, // 770: qGM92; Pad 5 bowedpad
    { 0x0D421A1,0x0D54221, 0x99,0x03, 0x9,+0 }, // 771: qGM92; Pad 5 bowedpad
    { 0x0336121,0x0354261, 0x8D,0x03, 0xA,+0 }, // 772: qGM93; Pad 6 metallic
    { 0x177A1A1,0x1473121, 0x1C,0x00, 0xB,+0 }, // 773: qGM93; Pad 6 metallic
    { 0x0331121,0x0354261, 0x89,0x03, 0xA,+0 }, // 774: qGM94; Pad 7 halo
    { 0x0E42121,0x0D54261, 0x8C,0x03, 0xB,+0 }, // 775: qGM94; Pad 7 halo
    { 0x1471121,0x007CF21, 0x15,0x00, 0x0,+0 }, // 776: qGM95; Pad 8 sweep
    { 0x0E41121,0x0D55261, 0x8C,0x00, 0x1,+0 }, // 777: qGM95; Pad 8 sweep
    { 0x58AFE0F,0x006FB04, 0x83,0x85, 0xC,+0 }, // 778: qGM96; FX 1 rain
    { 0x003A821,0x004A722, 0x99,0x00, 0xD,+0 }, // 779: qGM96; FX 1 rain
    { 0x2322121,0x0133220, 0x8C,0x97, 0x6,+0 }, // 780: qGM97; FX 2 soundtrack
    { 0x1031121,0x0133121, 0x0E,0x00, 0x7,+0 }, // 781: qGM97; FX 2 soundtrack
    { 0x0937501,0x0B4C502, 0x61,0x80, 0x8,+0 }, // 782: qGM98; FX 3 crystal
    { 0x0957406,0x072A501, 0x5B,0x00, 0x9,+0 }, // 783: qGM98; FX 3 crystal
    { 0x056B222,0x056F261, 0x92,0x8A, 0xC,+0 }, // 784: qGM99; FX 4 atmosphere
    { 0x2343121,0x00532A1, 0x9D,0x80, 0xD,+0 }, // 785: qGM99; FX 4 atmosphere
    { 0x088A324,0x087A322, 0x40,0x5B, 0xE,+0 }, // 786: qGM100; FX 5 brightness
    { 0x151F101,0x0F5F241, 0x13,0x00, 0xF,+0 }, // 787: qGM100; FX 5 brightness
    { 0x04211A1,0x0731161, 0x10,0x92, 0xA,+0 }, // 788: qGM101; FX 6 goblins
    { 0x0211161,0x0031DA1, 0x98,0x80, 0xB,+0 }, // 789: qGM101; FX 6 goblins
    { 0x0167D62,0x01672A2, 0x57,0x80, 0x4,+0 }, // 790: qGM102; FX 7 echoes
    { 0x0069F61,0x0049FA1, 0x5B,0x00, 0x5,+0 }, // 791: qGM102; FX 7 echoes
    { 0x024A238,0x024F231, 0x9F,0x9C, 0x6,+0 }, // 792: qGM103; FX 8 sci-fi
    { 0x014F123,0x0238161, 0x9F,0x00, 0x6,+0 }, // 793: qGM103; FX 8 sci-fi
    { 0x053F301,0x1F6F101, 0x46,0x80, 0x0,+0 }, // 794: qGM104; Sitar
    { 0x053F201,0x0F6F208, 0x43,0x40, 0x1,+0 }, // 795: qGM104; Sitar
    { 0x135A511,0x133A517, 0x10,0xA4, 0x0,+0 }, // 796: qGM105; Banjo
    { 0x141F611,0x2E5F211, 0x0D,0x00, 0x0,+0 }, // 797: qGM105; Banjo
    { 0x0F8F755,0x1E4F752, 0x92,0x9F, 0xE,+0 }, // 798: qGM106; Shamisen
    { 0x0E4F341,0x1E5F351, 0x13,0x00, 0xE,+0 }, // 799: qGM106; Shamisen
    { 0x032D493,0x111EB11, 0x91,0x00, 0x8,+0 }, // 800: qGM107; Koto
    { 0x032D453,0x112EB13, 0x91,0x0D, 0x9,+0 }, // 801: qGM107; Koto
    { 0x3E5F720,0x0E5F521, 0x00,0x0C, 0xD,+0 }, // 802: qGM108; Kalimba
    { 0x0207C21,0x10C6F22, 0x09,0x09, 0x7,+0 }, // 803: qGM109; Bagpipe
    { 0x133DD02,0x0166601, 0x83,0x80, 0xB,+0 }, // 804: qGM110; Fiddle
    { 0x0298961,0x406D8A3, 0x33,0xA4, 0x6,+0 }, // 805: qGM111; Shanai
    { 0x005DA21,0x00B8F22, 0x17,0x80, 0x6,+0 }, // 806: qGM111; Shanai
    { 0x053C601,0x0D5F583, 0x71,0x40, 0x7,+0 }, // 807: qGM112; Tinkle Bell
    { 0x026EC08,0x016F804, 0x15,0x00, 0xA,+0 }, // 808: qGM113; Agogo Bells
    { 0x026EC07,0x016F802, 0x15,0x00, 0xB,+0 }, // 809: qGM113; Agogo Bells
    { 0x024682C,0x035DF01, 0xAB,0x00, 0x0,+0 }, // 810: qGM114; Steel Drums
    { 0x0356705,0x005DF01, 0x9D,0x00, 0x1,+0 }, // 811: qGM114; Steel Drums
    { 0x4FCFA15,0x0ECFA12, 0x11,0x80, 0xA,+0 }, // 812: qGM115; Woodblock
    { 0x0FCFA18,0x0E5F812, 0x9D,0x00, 0xB,+0 }, // 813: qGM115; Woodblock
    { 0x007A801,0x083F600, 0x5C,0x03, 0x7,+0 }, // 814: qGM116; Taiko Drum
    { 0x458F811,0x0E5F310, 0x8F,0x00, 0xE,+0 }, // 815: qGM117; Melodic Tom
    { 0x154F610,0x0E4F410, 0x92,0x00, 0xF,+0 }, // 816: qGM117; Melodic Tom
    { 0x455F811,0x0E5F410, 0x86,0x00, 0xE,+0 }, // 817: qGM118; Synth Drum
    { 0x155F311,0x0E5F410, 0x9C,0x00, 0xF,+0 }, // 818: qGM118; Synth Drum
    { 0x0001F0F,0x3F01FC0, 0x00,0x00, 0xE,+0 }, // 819: qGM119; Reverse Cymbal
    { 0x0001F0F,0x3F11FC0, 0x3F,0x3F, 0xF,+0 }, // 820: qGM119; Reverse Cymbal
    { 0x024F806,0x7845603, 0x80,0x88, 0xE,+0 }, // 821: qGM120; Guitar FretNoise
    { 0x024D803,0x7846604, 0x1E,0x08, 0xF,+0 }, // 822: qGM120; Guitar FretNoise
    { 0x001FF06,0x3043414, 0x00,0x00, 0xE,+0 }, // 823: qGM121; Breath Noise
    { 0x0F10001,0x0F10001, 0x3F,0x3F, 0xF,+0 }, // 824: qGM121; qGM122; qGM126; Applause/Noise; Breath Noise; Seashore
    { 0x001FF26,0x1841204, 0x00,0x00, 0xE,+0 }, // 825: qGM122; Seashore
    { 0x0F86848,0x0F10001, 0x00,0x3F, 0x5,+0 }, // 826: qGM123; Bird Tweet
    { 0x0F86747,0x0F8464C, 0x00,0x00, 0x5,+0 }, // 827: qGM123; Bird Tweet
    { 0x261B235,0x015F414, 0x1C,0x08, 0xA,+1 }, // 828: qGM124; Telephone
    { 0x715FE11,0x019F487, 0x20,0xC0, 0xB,+0 }, // 829: qGM124; Telephone
    { 0x1112EF0,0x11621E2, 0x00,0xC0, 0x8,-36 }, // 830: qGM125; Helicopter
    { 0x7112EF0,0x11621E2, 0x00,0xC0, 0x9,+0 }, // 831: qGM125; Helicopter
    { 0x001FF26,0x71612E4, 0x00,0x00, 0xE,+0 }, // 832: qGM126; Applause/Noise
    { 0x059F200,0x000F701, 0x00,0x00, 0xE,+0 }, // 833: qGM127; Gunshot
    { 0x0F0F301,0x6C9F601, 0x00,0x00, 0xE,+0 }, // 834: qGM127; Gunshot
    { 0x0FEF512,0x0FFF612, 0x11,0xA2, 0x6,+0 }, // 835: qGP37; Side Stick
    { 0x0FFF901,0x0FFF811, 0x0F,0x00, 0x6,+0 }, // 836: qGP37; Side Stick
    { 0x007FC01,0x638F802, 0x03,0x03, 0xF,+0 }, // 837: qGP38; Acoustic Snare
    { 0x204FF82,0x015FF10, 0x00,0x06, 0xE,+0 }, // 838: qGP39; Hand Clap
    { 0x007FF00,0x008FF01, 0x02,0x00, 0xF,+0 }, // 839: qGP39; Hand Clap
    { 0x007FC00,0x638F801, 0x03,0x03, 0xF,+0 }, // 840: qGP40; Electric Snare
    { 0x00CFD01,0x03CD600, 0x07,0x00, 0x0,+0 }, // 841: qGP41; qGP43; qGP45; qGP47; qGP48; qGP50; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom
    { 0x00CF600,0x006F600, 0x00,0x00, 0x1,+0 }, // 842: qGP41; qGP43; qGP45; qGP47; qGP48; qGP50; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom
    { 0x008F60C,0x247FB12, 0x00,0x00, 0xB,+0 }, // 843: qGP42; Closed High Hat
    { 0x008F60C,0x2477B12, 0x00,0x00, 0xA,+0 }, // 844: qGP44; Pedal High Hat
    { 0x008F60C,0x2477B12, 0x00,0x00, 0xB,+0 }, // 845: qGP44; Pedal High Hat
    { 0x002F60C,0x243CB12, 0x00,0x15, 0xB,+0 }, // 846: qGP46; Open High Hat
    { 0x055F201,0x000F441, 0x00,0x00, 0xE,+0 }, // 847: qGP49; Crash Cymbal 1
    { 0x000F301,0x0A4F48F, 0x00,0x00, 0xE,+0 }, // 848: qGP49; Crash Cymbal 1
    { 0x3E4E40F,0x1E5F508, 0x00,0x0A, 0x6,+0 }, // 849: qGP51; qGP59; Ride Cymbal 1; Ride Cymbal 2
    { 0x366F50F,0x1A5F508, 0x00,0x19, 0x7,+0 }, // 850: qGP51; qGP59; Ride Cymbal 1; Ride Cymbal 2
    { 0x065F981,0x030F241, 0x00,0x00, 0xE,+0 }, // 851: qGP52; Chinese Cymbal
    { 0x000FE46,0x055F585, 0x00,0x00, 0xE,+0 }, // 852: qGP52; Chinese Cymbal
    { 0x3E4E40F,0x1E5F507, 0x00,0x11, 0x6,+0 }, // 853: qGP53; Ride Bell
    { 0x365F50F,0x1A5F506, 0x00,0x1E, 0x7,+0 }, // 854: qGP53; Ride Bell
    { 0x0C49406,0x2F5F604, 0x00,0x00, 0x0,+0 }, // 855: qGP54; Tambourine
    { 0x004F902,0x0F79705, 0x00,0x03, 0x0,+0 }, // 856: qGP54; Tambourine
    { 0x156F28F,0x100F446, 0x03,0x00, 0xE,+0 }, // 857: qGP55; Splash Cymbal
    { 0x000F38F,0x0A5F442, 0x00,0x06, 0xE,+0 }, // 858: qGP55; Splash Cymbal
    { 0x237F811,0x005F310, 0x45,0x00, 0x8,+0 }, // 859: qGP56; Cow Bell
    { 0x037F811,0x005F310, 0x05,0x08, 0x9,+0 }, // 860: qGP56; Cow Bell
    { 0x155F381,0x000F441, 0x00,0x00, 0xE,+0 }, // 861: qGP57; Crash Cymbal 2
    { 0x000F341,0x0A4F48F, 0x00,0x00, 0xE,+0 }, // 862: qGP57; Crash Cymbal 2
    { 0x503FF80,0x014FF10, 0x00,0x00, 0xC,+0 }, // 863: qGP58; Vibraslap
    { 0x503FF80,0x014FF10, 0x00,0x0D, 0xD,+0 }, // 864: qGP58; Vibraslap
    { 0x00CF506,0x008F502, 0xC8,0x0B, 0x6,+0 }, // 865: qGP60; High Bongo
    { 0x00CF506,0x007F501, 0xC5,0x03, 0x7,+0 }, // 866: qGP60; High Bongo
    { 0x0BFFA01,0x096C802, 0x8F,0x80, 0x6,+0 }, // 867: qGP61; Low Bongo
    { 0x0BFFA01,0x096C802, 0xCF,0x0B, 0x7,+0 }, // 868: qGP61; Low Bongo
    { 0x087FA01,0x0B7FA01, 0x4F,0x08, 0x7,+0 }, // 869: qGP62; Mute High Conga
    { 0x08DFA01,0x0B5F802, 0x55,0x00, 0x6,+0 }, // 870: qGP63; Open High Conga
    { 0x08DFA01,0x0B5F802, 0x55,0x12, 0x7,+0 }, // 871: qGP63; Open High Conga
    { 0x08DFA01,0x0B6F802, 0x59,0x00, 0x6,+0 }, // 872: qGP64; Low Conga
    { 0x08DFA01,0x0B6F802, 0x59,0x12, 0x7,+0 }, // 873: qGP64; Low Conga
    { 0x00AFA01,0x006F900, 0x00,0x00, 0xE,+0 }, // 874: qGP65; High Timbale
    { 0x00AFA01,0x006F900, 0x00,0x0D, 0xF,+0 }, // 875: qGP65; High Timbale
    { 0x089F900,0x06CF600, 0x80,0x08, 0xF,+0 }, // 876: qGP66; Low Timbale
    { 0x388F803,0x0B6F60C, 0x8D,0x00, 0xE,+0 }, // 877: qGP67; High Agogo
    { 0x088F803,0x0B8F80C, 0x88,0x12, 0xF,+0 }, // 878: qGP67; High Agogo
    { 0x388F803,0x0B6F60C, 0x88,0x03, 0xE,+0 }, // 879: qGP68; Low Agogo
    { 0x388F803,0x0B8F80C, 0x88,0x0F, 0xF,+0 }, // 880: qGP68; Low Agogo
    { 0x04F760F,0x2187700, 0x40,0x08, 0xE,+0 }, // 881: qGP69; Cabasa
    { 0x04F760F,0x2187700, 0x00,0x12, 0xF,+0 }, // 882: qGP69; Cabasa
    { 0x249C80F,0x2699B02, 0x40,0x80, 0xE,+0 }, // 883: qGP70; Maracas
    { 0x249C80F,0x2699B0F, 0xC0,0x19, 0xF,+0 }, // 884: qGP70; Maracas
    { 0x305AD57,0x0058D87, 0xDC,0x00, 0xE,+0 }, // 885: qGP71; Short Whistle
    { 0x305AD47,0x0058D87, 0xDC,0x12, 0xF,+0 }, // 886: qGP71; Short Whistle
    { 0x304A857,0x0048887, 0xDC,0x00, 0xE,+0 }, // 887: qGP72; Long Whistle
    { 0x304A857,0x0058887, 0xDC,0x08, 0xF,+0 }, // 888: qGP72; Long Whistle
    { 0x506F680,0x016F610, 0x00,0x00, 0xC,+0 }, // 889: qGP73; qGP74; Long Guiro; Short Guiro
    { 0x50F6F00,0x50F6F00, 0x00,0x00, 0xD,+0 }, // 890: qGP73; Short Guiro
    { 0x50F4F00,0x50F4F00, 0x00,0x00, 0xD,+0 }, // 891: qGP74; Long Guiro
    { 0x3F40006,0x0F5F715, 0x3F,0x00, 0x0,+0 }, // 892: qGP75; Claves
    { 0x3F40006,0x0F5F715, 0x3F,0x08, 0x1,+0 }, // 893: qGP75; Claves
    { 0x3F40006,0x0F5F712, 0x3F,0x08, 0x1,+0 }, // 894: qGP76; qGP77; High Wood Block; Low Wood Block
    { 0x7476701,0x0476703, 0xCD,0x40, 0x8,+0 }, // 895: qGP78; Mute Cuica
    { 0x0476701,0x0556501, 0xC0,0x00, 0x9,+0 }, // 896: qGP78; Mute Cuica
    { 0x0A76701,0x0356503, 0x17,0x1E, 0xA,+0 }, // 897: qGP79; Open Cuica
    { 0x0777701,0x0057501, 0x9D,0x00, 0xB,+0 }, // 898: qGP79; Open Cuica
    { 0x3F0E00A,0x005FF1F, 0x40,0x40, 0x8,+0 }, // 899: qGP80; Mute Triangle
    { 0x3F0E00A,0x005FF1F, 0x40,0x48, 0x9,+0 }, // 900: qGP80; Mute Triangle
    { 0x3F0E00A,0x002FF1F, 0x7C,0x40, 0x8,+0 }, // 901: qGP81; Open Triangle
    { 0x3E0F50A,0x003FF1F, 0x7C,0x40, 0x9,+0 }, // 902: qGP81; Open Triangle
    { 0x04F7F0F,0x21E7E00, 0x40,0x88, 0xE,+0 }, // 903: qGP82; Shaker
    { 0x04F7F0F,0x21E7E00, 0x40,0x14, 0xF,+0 }, // 904: qGP82; Shaker
    { 0x332F905,0x0A6D604, 0x05,0x40, 0xE,+0 }, // 905: qGP83; Jingle Bell
    { 0x332F805,0x0A67404, 0x05,0x40, 0xF,+0 }, // 906: qGP83; Jingle Bell
    { 0x6E5E403,0x7E7F507, 0x0D,0x11, 0xB,+0 }, // 907: qGP84; Bell Tree
    { 0x366F500,0x4A8F604, 0x1B,0x15, 0xA,+0 }, // 908: qGP84; Bell Tree
    { 0x3F40003,0x0F5F715, 0x3F,0x00, 0x8,+0 }, // 909: qGP85; Castanets
    { 0x3F40003,0x0F5F715, 0x3F,0x08, 0x9,+0 }, // 910: qGP85; Castanets
    { 0x08DFA01,0x0B5F802, 0x4F,0x00, 0x6,+0 }, // 911: qGP86; Mute Surdu
    { 0x08DFA01,0x0B5F802, 0x4F,0x12, 0x7,+0 }, // 912: qGP86; Mute Surdu
    { 0x084FA01,0x0B4F800, 0x4F,0x00, 0x6,+0 }, // 913: qGP87; Open Surdu
    { 0x084FA01,0x0B4F800, 0x4F,0x00, 0x7,+0 }, // 914: qGP87; Open Surdu
    { 0x045F221,0x076F221, 0x8F,0x06, 0x8,+0 }, // 915: mGM0; AcouGrandPiano
    { 0x03BF271,0x00BF3A1, 0x0E,0x00, 0x6,+0 }, // 916: mGM3; Honky-tonkPiano
    { 0x054F60C,0x0B5F341, 0x5C,0x00, 0x0,+0 }, // 917: mGM8; Celesta
    { 0x0E6F318,0x0F6F241, 0x62,0x00, 0x0,+0 }, // 918: mGM11; Vibraphone
    { 0x082D385,0x0E3A341, 0x59,0x80, 0xC,+0 }, // 919: mGM14; Tubular Bells
    { 0x1557403,0x005B341, 0x49,0x80, 0x4,+0 }, // 920: mGM15; Dulcimer
    { 0x014F6B1,0x007F131, 0x92,0x00, 0x2,+0 }, // 921: mGM16; Hammond Organ
    { 0x058C7B2,0x008C730, 0x14,0x00, 0x2,+0 }, // 922: mGM17; Percussive Organ
    { 0x018AAB0,0x0088A71, 0x44,0x00, 0x4,+0 }, // 923: mGM18; Rock Organ
    { 0x1239723,0x0145571, 0x93,0x00, 0x4,+0 }, // 924: mGM19; Church Organ
    { 0x10497A1,0x0045571, 0x13,0x80, 0x0,+0 }, // 925: mGM20; Reed Organ
    { 0x12A9824,0x01A4671, 0x48,0x00, 0xC,+0 }, // 926: mGM21; Accordion
    { 0x10691A1,0x0076121, 0x13,0x00, 0xA,+0 }, // 927: mGM22; Harmonica
    { 0x0067121,0x0076161, 0x13,0x89, 0x6,+0 }, // 928: mGM23; Tango Accordion
    { 0x194F302,0x0C8F381, 0x9C,0x80, 0xC,+0 }, // 929: mGM24; Acoustic Guitar1
    { 0x04F2009,0x0F8D144, 0xA1,0x80, 0x8,+0 }, // 930: mGM31; Guitar Harmonics
    { 0x0069421,0x0A6C362, 0x1E,0x00, 0x2,+0 }, // 931: mGM32; Acoustic Bass
    { 0x11CD1B1,0x00C6131, 0x49,0x00, 0x8,+0 }, // 932: mGM42; Cello
    { 0x125B121,0x0087262, 0x9B,0x01, 0xE,+0 }, // 933: mGM48; String Ensemble1
    { 0x1037F61,0x1073F21, 0x98,0x00, 0x0,+0 }, // 934: mGM49; String Ensemble2
    { 0x012C161,0x0054FA1, 0x93,0x00, 0xA,+0 }, // 935: mGM50; Synth Strings 1
    { 0x022C121,0x0054FA1, 0x18,0x00, 0xC,+0 }, // 936: mGM51; SynthStrings 2
    { 0x015F431,0x0058AB2, 0x5B,0x83, 0x0,+0 }, // 937: mGM52; Choir Aahs
    { 0x0397461,0x06771A1, 0x90,0x00, 0x0,+0 }, // 938: mGM53; Voice Oohs
    { 0x00554B1,0x0057AB2, 0x57,0x00, 0xC,+0 }, // 939: mGM54; Synth Voice
    { 0x0635450,0x045A581, 0x00,0x00, 0x8,+0 }, // 940: mGM55; Orchestra Hit
    { 0x0157621,0x03782A1, 0x94,0x00, 0xC,+0 }, // 941: mGM58; Tuba
    { 0x01F75A1,0x00F7422, 0x8A,0x06, 0x8,+0 }, // 942: mGM61; Brass Section
    { 0x1557261,0x0187121, 0x86,0x0D, 0x0,+0 }, // 943: mGM62; Synth Brass 1
    { 0x1029331,0x00B72A1, 0x8F,0x00, 0x8,+0 }, // 944: mGM64; Soprano Sax
    { 0x1039331,0x00982A1, 0x91,0x00, 0xA,+0 }, // 945: mGM66; Tenor Sax
    { 0x10F9331,0x00F72A1, 0x8E,0x00, 0xA,+0 }, // 946: mGM67; Baritone Sax
    { 0x01F7561,0x00A7521, 0x9C,0x00, 0x2,+0 }, // 947: mGM74; Recorder
    { 0x05666E1,0x0266561, 0x4C,0x00, 0x0,+0 }, // 948: mGM76; Bottle Blow
    { 0x04676A2,0x0365561, 0xCB,0x00, 0x0,+0 }, // 949: mGM77; Shakuhachi
    { 0x00757A2,0x0075661, 0x99,0x00, 0xB,+0 }, // 950: mGM78; Whistle
    { 0x00777A2,0x0077661, 0x93,0x00, 0xB,+0 }, // 951: mGM79; Ocarina
    { 0x0126621,0x00A9661, 0x45,0x00, 0x0,+0 }, // 952: mGM83; Lead 4 chiff
    { 0x005DF62,0x0076FA1, 0x9E,0x40, 0x2,+0 }, // 953: mGM85; Lead 6 voice
    { 0x001EF20,0x2068FA0, 0x1A,0x00, 0x0,+0 }, // 954: mGM86; Lead 7 fifths
    { 0x09453B7,0x005A061, 0xA5,0x00, 0x2,+0 }, // 955: mGM88; Pad 1 new age
    { 0x011A8A1,0x0032571, 0x1F,0x80, 0xA,+0 }, // 956: mGM89; Pad 2 warm
    { 0x03491A1,0x01655A1, 0x17,0x00, 0xC,+0 }, // 957: mGM90; Pad 3 polysynth
    { 0x00154B1,0x0036AB2, 0x5D,0x00, 0x0,+0 }, // 958: mGM91; Pad 4 choir
    { 0x0432121,0x0354262, 0x97,0x00, 0x8,+0 }, // 959: mGM92; Pad 5 bowedpad
    { 0x177A161,0x1473121, 0x1C,0x00, 0x0,+0 }, // 960: mGM93; Pad 6 metallic
    { 0x0331121,0x02542A1, 0x89,0x03, 0xA,+0 }, // 961: mGM94; Pad 7 halo
    { 0x1471161,0x007CF21, 0x15,0x00, 0x0,+0 }, // 962: mGM95; Pad 8 sweep
    { 0x0F6F83A,0x0028691, 0xCE,0x00, 0x2,+0 }, // 963: mGM96; FX 1 rain
    { 0x081B122,0x026F2A1, 0x92,0x83, 0xC,+0 }, // 964: mGM99; FX 4 atmosphere
    { 0x151F181,0x0F5F282, 0x4D,0x00, 0x0,+0 }, // 965: mGM100; FX 5 brightness
    { 0x15111A1,0x0131163, 0x94,0x80, 0x6,+0 }, // 966: mGM101; FX 6 goblins
    { 0x03111A1,0x0031D61, 0x8C,0x80, 0x6,+0 }, // 967: mGM102; FX 7 echoes
    { 0x173F364,0x02381A1, 0x4C,0x00, 0x4,+0 }, // 968: mGM103; FX 8 sci-fi
    { 0x032D453,0x111EB51, 0x91,0x00, 0x8,+0 }, // 969: mGM107; Koto
    { 0x000F68E,0x3029F5E, 0x00,0x00, 0xE,+0 }, // 970: mGP55; Splash Cymbal
    { 0x303FF40,0x014FF10, 0x00,0x0D, 0xC,+0 }, // 971: mGP58; Vibraslap
    { 0x306F640,0x3176711, 0x00,0x00, 0xE,+0 }, // 972: mGP73; Short Guiro
    { 0x205F540,0x3164611, 0x00,0x09, 0xE,+0 }, // 973: mGP74; Long Guiro
    { 0x048F881,0x0057582, 0x45,0x08, 0x0,+0 }, // 974: mGP79; Open Cuica
    { 0x053F101,0x074D211, 0x4F,0x00, 0x6,+0 }, // 975: MGM0; MGM7; AcouGrandPiano; Clavinet
    { 0x050F101,0x07CD201, 0x4F,0x04, 0x6,+0 }, // 976: MGM1; MGM3; MGM5; BrightAcouGrand; Chorused Piano; Honky-tonkPiano
    { 0x060F101,0x07BD211, 0x4D,0x00, 0x8,+0 }, // 977: MGM2; ElecGrandPiano
    { 0x013F202,0x043F502, 0x22,0x00, 0xE,+0 }, // 978: MGM4; Rhodes Piano
    { 0x053F101,0x083F212, 0xCF,0x00, 0x2,+0 }, // 979: MGM6; Harpsichord
    { 0x00FFF24,0x00FFF21, 0x80,0x80, 0x1,+0 }, // 980: MGM8; Celesta
    { 0x00381A5,0x005F1B2, 0xD2,0x80, 0x2,+0 }, // 981: MGM10; MGM9; Glockenspiel; Music box
    { 0x0F0FB3E,0x09BA0B1, 0x29,0x40, 0x0,+0 }, // 982: MGM11; oGM11; Vibraphone
    { 0x00381A5,0x005F1B1, 0xD2,0x40, 0x2,+0 }, // 983: MGM12; MGM13; MGM14; Marimba; Tubular Bells; Xylophone
    { 0x00BF224,0x00B5231, 0x4F,0x00, 0xE,+0 }, // 984: MGM15; Dulcimer
    { 0x001F211,0x0B1F215, 0x0D,0x0D, 0xA,+0 }, // 985: MGM16; MGM17; MGM18; Hammond Organ; Percussive Organ; Rock Organ
    { 0x153F101,0x274F111, 0x49,0x00, 0x6,+0 }, // 986: MGM19; MGM20; MGM21; Accordion; Church Organ; Reed Organ
    { 0x0100133,0x0337D14, 0x87,0x80, 0x8,+0 }, // 987: MGM22; MGM23; Harmonica; Tango Accordion
    { 0x0AE71E1,0x09E81E1, 0x16,0x00, 0xA,+0 }, // 988: MGM24; MGM25; MGM26; MGM27; oGM26; Acoustic Guitar1; Acoustic Guitar2; Electric Guitar1; Electric Guitar2
    { 0x0EFF201,0x078F501, 0x1D,0x00, 0xA,+0 }, // 989: MGM28; MGM29; MGM30; MGM31; MGM44; MGM45; MGM65; MGM66; MGM67; oGM28; oGM29; oGM30; oGM31; oGM44; oGM45; oGM65; oGM66; oGM67; Alto Sax; Baritone Sax; Distorton Guitar; Electric Guitar3; Guitar Harmonics; Overdrive Guitar; Pizzicato String; Tenor Sax; Tremulo Strings
    { 0x002A474,0x04245D7, 0x47,0x40, 0x6,+0 }, // 990: MGM32; Acoustic Bass
    { 0x05331C5,0x07242D9, 0x8F,0x00, 0x6,+0 }, // 991: MGM125; MGM33; MGM36; Electric Bass 1; Helicopter; Slap Bass 1
    { 0x0022A95,0x0F34212, 0x97,0x80, 0x0,+0 }, // 992: MGM34; Electric Bass 2
    { 0x001EF4F,0x0F19801, 0x81,0x00, 0x4,+0 }, // 993: MGM35; Fretless Bass
    { 0x019D530,0x01B61B1, 0x88,0x80, 0xC,+0 }, // 994: MGM39; oGM39; Synth Bass 2
    { 0x060F207,0x072F212, 0x4F,0x00, 0x8,+0 }, // 995: MGM126; MGM40; MGM41; Applause/Noise; Viola; Violin
    { 0x0176E71,0x00E8B22, 0xC5,0x05, 0x2,+0 }, // 996: MGM42; oGM42; Cello
    { 0x02F2501,0x06C6521, 0x15,0x80, 0xA,+0 }, // 997: MGM43; MGM70; MGM71; Bassoon; Clarinet; Contrabass
    { 0x0427847,0x0548554, 0x4D,0x00, 0xA,+0 }, // 998: MGM46; oGM46; Orchestral Harp
    { 0x011F111,0x0B3F101, 0x4A,0x88, 0x6,+0 }, // 999: MGM47; Timpany
    { 0x0117171,0x11542A1, 0x8B,0x40, 0x6,+0 }, // 1000: MGM48; MGM50; String Ensemble1; Synth Strings 1
    { 0x3D3B1E1,0x1741221, 0x4F,0x00, 0x6,+0 }, // 1001: MGM49; String Ensemble2
    { 0x053090E,0x094F702, 0x80,0x00, 0xE,+0 }, // 1002: MGM105; MGM51; oGM105; Banjo; SynthStrings 2
    { 0x0035172,0x0135262, 0x1C,0x05, 0xE,+0 }, // 1003: MGM52; MGM54; oGM52; oGM54; Choir Aahs; Synth Voice
    { 0x0035131,0x06754A1, 0x1C,0x80, 0xE,+0 }, // 1004: MGM53; Voice Oohs
    { 0x0115270,0x0FE3171, 0xC5,0x40, 0x0,+0 }, // 1005: MGM55; Orchestra Hit
    { 0x0B69401,0x0268300, 0x00,0x00, 0x0,+0 }, // 1006: MGM56; Trumpet
    { 0x075F502,0x0F3F201, 0x29,0x83, 0x0,+0 }, // 1007: MGM57; Trombone
    { 0x243A321,0x022C411, 0x11,0x00, 0xC,+0 }, // 1008: MGM58; Tuba
    { 0x01FF201,0x088F501, 0x11,0x00, 0xA,+0 }, // 1009: MGM59; MGM60; French Horn; Muted Trumpet
    { 0x021FF13,0x003FF11, 0x96,0x80, 0xA,+0 }, // 1010: MGM61; Brass Section
    { 0x01797F1,0x018F121, 0x01,0x0D, 0x8,+0 }, // 1011: MGM62; Synth Brass 1
    { 0x053F101,0x053F108, 0x40,0x40, 0x0,+0 }, // 1012: MGM63; Synth Brass 2
    { 0x021FF13,0x003FF10, 0x51,0x40, 0xA,+0 }, // 1013: MGM64; MGM68; MGM69; oGM68; oGM69; English Horn; Oboe; Soprano Sax
    { 0x08F7461,0x02A6561, 0x27,0x80, 0x2,+0 }, // 1014: MGM72; MGM73; MGM74; MGM75; Flute; Pan Flute; Piccolo; Recorder
    { 0x08F6EE0,0x02A65A1, 0xEC,0x00, 0xE,+0 }, // 1015: MGM110; MGM111; MGM76; MGM77; oGM110; oGM111; oGM77; Bottle Blow; Fiddle; Shakuhachi; Shanai
    { 0x0537101,0x07C5212, 0x4F,0x00, 0xA,+0 }, // 1016: MGM78; MGM79; MGM80; Lead 1 squareea; Ocarina; Whistle
    { 0x0667150,0x08B5290, 0x92,0x00, 0xE,+0 }, // 1017: MGM81; Lead 2 sawtooth
    { 0x0247332,0x0577521, 0x16,0x80, 0xE,+0 }, // 1018: MGM82; Lead 3 calliope
    { 0x01B5132,0x03BA2A1, 0x9A,0x82, 0xC,+0 }, // 1019: MGM83; Lead 4 chiff
    { 0x0176E71,0x00E8B62, 0xC5,0x05, 0x2,+0 }, // 1020: MGM84; MGM85; Lead 5 charang; Lead 6 voice
    { 0x019D530,0x01B61B1, 0xCD,0x40, 0xC,+0 }, // 1021: MGM86; Lead 7 fifths
    { 0x2034122,0x10561F2, 0x4F,0x80, 0x2,+0 }, // 1022: MGM87; oGM87; Lead 8 brass
    { 0x00B4131,0x03B92A1, 0x1C,0x80, 0xC,+0 }, // 1023: MGM88; MGM89; Pad 1 new age; Pad 2 warm
    { 0x01D5321,0x03B52A1, 0x1C,0x80, 0xC,+0 }, // 1024: MGM90; Pad 3 polysynth
    { 0x01F4171,0x03B92A1, 0x1C,0x80, 0xE,+0 }, // 1025: MGM91; Pad 4 choir
    { 0x05A5321,0x01AAA21, 0x9F,0x80, 0xC,+0 }, // 1026: MGM92; Pad 5 bowedpad
    { 0x28FA520,0x03D3621, 0x8E,0x00, 0x6,+0 }, // 1027: MGM93; Pad 6 metallic
    { 0x08C4321,0x02F8521, 0x19,0x80, 0xC,+0 }, // 1028: MGM94; Pad 7 halo
    { 0x0AE7121,0x09E8121, 0x16,0x00, 0xE,+0 }, // 1029: MGM95; oGM95; Pad 8 sweep
    { 0x0AE7161,0x02E8160, 0x1C,0x00, 0xE,+0 }, // 1030: MGM96; oGM96; FX 1 rain
    { 0x054F606,0x0B3F241, 0x73,0x03, 0x0,+0 }, // 1031: MGM97; FX 2 soundtrack
    { 0x212AA53,0x021AC51, 0x97,0x80, 0xE,+0 }, // 1032: MGM98; FX 3 crystal
    { 0x025DA05,0x015F901, 0x8E,0x00, 0xA,+0 }, // 1033: MGM104; MGM99; oGM104; oGM99; FX 4 atmosphere; Sitar
    { 0x001FFA4,0x0F3F53E, 0xDB,0xC0, 0x4,+0 }, // 1034: MGM100; MGM101; MGM102; oGM100; oGM101; oGM102; FX 5 brightness; FX 6 goblins; FX 7 echoes
    { 0x22F55B0,0x31E87E0, 0x16,0x80, 0xC,+0 }, // 1035: MGM106; oGM106; Shamisen
    { 0x0177421,0x0176562, 0x83,0x8D, 0x7,+0 }, // 1036: MGM107; MGM108; MGM109; oGM108; oGM109; Bagpipe; Kalimba; Koto
    { 0x0EFE800,0x0FFA500, 0x0D,0x00, 0x6,+0 }, // 1037: MGM112; Tinkle Bell
    { 0x2A2B264,0x1D49703, 0x02,0x80, 0xE,+0 }, // 1038: MGM119; Reverse Cymbal
    { 0x0F3F8E2,0x0F3F770, 0x86,0x40, 0x4,+0 }, // 1039: MGM120; Guitar FretNoise
    { 0x0F0E026,0x031FF1E, 0x03,0x00, 0x8,+0 }, // 1040: MGM121; Breath Noise
    { 0x0056541,0x0743291, 0x83,0x00, 0xA,+0 }, // 1041: MGM122; Seashore
    { 0x061F217,0x0B2F112, 0x4F,0x08, 0x8,+0 }, // 1042: MGM123; oGM123; Bird Tweet
    { 0x0F0F000,0x0F05F0C, 0x2E,0x00, 0xE,+0 }, // 1043: MGM124; Telephone
    { 0x0031801,0x090F674, 0x80,0xC1, 0xE,+0 }, // 1044: MGM127; oGM127; Gunshot
    { 0x04CA800,0x04FD600, 0x0B,0x03, 0x0,+0 }, // 1045: MGP35; MGP36; Ac Bass Drum; Bass Drum 1
    { 0x00FFF2E,0x04CF600, 0x00,0x18, 0xE,+0 }, // 1046: MGP38; MGP39; MGP40; MGP67; MGP68; Acoustic Snare; Electric Snare; Hand Clap; High Agogo; Low Agogo
    { 0x282B264,0x1DA9803, 0x00,0x93, 0xE,+0 }, // 1047: MGP42; Closed High Hat
    { 0x282B264,0x1D49703, 0x00,0x80, 0xE,+0 }, // 1048: MGP44; MGP46; MGP51; MGP54; MGP69; MGP70; MGP71; MGP72; MGP73; MGP75; Cabasa; Claves; Long Whistle; Maracas; Open High Hat; Pedal High Hat; Ride Cymbal 1; Short Guiro; Short Whistle; Tambourine
    { 0x0A0B264,0x1D69603, 0x02,0x80, 0xE,+0 }, // 1049: MGP49; Crash Cymbal 1
    { 0x053F101,0x074F111, 0x4B,0x00, 0x6,+0 }, // 1050: oGM0; oGM1; oGM2; AcouGrandPiano; BrightAcouGrand; ElecGrandPiano
    { 0x0117F27,0x0441122, 0x0E,0x00, 0xE,+0 }, // 1051: oGM3; Honky-tonkPiano
    { 0x0111122,0x0121123, 0x15,0x00, 0x4,+0 }, // 1052: oGM4; Rhodes Piano
    { 0x053F101,0x074F111, 0x59,0x00, 0x6,+0 }, // 1053: oGM5; Chorused Piano
    { 0x0FFF691,0x0F4F511, 0x00,0x00, 0x8,+0 }, // 1054: oGM6; Harpsichord
    { 0x3087631,0x00F6531, 0x08,0x00, 0x2,+0 }, // 1055: oGM7; Clavinet
    { 0x019D083,0x017F002, 0x5D,0x80, 0xA,+0 }, // 1056: oGM8; Celesta
    { 0x019D083,0x017F002, 0x58,0x80, 0xA,+0 }, // 1057: oGM9; Glockenspiel
    { 0x013F6A6,0x005F1B1, 0xE5,0x40, 0x2,+0 }, // 1058: oGM10; Music box
    { 0x1239722,0x013457A, 0x44,0x00, 0x4,+0 }, // 1059: oGM12; Marimba
    { 0x1239721,0x0134572, 0x8A,0x80, 0x2,+0 }, // 1060: oGM13; Xylophone
    { 0x0FFF4F1,0x06FF2F1, 0x02,0x00, 0x0,+0 }, // 1061: oGM14; Tubular Bells
    { 0x00F3FF1,0x06FF2F1, 0x02,0x00, 0x0,+0 }, // 1062: oGM15; Dulcimer
    { 0x000A121,0x0F6F236, 0x80,0x00, 0x8,+0 }, // 1063: oGM16; oGM17; oGM18; Hammond Organ; Percussive Organ; Rock Organ
    { 0x085F211,0x0B7F212, 0x87,0x80, 0x4,+0 }, // 1064: oGM19; oGM20; oGM21; Accordion; Church Organ; Reed Organ
    { 0x054F607,0x0B6F242, 0x73,0x00, 0x0,+0 }, // 1065: oGM22; Harmonica
    { 0x054F60E,0x0B6F242, 0x73,0x00, 0x0,+0 }, // 1066: oGM23; Tango Accordion
    { 0x1E26301,0x01EB821, 0x16,0x00, 0x8,+0 }, // 1067: oGM24; Acoustic Guitar1
    { 0x1226341,0x01E8821, 0x8F,0x00, 0x8,+0 }, // 1068: oGM25; Acoustic Guitar2
    { 0x0024471,0x01E8831, 0x9D,0x00, 0xE,+0 }, // 1069: oGM27; Electric Guitar2
    { 0x002A434,0x0427575, 0x54,0x40, 0x8,+0 }, // 1070: oGM32; Acoustic Bass
    { 0x00211B1,0x0034231, 0x93,0x80, 0x0,+0 }, // 1071: oGM33; Electric Bass 1
    { 0x0023AB1,0x0134232, 0xAF,0x80, 0x0,+0 }, // 1072: oGM34; Electric Bass 2
    { 0x256F605,0x2047404, 0xC0,0x00, 0xE,+0 }, // 1073: oGM35; Fretless Bass
    { 0x05312C4,0x07212F1, 0x10,0x00, 0x2,+0 }, // 1074: oGM36; Slap Bass 1
    { 0x061F217,0x074F212, 0x4F,0x00, 0x8,+0 }, // 1075: oGM38; Synth Bass 1
    { 0x02FA433,0x0117575, 0x14,0x00, 0x0,+0 }, // 1076: oGM40; Violin
    { 0x0FFF09E,0x00F3F00, 0x07,0x00, 0xE,+0 }, // 1077: oGM41; Viola
    { 0x0124D01,0x013F501, 0x02,0x00, 0x7,+0 }, // 1078: oGM43; Contrabass
    { 0x20FFF22,0x00FFF21, 0x5A,0x80, 0x0,+0 }, // 1079: oGM47; Timpany
    { 0x1217131,0x0066222, 0x40,0x40, 0x2,+0 }, // 1080: oGM48; String Ensemble1
    { 0x121F131,0x0166F21, 0x40,0x00, 0x2,+0 }, // 1081: oGM49; String Ensemble2
    { 0x131F231,0x0066F21, 0x47,0x00, 0x0,+0 }, // 1082: oGM50; Synth Strings 1
    { 0x175F502,0x0F8F501, 0x58,0x80, 0x0,+0 }, // 1083: oGM51; SynthStrings 2
    { 0x0035131,0x06764A1, 0x1C,0x80, 0xE,+0 }, // 1084: oGM53; Voice Oohs
    { 0x0115270,0x0FE4171, 0xC5,0x40, 0x0,+0 }, // 1085: oGM55; Orchestra Hit
    { 0x1218131,0x0167423, 0x4D,0x40, 0x2,+0 }, // 1086: oGM56; Trumpet
    { 0x151D203,0x278F301, 0x1D,0x00, 0xA,+0 }, // 1087: oGM59; Muted Trumpet
    { 0x0F0F09E,0x063F300, 0x07,0x00, 0xE,+0 }, // 1088: oGM60; French Horn
    { 0x0F7B096,0x00FFFE0, 0x00,0x00, 0x0,+0 }, // 1089: oGM61; Brass Section
    { 0x3199B85,0x0297424, 0x49,0x00, 0x6,+0 }, // 1090: oGM62; Synth Brass 1
    { 0x0FFA691,0x0F45511, 0x00,0x00, 0x8,+0 }, // 1091: oGM63; Synth Brass 2
    { 0x05FF561,0x02AF562, 0x21,0x00, 0x2,+0 }, // 1092: oGM64; Soprano Sax
    { 0x04F6421,0x028F231, 0x91,0x00, 0xA,+0 }, // 1093: oGM70; oGM71; Bassoon; Clarinet
    { 0x05FF561,0x05A6661, 0x1E,0x00, 0x2,+0 }, // 1094: oGM72; Piccolo
    { 0x05FF561,0x02A7561, 0x1E,0x07, 0x2,+0 }, // 1095: oGM73; oGM74; Flute; Recorder
    { 0x03FF561,0x01A7562, 0x28,0x04, 0x2,+0 }, // 1096: oGM75; Pan Flute
    { 0x01F7561,0x02A7561, 0x21,0x00, 0x2,+0 }, // 1097: oGM76; Bottle Blow
    { 0x1226341,0x000A821, 0x8F,0x00, 0x8,+0 }, // 1098: oGM78; Whistle
    { 0x1239721,0x0136572, 0x8A,0x80, 0x2,+0 }, // 1099: oGM79; Ocarina
    { 0x061F217,0x074F212, 0x6C,0x00, 0x8,+0 }, // 1100: oGM80; Lead 1 squareea
    { 0x1239721,0x0138572, 0x8A,0x80, 0x2,+0 }, // 1101: oGM81; Lead 2 sawtooth
    { 0x0217B32,0x0176221, 0x95,0x00, 0x0,+0 }, // 1102: oGM82; Lead 3 calliope
    { 0x0219B32,0x0176221, 0x97,0x00, 0x0,+0 }, // 1103: oGM83; Lead 4 chiff
    { 0x0115231,0x11E3132, 0xC5,0x00, 0x8,+0 }, // 1104: oGM84; Lead 5 charang
    { 0x1177E31,0x10C8B21, 0x43,0x00, 0x2,+0 }, // 1105: oGM85; Lead 6 voice
    { 0x019D520,0x11B6121, 0x93,0x00, 0xC,+0 }, // 1106: oGM86; Lead 7 fifths
    { 0x00D5131,0x01F7221, 0x1C,0x80, 0xE,+0 }, // 1107: oGM88; oGM89; Pad 1 new age; Pad 2 warm
    { 0x00D5131,0x01F7221, 0x1C,0x80, 0xC,+0 }, // 1108: oGM90; oGM91; Pad 3 polysynth; Pad 4 choir
    { 0x06A6121,0x00A7F21, 0x26,0x00, 0x2,+0 }, // 1109: oGM92; oGM93; Pad 5 bowedpad; Pad 6 metallic
    { 0x08C6320,0x02F9520, 0x19,0x80, 0xC,+0 }, // 1110: oGM94; Pad 7 halo
    { 0x033F5C5,0x025FDE1, 0x53,0x80, 0xA,+0 }, // 1111: oGM97; oGM98; FX 2 soundtrack; FX 3 crystal
    { 0x05FF561,0x01A6661, 0x1E,0x00, 0x2,+0 }, // 1112: oGM107; Koto
    { 0x1F5E510,0x162E231, 0x46,0x00, 0x0,+0 }, // 1113: oGM112; Tinkle Bell
    { 0x24FF60E,0x318F700, 0x40,0x00, 0xE,+0 }, // 1114: oGM114; oGP38; oGP40; Acoustic Snare; Electric Snare; Steel Drums
    { 0x0C8F60C,0x257FF12, 0xC0,0x00, 0xA,+0 }, // 1115: oGM115; oGP42; Closed High Hat; Woodblock
    { 0x354B506,0x095D507, 0x00,0xC0, 0x0,+0 }, // 1116: oGM116; oGM119; oGP51; Reverse Cymbal; Ride Cymbal 1; Taiko Drum
    { 0x04CA800,0x13FD600, 0x0B,0x00, 0x0,+0 }, // 1117: oGM117; oGM120; oGP37; oGP39; oGP41; oGP43; oGP45; oGP47; oGP48; oGP50; Guitar FretNoise; Hand Clap; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; Melodic Tom; Side Stick
    { 0x0F0E02A,0x031FF1E, 0x52,0x54, 0x8,+0 }, // 1118: oGM121; Breath Noise
    { 0x0745451,0x0756591, 0x00,0x00, 0xA,+0 }, // 1119: oGM122; Seashore
    { 0x002A414,0x0427555, 0x54,0x40, 0x8,+0 }, // 1120: oGM124; Telephone
    { 0x0115F31,0x11E3132, 0xC5,0x00, 0x8,+0 }, // 1121: oGM125; Helicopter
    { 0x1217131,0x0069222, 0x40,0x40, 0x2,+0 }, // 1122: oGM126; Applause/Noise
    { 0x000F60E,0x3059F10, 0x00,0x00, 0xE,+0 }, // 1123: oGP44; Pedal High Hat
    { 0x000F60E,0x3039F10, 0x00,0x00, 0xE,+0 }, // 1124: oGP46; Open High Hat
    { 0x0C5F59E,0x2F7F70E, 0x00,0x00, 0xF,+0 }, // 1125: oGP54; Tambourine
    { 0x0BFFA01,0x097C803, 0x00,0x00, 0x7,+0 }, // 1126: oGP60; High Bongo
    { 0x053F101,0x0B5F704, 0x4F,0x00, 0x7,+0 }, // 1127: oGP62; oGP63; oGP64; Low Conga; Mute High Conga; Open High Conga
    { 0x04FF60E,0x218F700, 0x40,0x00, 0xE,+0 }, // 1128: oGP69; oGP70; Cabasa; Maracas
};
const struct adlinsdata adlins[1085] =
{
    // Amplitude begins at 2487.8, peaks 3203.1 at 0.1s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    {   0,  0,  0,   1560,  1560 }, // 0: GM0; sGM0; AcouGrandPiano

    // Amplitude begins at 3912.6,
    // fades to 20% at 2.1s, keyoff fades to 20% in 2.1s.
    {   1,  1,  0,   2080,  2080 }, // 1: GM1; sGM1; BrightAcouGrand

    // Amplitude begins at 2850.7, peaks 4216.6 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    {   2,  2,  0,   1973,  1973 }, // 2: GM2; sGM2; ElecGrandPiano

    // Amplitude begins at 1714.3, peaks 1785.0 at 0.1s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    {   3,  3,  0,   2226,  2226 }, // 3: GM3; sGM3; Honky-tonkPiano

    // Amplitude begins at 4461.0, peaks 6341.0 at 0.0s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    {   4,  4,  0,   2606,  2606 }, // 4: GM4; sGM4; Rhodes Piano

    // Amplitude begins at 4781.0, peaks 6329.2 at 0.0s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    {   5,  5,  0,   2640,  2640 }, // 5: GM5; sGM5; Chorused Piano

    // Amplitude begins at 1162.2, peaks 1404.5 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    {   6,  6,  0,   1186,  1186 }, // 6: GM6; Harpsichord

    // Amplitude begins at 1144.6, peaks 1235.5 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    {   7,  7,  0,   2313,  2313 }, // 7: GM7; sGM7; Clavinet

    // Amplitude begins at 2803.9, peaks 2829.0 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {   8,  8,  0,    906,   906 }, // 8: GM8; sGM8; Celesta

    // Amplitude begins at 3085.2, peaks 3516.8 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    {   9,  9,  0,   1120,  1120 }, // 9: GM9; Glockenspiel

    // Amplitude begins at 2073.6, peaks 3449.1 at 0.1s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  10, 10,  0,    453,   453 }, // 10: GM10; sGM10; Music box

    // Amplitude begins at 2976.7, peaks 3033.0 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    {  11, 11,  0,   1746,  1746 }, // 11: GM11; sGM11; Vibraphone

    // Amplitude begins at 3371.2, peaks 3554.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    {  12, 12,  0,     80,    80 }, // 12: GM12; sGM12; Marimba

    // Amplitude begins at 2959.7, peaks 3202.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    {  13, 13,  0,    100,   100 }, // 13: GM13; sGM13; Xylophone

    // Amplitude begins at 2057.2, peaks 2301.4 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    {  14, 14,  0,   1306,  1306 }, // 14: GM14; Tubular Bells

    // Amplitude begins at 1672.7, peaks 2154.8 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  15, 15,  0,    320,   320 }, // 15: GM15; sGM15; Dulcimer

    // Amplitude begins at 2324.3, peaks 2396.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  16, 16,  0,  40000,    40 }, // 16: GM16; HMIGM16; sGM16; Hammond Organ; am016.in

    // Amplitude begins at 2299.6, peaks 2620.0 at 14.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  17, 17,  0,  40000,    20 }, // 17: GM17; HMIGM17; sGM17; Percussive Organ; am017.in

    // Amplitude begins at  765.8, peaks 2166.1 at 4.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  18, 18,  0,  40000,    20 }, // 18: GM18; HMIGM18; sGM18; Rock Organ; am018.in

    // Amplitude begins at  336.5, peaks 2029.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  19, 19,  0,  40000,   500 }, // 19: GM19; HMIGM19; Church Organ; am019.in

    // Amplitude begins at  153.2, peaks 4545.1 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  20, 20,  0,  40000,   280 }, // 20: GM20; HMIGM20; sGM20; Reed Organ; am020.in

    // Amplitude begins at    0.0, peaks  883.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  21, 21,  0,  40000,     6 }, // 21: GM21; HMIGM21; sGM21; Accordion; am021.in

    // Amplitude begins at  181.4, peaks 3015.8 at 25.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  22, 22,  0,  40000,    86 }, // 22: GM22; HMIGM22; sGM22; Harmonica; am022.in

    // Amplitude begins at    3.2, peaks 3113.2 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  23, 23,  0,  40000,    73 }, // 23: GM23; HMIGM23; sGM23; Tango Accordion; am023.in

    // Amplitude begins at  955.9, peaks 1147.3 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    {  24, 24,  0,   1120,  1120 }, // 24: GM24; HMIGM24; Acoustic Guitar1; am024.in

    // Amplitude begins at 1026.8, peaks 1081.7 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    {  25, 25,  0,   1973,  1973 }, // 25: GM25; HMIGM25; mGM25; sGM25; Acoustic Guitar2; am025.in

    // Amplitude begins at 4157.9, peaks 4433.1 at 0.1s,
    // fades to 20% at 10.3s, keyoff fades to 20% in 10.3s.
    {  26, 26,  0,  10326, 10326 }, // 26: GM26; HMIGM26; mGM26; sGM26; Electric Guitar1; am026.in

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    {  27, 27,  0,   1266,  1266 }, // 27: GM27; sGM27; Electric Guitar2

    // Amplitude begins at 3418.1,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  28, 28,  0,  40000,     0 }, // 28: GM28; HMIGM28; hamM3; hamM60; intM3; mGM28; rickM3; sGM28; BPerc; BPerc.in; Electric Guitar3; am028.in; muteguit

    // Amplitude begins at   57.7, peaks 1476.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  29, 29,  0,  40000,    13 }, // 29: GM29; sGM29; Overdrive Guitar

    // Amplitude begins at  396.9, peaks 1480.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  30, 30,  0,  40000,    13 }, // 30: GM30; HMIGM30; hamM6; intM6; mGM30; rickM6; sGM30; Distorton Guitar; GDist; GDist.in; am030.in

    // Amplitude begins at 1424.2, peaks 2686.1 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  31, 31,  0,  40000,     0 }, // 31: GM31; HMIGM31; hamM5; intM5; rickM5; sGM31; GFeedbck; Guitar Harmonics; am031.in

    // Amplitude begins at 2281.6, peaks 2475.5 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    {  32, 32,  0,   1153,  1153 }, // 32: GM32; HMIGM32; sGM32; Acoustic Bass; am032.in

    // Amplitude begins at 1212.1, peaks 1233.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  33, 33,  0,  40000,    13 }, // 33: GM33; GM39; HMIGM33; HMIGM39; hamM68; mGM33; mGM39; sGM33; sGM39; Electric Bass 1; Synth Bass 2; am033.in; am039.in; synbass2

    // Amplitude begins at 3717.2, peaks 4282.2 at 0.1s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    {  34, 34,  0,   1540,  1540 }, // 34: GM34; HMIGM34; mGM34; rickM81; sGM34; Electric Bass 2; Slapbass; am034.in

    // Amplitude begins at   49.0, peaks 3572.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  35, 35,  0,  40000,     0 }, // 35: GM35; HMIGM35; mGM35; sGM35; Fretless Bass; am035.in

    // Amplitude begins at 1755.7, peaks 2777.7 at 0.0s,
    // fades to 20% at 3.6s, keyoff fades to 20% in 3.6s.
    {  36, 36,  0,   3566,  3566 }, // 36: GM36; HMIGM36; mGM36; sGM36; Slap Bass 1; am036.in

    // Amplitude begins at 1352.0, peaks 2834.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  37, 37,  0,  40000,    53 }, // 37: GM37; sGM37; Slap Bass 2

    // Amplitude begins at 3787.6,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  38, 38,  0,    540,   540 }, // 38: GM38; HMIGM38; hamM13; hamM67; intM13; mGM38; rickM13; sGM38; BSynth3; BSynth3.; Synth Bass 1; am038.in; synbass1

    // Amplitude begins at 1114.5, peaks 2586.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  39, 39,  0,  40000,   106 }, // 39: GM40; HMIGM40; mGM40; sGM40; Violin; am040.in

    // Amplitude begins at    0.5, peaks 1262.2 at 16.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  40, 40,  0,  40000,   126 }, // 40: GM41; HMIGM41; mGM41; sGM41; Viola; am041.in

    // Amplitude begins at 1406.9, peaks 2923.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  41, 41,  0,  40000,     6 }, // 41: GM42; HMIGM42; sGM42; Cello; am042.in

    // Amplitude begins at    5.0, peaks 2928.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  42, 42,  0,  40000,   193 }, // 42: GM43; HMIGM43; mGM43; sGM43; Contrabass; am043.in

    // Amplitude begins at    0.6, peaks 1972.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  43, 43,  0,  40000,   106 }, // 43: GM44; HMIGM44; mGM44; Tremulo Strings; am044.in

    // Amplitude begins at  123.0, peaks 2834.7 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  44, 44,  0,    326,   326 }, // 44: GM45; HMIGM45; mGM45; Pizzicato String; am045.in

    // Amplitude begins at 2458.2, peaks 3405.7 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {  45, 45,  0,    853,   853 }, // 45: GM46; HMIGM46; mGM46; oGM57; oGM58; Orchestral Harp; Trombone; Tuba; am046.in

    // Amplitude begins at 2061.3, peaks 2787.1 at 0.1s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    {  46, 46,  0,    906,   906 }, // 46: GM47; HMIGM47; hamM14; intM14; mGM47; rickM14; BSynth4; BSynth4.; Timpany; am047.in

    // Amplitude begins at  845.0, peaks 1851.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  47, 47,  0,  40000,    66 }, // 47: GM48; HMIGM48; String Ensemble1; am048.in

    // Amplitude begins at    0.0, peaks 1531.6 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  48, 48,  0,  40000,    40 }, // 48: GM49; HMIGM49; String Ensemble2; am049.in

    // Amplitude begins at 2323.6, peaks 4179.7 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    {  49, 49,  0,  40000,   773 }, // 49: GM50; HMIGM50; hamM20; intM20; rickM20; PMellow; PMellow.; Synth Strings 1; am050.in

    // Amplitude begins at    0.0, peaks 1001.6 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  50, 50,  0,  40000,   280 }, // 50: GM51; HMIGM51; sGM51; SynthStrings 2; am051.in

    // Amplitude begins at  889.7, peaks 3770.3 at 20.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  51, 51,  0,  40000,   133 }, // 51: GM52; HMIGM52; rickM85; Choir Aahs; Choir.in; am052.in

    // Amplitude begins at    6.6, peaks 3005.9 at 0.2s,
    // fades to 20% at 4.3s, keyoff fades to 20% in 4.3s.
    {  52, 52,  0,   4346,  4346 }, // 52: GM53; HMIGM53; rickM86; sGM53; Oohs.ins; Voice Oohs; am053.in

    // Amplitude begins at   48.8, peaks 3994.2 at 19.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  53, 53,  0,  40000,   106 }, // 53: GM54; HMIGM54; sGM54; Synth Voice; am054.in

    // Amplitude begins at  899.0, peaks 1546.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    {  54, 54,  0,    260,   260 }, // 54: GM55; HMIGM55; Orchestra Hit; am055.in

    // Amplitude begins at   39.3, peaks  930.6 at 39.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  55, 55,  0,  40000,    20 }, // 55: GM56; HMIGM56; mGM56; Trumpet; am056.in

    // Amplitude begins at   40.6, peaks  925.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  56, 56,  0,  40000,    20 }, // 56: GM57; HMIGM57; mGM57; Trombone; am057.in

    // Amplitude begins at   39.5, peaks 1061.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  57, 57,  0,  40000,    26 }, // 57: GM58; HMIGM58; Tuba; am058.in

    // Amplitude begins at  886.2, peaks 2598.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  58, 58,  0,  40000,    20 }, // 58: GM59; HMIGM59; mGM59; sGM59; Muted Trumpet; am059.in

    // Amplitude begins at    6.8, peaks 4177.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  59, 59,  0,  40000,     6 }, // 59: GM60; HMIGM60; mGM60; French Horn; am060.in

    // Amplitude begins at    4.0, peaks 1724.3 at 21.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  60, 60,  0,  40000,     0 }, // 60: GM61; HMIGM61; Brass Section; am061.in

    // Amplitude begins at    7.2, peaks 2980.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  61, 61,  0,  40000,    20 }, // 61: GM62; sGM62; Synth Brass 1

    // Amplitude begins at  894.6, peaks 4216.8 at 0.1s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    {  62, 62,  0,    700,   700 }, // 62: GM63; HMIGM63; mGM63; sGM63; Synth Brass 2; am063.in

    // Amplitude begins at  674.0, peaks 3322.1 at 1.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  63, 63,  0,  40000,   126 }, // 63: GM64; HMIGM64; sGM64; Soprano Sax; am064.in

    // Amplitude begins at    3.5, peaks 1727.6 at 14.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  64, 64,  0,  40000,    20 }, // 64: GM65; HMIGM65; sGM65; Alto Sax; am065.in

    // Amplitude begins at  979.4, peaks 3008.4 at 27.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  65, 65,  0,  40000,   106 }, // 65: GM66; HMIGM66; sGM66; Tenor Sax; am066.in

    // Amplitude begins at    3.0, peaks 1452.8 at 14.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  66, 66,  0,  40000,     0 }, // 66: GM67; HMIGM67; sGM67; Baritone Sax; am067.in

    // Amplitude begins at  486.9, peaks 2127.8 at 39.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  67, 67,  0,  40000,    66 }, // 67: GM68; HMIGM68; mGM68; Oboe; am068.in

    // Amplitude begins at    5.0, peaks  784.7 at 34.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  68, 68,  0,  40000,     6 }, // 68: GM69; HMIGM69; mGM69; sGM69; English Horn; am069.in

    // Amplitude begins at   10.1, peaks 3370.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  69, 69,  0,  40000,     6 }, // 69: GM70; HMIGM70; mGM70; Bassoon; am070.in

    // Amplitude begins at    4.5, peaks 1934.6 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  70, 70,  0,  40000,    60 }, // 70: GM71; HMIGM71; mGM71; Clarinet; am071.in

    // Amplitude begins at   64.0, peaks 4162.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  71, 71,  0,  40000,     6 }, // 71: GM72; HMIGM72; mGM72; Piccolo; am072.in

    // Amplitude begins at    0.5, peaks 2752.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  72, 72,  0,  40000,     6 }, // 72: GM73; HMIGM73; mGM73; Flute; am073.in

    // Amplitude begins at    5.6, peaks 2782.3 at 34.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  73, 73,  0,  40000,     6 }, // 73: GM74; HMIGM74; sGM74; Recorder; am074.in

    // Amplitude begins at  360.9, peaks 4296.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  74, 74,  0,  40000,     6 }, // 74: GM75; HMIGM75; mGM75; sGM75; Pan Flute; am075.in

    // Amplitude begins at    0.8, peaks 2730.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  75, 75,  0,  40000,    40 }, // 75: GM76; HMIGM76; sGM76; Bottle Blow; am076.in

    // Amplitude begins at    7.6, peaks 3112.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  76, 76,  0,  40000,    46 }, // 76: GM77; HMIGM77; sGM77; Shakuhachi; am077.in

    // Amplitude begins at    0.0, peaks 2920.2 at 32.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  77, 77,  0,  40000,    73 }, // 77: GM78; HMIGM78; sGM78; Whistle; am078.in

    // Amplitude begins at    5.0, peaks 3460.4 at 13.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  78, 78,  0,  40000,    66 }, // 78: GM79; HMIGM79; hamM61; sGM79; Ocarina; am079.in; ocarina

    // Amplitude begins at 2183.6, peaks 2909.4 at 29.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    {  79, 79,  0,  40000,   453 }, // 79: GM80; HMIGM80; hamM16; hamM65; intM16; mGM80; rickM16; sGM80; LSquare; LSquare.; Lead 1 squareea; am080.in; squarewv

    // Amplitude begins at 1288.7, peaks 1362.9 at 35.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  80, 80,  0,  40000,     6 }, // 80: GM81; HMIGM81; mGM81; sGM81; Lead 2 sawtooth; am081.in

    // Amplitude begins at    0.4, peaks 3053.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  81, 81,  0,  40000,    26 }, // 81: GM82; HMIGM82; mGM82; sGM82; Lead 3 calliope; am082.in

    // Amplitude begins at  867.8, peaks 5910.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    {  82, 82,  0,  40000,   300 }, // 82: GM83; HMIGM83; sGM83; Lead 4 chiff; am083.in

    // Amplitude begins at  991.1, peaks 3848.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  83, 83,  0,  40000,     6 }, // 83: GM84; HMIGM84; mGM84; sGM84; Lead 5 charang; am084.in

    // Amplitude begins at    0.5, peaks 2501.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  84, 84,  0,  40000,    60 }, // 84: GM85; HMIGM85; hamM17; intM17; rickM17; rickM87; sGM85; Lead 6 voice; PFlutes; PFlutes.; Solovox.; am085.in

    // Amplitude begins at  113.3, peaks 1090.0 at 29.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  85, 85,  0,  40000,   126 }, // 85: GM86; HMIGM86; rickM93; sGM86; Lead 7 fifths; Saw_wave; am086.in

    // Amplitude begins at 2582.4, peaks 3331.5 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  86, 86,  0,  40000,    13 }, // 86: GM87; HMIGM87; mGM87; sGM87; Lead 8 brass; am087.in

    // Amplitude begins at 1504.8, peaks 3734.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  87, 87,  0,  40000,   160 }, // 87: GM88; HMIGM88; sGM88; Pad 1 new age; am088.in

    // Amplitude begins at 1679.1, peaks 3279.1 at 24.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.9s.
    {  88, 88,  0,  40000,  2880 }, // 88: GM89; HMIGM89; sGM89; Pad 2 warm; am089.in

    // Amplitude begins at  641.8, peaks 4073.9 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  89, 89,  0,  40000,   106 }, // 89: GM90; HMIGM90; hamM21; intM21; rickM21; sGM90; LTriang; LTriang.; Pad 3 polysynth; am090.in

    // Amplitude begins at    7.2, peaks 4761.3 at 6.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.5s.
    {  90, 90,  0,  40000,  1526 }, // 90: GM91; HMIGM91; rickM95; sGM91; Pad 4 choir; Spacevo.; am091.in

    // Amplitude begins at    0.0, peaks 3767.6 at 1.2s,
    // fades to 20% at 4.6s, keyoff fades to 20% in 0.0s.
    {  91, 91,  0,   4586,     6 }, // 91: GM92; HMIGM92; sGM92; Pad 5 bowedpad; am092.in

    // Amplitude begins at    0.0, peaks 1692.3 at 0.6s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 2.8s.
    {  92, 92,  0,   2773,  2773 }, // 92: GM93; HMIGM93; hamM22; intM22; rickM22; sGM93; PSlow; PSlow.in; Pad 6 metallic; am093.in

    // Amplitude begins at    0.0, peaks 3007.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  93, 93,  0,  40000,   193 }, // 93: GM94; HMIGM94; hamM23; hamM54; intM23; rickM23; rickM96; sGM94; Halopad.; PSweep; PSweep.i; Pad 7 halo; am094.in; halopad

    // Amplitude begins at 2050.9, peaks 4177.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  94, 94,  0,  40000,    53 }, // 94: GM95; HMIGM95; hamM66; rickM97; Pad 8 sweep; Sweepad.; am095.in; sweepad

    // Amplitude begins at  852.7, peaks 2460.9 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    {  95, 95,  0,    993,   993 }, // 95: GM96; HMIGM96; sGM96; FX 1 rain; am096.in

    // Amplitude begins at    0.0, peaks 4045.6 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    {  96, 96,  0,  40000,   580 }, // 96: GM97; HMIGM97; mGM97; sGM97; FX 2 soundtrack; am097.in

    // Amplitude begins at 1790.2, peaks 3699.2 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    {  97, 97,  0,    540,   540 }, // 97: GM98; HMIGM98; mGM98; sGM98; FX 3 crystal; am098.in

    // Amplitude begins at  992.2, peaks 1029.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  98, 98,  0,  40000,    93 }, // 98: GM99; HMIGM99; sGM99; FX 4 atmosphere; am099.in

    // Amplitude begins at 3083.3, peaks 3480.3 at 0.0s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    {  99, 99,  0,   2233,  2233 }, // 99: GM100; HMIGM100; hamM51; sGM100; FX 5 brightness; am100; am100.in

    // Amplitude begins at    0.0, peaks 1686.6 at 2.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 100,100,  0,  40000,   800 }, // 100: GM101; HMIGM101; sGM101; FX 6 goblins; am101.in

    // Amplitude begins at    0.0, peaks 1834.1 at 4.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 101,101,  0,  40000,   773 }, // 101: GM102; HMIGM102; rickM98; sGM102; Echodrp1; FX 7 echoes; am102.in

    // Amplitude begins at   88.5, peaks 2197.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 102,102,  0,  40000,   453 }, // 102: GM103; HMIGM103; sGM103; FX 8 sci-fi; am103.in

    // Amplitude begins at 2640.7, peaks 2812.3 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 103,103,  0,   2306,  2306 }, // 103: GM104; HMIGM104; mGM104; sGM104; Sitar; am104.in

    // Amplitude begins at 2465.5, peaks 2912.3 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 104,104,  0,   1053,  1053 }, // 104: GM105; HMIGM105; mGM105; sGM105; Banjo; am105.in

    // Amplitude begins at  933.2,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 105,105,  0,   1160,  1160 }, // 105: GM106; HMIGM106; hamM24; intM24; mGM106; rickM24; sGM106; LDist; LDist.in; Shamisen; am106.in

    // Amplitude begins at 2865.6,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 106,106,  0,    653,   653 }, // 106: GM107; HMIGM107; sGM107; Koto; am107.in

    // Amplitude begins at 2080.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 107,107,  0,    153,   153 }, // 107: GM108; HMIGM108; hamM57; mGM108; sGM108; Kalimba; am108.in; kalimba

    // Amplitude begins at    6.6, peaks 2652.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 108,108,  0,  40000,     0 }, // 108: GM109; HMIGM109; mGM109; sGM109; Bagpipe; am109.in

    // Amplitude begins at  533.2, peaks 1795.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 109,109,  0,  40000,   106 }, // 109: GM110; HMIGM110; mGM110; sGM110; Fiddle; am110.in

    // Amplitude begins at   66.0, peaks 1441.9 at 16.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 110,110,  0,  40000,     6 }, // 110: GM111; HMIGM111; mGM111; sGM111; Shanai; am111.in

    // Amplitude begins at 1669.3, peaks 1691.7 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 111,111,  0,   1226,  1226 }, // 111: GM112; HMIGM112; mGM112; sGM112; Tinkle Bell; am112.in

    // Amplitude begins at 1905.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 112,112,  0,    146,   146 }, // 112: GM113; HMIGM113; hamM50; mGM113; sGM113; Agogo Bells; agogo; am113.in

    // Amplitude begins at 1008.1, peaks 3001.4 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 113,113,  0,    226,   226 }, // 113: GM114; HMIGM114; mGM114; sGM114; Steel Drums; am114.in

    // Amplitude begins at  894.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 114,114,  0,     26,    26 }, // 114: GM115; HMIGM115; mGM115; rickM100; sGM115; Woodblk.; Woodblock; am115.in

    // Amplitude begins at 1571.0, peaks 1764.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,115,  0,    146,   146 }, // 115: GM116; HMIGM116; hamM69; mGM116; Taiko; Taiko Drum; am116.in

    // Amplitude begins at 1088.6, peaks 1805.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 116,116,  0,     40,    40 }, // 116: GM117; HMIGM117; hamM58; mGM117; sGM117; Melodic Tom; am117.in; melotom

    // Amplitude begins at  781.7, peaks  845.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 117,117,  0,    386,   386 }, // 117: GM118; HMIGM118; mGM118; Synth Drum; am118.in

    // Amplitude begins at    0.0, peaks  452.7 at 2.2s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 118,118,  0,   2333,  2333 }, // 118: GM119; HMIGM119; mGM119; Reverse Cymbal; am119.in

    // Amplitude begins at    0.0, peaks  363.9 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 119,119,  0,    313,   313 }, // 119: GM120; HMIGM120; hamM36; intM36; mGM120; rickM101; rickM36; sGM120; DNoise1; DNoise1.; Fretnos.; Guitar FretNoise; am120.in

    // Amplitude begins at    0.0, peaks  472.1 at 0.3s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 120,120,  0,    586,   586 }, // 120: GM121; HMIGM121; mGM121; sGM121; Breath Noise; am121.in

    // Amplitude begins at    0.0, peaks  449.3 at 2.3s,
    // fades to 20% at 4.4s, keyoff fades to 20% in 4.4s.
    { 121,121,  0,   4380,  4380 }, // 121: GM122; HMIGM122; mGM122; sGM122; Seashore; am122.in

    // Amplitude begins at    0.6, peaks 2634.5 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 122,122,  0,    320,   320 }, // 122: GM123; HMIGM123; mGM123; sGM123; Bird Tweet; am123.in

    // Amplitude begins at 1196.7,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 123,123,  0,    186,   186 }, // 123: GM124; HMIGM124; mGM124; sGM124; Telephone; am124.in

    // Amplitude begins at    0.0, peaks  389.6 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 124,124,  0,    146,   146 }, // 124: GM125; HMIGM125; mGM125; sGM125; Helicopter; am125.in

    // Amplitude begins at    0.0, peaks  459.9 at 2.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 125,125,  0,  40000,   113 }, // 125: GM126; HMIGM126; mGM126; sGM126; Applause/Noise; am126.in

    // Amplitude begins at  361.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 126,126,  0,    160,   160 }, // 126: GM127; HMIGM127; mGM127; sGM127; Gunshot; am127.in

    // Amplitude begins at 1345.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 35,     53,    53 }, // 127: GP35; GP36; hamP11; rickP14; Ac Bass Drum; Bass Drum 1; aps035; kick2.in

    // Amplitude begins at 2509.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 128,128, 52,     20,    20 }, // 128: GP37; Side Stick

    // Amplitude begins at  615.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,129, 48,     80,    80 }, // 129: GP38; Acoustic Snare

    // Amplitude begins at 2088.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 130,130, 58,     40,    40 }, // 130: GP39; Hand Clap

    // Amplitude begins at  606.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,129, 60,     80,    80 }, // 131: GP40; Electric Snare

    // Amplitude begins at 1552.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 47,    106,   106 }, // 132: GP41; hamP1; Low Floor Tom; aps041

    // Amplitude begins at  256.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 43,     26,    26 }, // 133: GP42; Closed High Hat

    // Amplitude begins at 1387.8,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 49,    153,   153 }, // 134: GP43; hamP2; High Floor Tom; aps041

    // Amplitude begins at   43.8, peaks  493.1 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 133,133, 43,     26,    26 }, // 135: GP44; Pedal High Hat

    // Amplitude begins at 1571.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 51,    153,   153 }, // 136: GP45; hamP3; Low Tom; aps041

    // Amplitude begins at  319.5,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 134,134, 43,    253,   253 }, // 137: GP46; Open High Hat

    // Amplitude begins at 1793.9, peaks 1833.4 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 54,    113,   113 }, // 138: GP47; hamP4; Low-Mid Tom; aps041

    // Amplitude begins at 1590.0, peaks 1898.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 57,    140,   140 }, // 139: GP48; hamP5; High-Mid Tom; aps041

    // Amplitude begins at  380.1, peaks  387.8 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 135,135, 72,    573,   573 }, // 140: GP49; Crash Cymbal 1

    // Amplitude begins at 1690.3, peaks 1753.1 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 131,131, 60,    160,   160 }, // 141: GP50; hamP6; High Tom; aps041

    // Amplitude begins at  473.8,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 136,136, 76,    506,   506 }, // 142: GP51; Ride Cymbal 1

    // Amplitude begins at  648.1, peaks  699.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 137,137, 84,    406,   406 }, // 143: GP52; hamP19; Chinese Cymbal; aps052

    // Amplitude begins at  931.9,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 138,138, 36,    386,   386 }, // 144: GP53; Ride Bell

    // Amplitude begins at 1074.7, peaks 1460.5 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 139,139, 65,    553,   553 }, // 145: GP54; Tambourine

    // Amplitude begins at  367.4, peaks  378.6 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 140,140, 84,    420,   420 }, // 146: GP55; Splash Cymbal

    // Amplitude begins at 1444.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 141,141, 83,     53,    53 }, // 147: GP56; Cow Bell

    // Amplitude begins at  388.5,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 135,135, 84,    400,   400 }, // 148: GP57; Crash Cymbal 2

    // Amplitude begins at  691.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 142,142, 24,     13,    13 }, // 149: GP58; Vibraslap

    // Amplitude begins at  468.1,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 136,136, 77,    513,   513 }, // 150: GP59; Ride Cymbal 2

    // Amplitude begins at 1809.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 143,143, 60,     40,    40 }, // 151: GP60; High Bongo

    // Amplitude begins at 1360.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 144,144, 65,     40,    40 }, // 152: GP61; Low Bongo

    // Amplitude begins at 2805.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 145,145, 59,     13,    13 }, // 153: GP62; Mute High Conga

    // Amplitude begins at 1551.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 146,146, 51,     40,    40 }, // 154: GP63; Open High Conga

    // Amplitude begins at 2245.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 147,147, 45,     33,    33 }, // 155: GP64; Low Conga

    // Amplitude begins at  839.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 148,148, 71,    133,   133 }, // 156: GP65; hamP8; rickP98; High Timbale; timbale; timbale.

    // Amplitude begins at  888.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 149,149, 60,    140,   140 }, // 157: GP66; Low Timbale

    // Amplitude begins at  780.2,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 150,150, 58,    153,   153 }, // 158: GP67; High Agogo

    // Amplitude begins at 1327.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 151,151, 53,    100,   100 }, // 159: GP68; Low Agogo

    // Amplitude begins at    0.9, peaks  358.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 152,152, 64,     86,    86 }, // 160: GP69; Cabasa

    // Amplitude begins at  304.0, peaks  315.3 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 153,153, 71,     13,    13 }, // 161: GP70; Maracas

    // Amplitude begins at   37.5, peaks  789.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 154,154, 61,    326,   326 }, // 162: GP71; Short Whistle

    // Amplitude begins at   34.3, peaks  858.7 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 155,155, 61,    606,   606 }, // 163: GP72; Long Whistle

    // Amplitude begins at    1.8, peaks  475.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 156,156, 44,     80,    80 }, // 164: GP73; rickP96; Short Guiro; guiros.i

    // Amplitude begins at    0.0, peaks  502.1 at 0.2s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 157,157, 40,    300,   300 }, // 165: GP74; Long Guiro

    // Amplitude begins at 2076.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 158,158, 69,     20,    20 }, // 166: GP75; Claves

    // Amplitude begins at 2255.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 159,159, 68,     20,    20 }, // 167: GP76; High Wood Block

    // Amplitude begins at 2214.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 160,160, 63,     33,    33 }, // 168: GP77; Low Wood Block

    // Amplitude begins at   15.6, peaks 2724.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 161,161, 74,     93,    93 }, // 169: GP78; Mute Cuica

    // Amplitude begins at 1240.9, peaks 2880.7 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 162,162, 60,    346,   346 }, // 170: GP79; Open Cuica

    // Amplitude begins at 1090.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 163,163, 80,     60,    60 }, // 171: GP80; Mute Triangle

    // Amplitude begins at 1193.8,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 164,164, 64,    880,   880 }, // 172: GP81; Open Triangle

    // Amplitude begins at    3.9, peaks  349.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 165,165, 72,     40,    40 }, // 173: GP82; hamP7; Shaker; aps082

    // Amplitude begins at    0.0, peaks  937.8 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 166,166, 73,    293,   293 }, // 174: GP83; Jingle Bell

    // Amplitude begins at  904.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 167,167, 70,    180,   180 }, // 175: GP84; Bell Tree

    // Amplitude begins at 1135.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 168,168, 68,     20,    20 }, // 176: GP85; Castanets

    // Amplitude begins at 2185.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 169,169, 48,     26,    26 }, // 177: GP86; Mute Surdu

    // Amplitude begins at 1783.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 131,131, 53,    120,   120 }, // 178: GP87; rickP105; Open Surdu; surdu.in

    // Amplitude begins at   56.3, peaks 1484.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 170,170,  0,  40000,    13 }, // 179: HMIGM0; HMIGM29; mGM29; Overdrive Guitar; am029.in

    // Amplitude begins at 3912.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 0.0s.
    { 171,171,  0,   1326,     6 }, // 180: HMIGM1; mGM1; BrightAcouGrand; am001.in

    // Amplitude begins at 2850.7, peaks 4216.6 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 172,172,  0,   1293,  1293 }, // 181: HMIGM2; mGM2; ElecGrandPiano; am002.in

    // Amplitude begins at 1712.7, peaks 2047.5 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 173,173,  0,  40000,     6 }, // 182: HMIGM3; am003.in

    // Amplitude begins at 4461.0, peaks 6341.0 at 0.0s,
    // fades to 20% at 3.2s, keyoff fades to 20% in 0.0s.
    { 174,174,  0,   3200,     6 }, // 183: HMIGM4; mGM4; Rhodes Piano; am004.in

    // Amplitude begins at 4781.0, peaks 6329.2 at 0.0s,
    // fades to 20% at 3.2s, keyoff fades to 20% in 0.0s.
    { 175,175,  0,   3200,     6 }, // 184: HMIGM5; mGM5; Chorused Piano; am005.in

    // Amplitude begins at 1162.2, peaks 1404.5 at 0.0s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    { 176,176,  0,   2173,  2173 }, // 185: HMIGM6; mGM6; Harpsichord; am006.in

    // Amplitude begins at 1144.6, peaks 1235.5 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 177,177,  0,   2313,  2313 }, // 186: HMIGM7; mGM7; Clavinet; am007.in

    // Amplitude begins at 2803.9, peaks 2829.0 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 178,178,  0,    960,   960 }, // 187: HMIGM8; am008.in

    // Amplitude begins at 2999.3, peaks 3227.0 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 179,179,  0,   1380,  1380 }, // 188: HMIGM9; mGM9; Glockenspiel; am009.in

    // Amplitude begins at 2073.6, peaks 3450.4 at 0.1s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 180,180,  0,    453,   453 }, // 189: HMIGM10; mGM10; Music box; am010.in

    // Amplitude begins at 2976.7, peaks 3033.0 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 181,181,  0,   1746,  1746 }, // 190: HMIGM11; am011.in

    // Amplitude begins at 3343.0, peaks 3632.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 182,182,  0,    206,   206 }, // 191: HMIGM12; mGM12; Marimba; am012.in

    // Amplitude begins at 2959.7, peaks 3202.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 183,183,  0,    100,   100 }, // 192: HMIGM13; mGM13; Xylophone; am013.in

    // Amplitude begins at 2057.2, peaks 2301.4 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 184,184,  0,   1306,  1306 }, // 193: HMIGM14; am014.in

    // Amplitude begins at 1673.4, peaks 2155.0 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 185,185,  0,    460,   460 }, // 194: HMIGM15; am015.in

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 186,186,  0,   1266,  1266 }, // 195: HMIGM27; mGM27; Electric Guitar2; am027.in

    // Amplitude begins at 1957.2, peaks 3738.3 at 0.0s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 187,187,  0,   2886,  2886 }, // 196: HMIGM37; mGM37; Slap Bass 2; am037.in

    // Amplitude begins at    7.2, peaks 3168.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 188,188,  0,  40000,    20 }, // 197: HMIGM62; am062.in

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 189,189, 60,      0,     0 }, // 198: HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99; Blank.in

    // Amplitude begins at    4.1, peaks  788.6 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 73,    473,   473 }, // 199: HMIGP27; Wierd1.i

    // Amplitude begins at    4.2, peaks  817.8 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 74,    473,   473 }, // 200: HMIGP28; Wierd1.i

    // Amplitude begins at    4.9, peaks  841.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 80,    473,   473 }, // 201: HMIGP29; Wierd1.i

    // Amplitude begins at    5.3, peaks  778.3 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 190,190, 84,    473,   473 }, // 202: HMIGP30; Wierd1.i

    // Amplitude begins at    9.5, peaks  753.0 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 190,190, 92,    406,   406 }, // 203: HMIGP31; Wierd1.i

    // Amplitude begins at   91.3, peaks  917.0 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 191,191, 81,    280,   280 }, // 204: HMIGP32; Wierd2.i

    // Amplitude begins at   89.5, peaks  919.6 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 191,191, 83,    280,   280 }, // 205: HMIGP33; Wierd2.i

    // Amplitude begins at  155.5, peaks  913.4 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 191,191, 95,    240,   240 }, // 206: HMIGP34; Wierd2.i

    // Amplitude begins at  425.5, peaks  768.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 192,192, 83,    273,   273 }, // 207: HMIGP35; Wierd3.i

    // Amplitude begins at 2304.1, peaks 2323.3 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 193,193, 35,     33,    33 }, // 208: HMIGP36; Kick.ins

    // Amplitude begins at 2056.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 194,194, 36,     20,    20 }, // 209: HMIGP37; HMIGP86; RimShot.; rimshot.

    // Amplitude begins at  752.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 195,195, 60,     40,    40 }, // 210: HMIGP38; Snare.in

    // Amplitude begins at  585.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 196,196, 59,     20,    20 }, // 211: HMIGP39; Clap.ins

    // Amplitude begins at  768.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 195,195, 44,     40,    40 }, // 212: HMIGP40; Snare.in

    // Amplitude begins at  646.2, peaks  724.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 41,    160,   160 }, // 213: HMIGP41; Toms.ins

    // Amplitude begins at 2548.3, peaks 2665.5 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 198,198, 97,    200,   200 }, // 214: HMIGP42; HMIGP44; clshat97

    // Amplitude begins at  700.8, peaks  776.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 197,197, 44,    120,   120 }, // 215: HMIGP43; Toms.ins

    // Amplitude begins at  667.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 48,    160,   160 }, // 216: HMIGP45; Toms.ins

    // Amplitude begins at   11.0, peaks  766.2 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 199,199, 96,    253,   253 }, // 217: HMIGP46; Opnhat96

    // Amplitude begins at  646.7, peaks  707.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 51,    166,   166 }, // 218: HMIGP47; Toms.ins

    // Amplitude begins at  753.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 197,197, 54,    153,   153 }, // 219: HMIGP48; Toms.ins

    // Amplitude begins at  473.8,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 200,200, 40,    380,   380 }, // 220: HMIGP49; HMIGP52; HMIGP55; HMIGP57; Crashcym

    // Amplitude begins at  775.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 197,197, 57,    106,   106 }, // 221: HMIGP50; Toms.ins

    // Amplitude begins at  360.8,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 201,201, 58,    206,   206 }, // 222: HMIGP51; HMIGP53; Ridecym.

    // Amplitude begins at 2347.0, peaks 2714.6 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 202,202, 97,    186,   186 }, // 223: HMIGP54; Tamb.ins

    // Amplitude begins at 2920.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 203,203, 50,     73,    73 }, // 224: HMIGP56; Cowbell.

    // Amplitude begins at  556.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 204,204, 28,     26,    26 }, // 225: HMIGP58; vibrasla

    // Amplitude begins at  365.3,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 201,201, 60,    186,   186 }, // 226: HMIGP59; ridecym.

    // Amplitude begins at 2172.1, peaks 2678.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 205,205, 53,     46,    46 }, // 227: HMIGP60; mutecong

    // Amplitude begins at 2346.3, peaks 2455.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 206,206, 46,     53,    53 }, // 228: HMIGP61; conga.in

    // Amplitude begins at 2247.7, peaks 2709.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 205,205, 57,     46,    46 }, // 229: HMIGP62; mutecong

    // Amplitude begins at 1529.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 207,207, 42,    133,   133 }, // 230: HMIGP63; loconga.

    // Amplitude begins at 1495.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 207,207, 37,    133,   133 }, // 231: HMIGP64; loconga.

    // Amplitude begins at  486.6, peaks  530.9 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 208,208, 41,    186,   186 }, // 232: HMIGP65; timbale.

    // Amplitude begins at  508.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 208,208, 37,    193,   193 }, // 233: HMIGP66; timbale.

    // Amplitude begins at  789.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 209,209, 77,     40,    40 }, // 234: HMIGP67; agogo.in

    // Amplitude begins at  735.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 209,209, 72,     46,    46 }, // 235: HMIGP68; agogo.in

    // Amplitude begins at    5.1, peaks  818.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 210,210, 70,     66,    66 }, // 236: HMIGP69; HMIGP82; shaker.i

    // Amplitude begins at    4.4, peaks  758.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 210,210, 90,     66,    66 }, // 237: HMIGP70; shaker.i

    // Amplitude begins at  474.0, peaks 1257.8 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 211,211, 39,    180,   180 }, // 238: HMIGP71; hiwhist.

    // Amplitude begins at  468.8, peaks 1217.2 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 212,212, 36,    513,   513 }, // 239: HMIGP72; lowhist.

    // Amplitude begins at   28.5, peaks  520.5 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 213,213, 46,     46,    46 }, // 240: HMIGP73; higuiro.

    // Amplitude begins at 1096.9, peaks 2623.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 214,214, 48,    253,   253 }, // 241: HMIGP74; loguiro.

    // Amplitude begins at 2832.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 215,215, 85,     13,    13 }, // 242: HMIGP75; clavecb.

    // Amplitude begins at 2608.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 216,216, 66,     60,    60 }, // 243: HMIGP76; woodblok

    // Amplitude begins at 2613.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 216,216, 61,     60,    60 }, // 244: HMIGP77; woodblok

    // Amplitude begins at    2.6, peaks 2733.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 217,217, 41,    120,   120 }, // 245: HMIGP78; hicuica.

    // Amplitude begins at    2.4, peaks 2900.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 218,218, 41,  40000,    20 }, // 246: HMIGP79; locuica.

    // Amplitude begins at 1572.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 219,219, 81,     66,    66 }, // 247: HMIGP80; mutringl

    // Amplitude begins at 1668.0,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 220,220, 81,    260,   260 }, // 248: HMIGP81; triangle

    // Amplitude begins at 1693.0,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 220,220, 76,    260,   260 }, // 249: HMIGP83; triangle

    // Amplitude begins at 1677.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 220,220,103,    220,   220 }, // 250: HMIGP84; triangle

    // Amplitude begins at 1635.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 194,194, 60,     13,    13 }, // 251: HMIGP85; rimShot.

    // Amplitude begins at  976.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 221,221, 53,    120,   120 }, // 252: HMIGP87; taiko.in

    // Amplitude begins at   64.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 222,222,  0,      6,     6 }, // 253: hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; intP0; intP1; intP10; intP100; intP101; intP102; intP103; intP104; intP105; intP106; intP107; intP108; intP109; intP11; intP110; intP111; intP112; intP113; intP114; intP115; intP116; intP117; intP118; intP119; intP12; intP120; intP121; intP122; intP123; intP124; intP125; intP126; intP127; intP13; intP14; intP15; intP16; intP17; intP18; intP19; intP2; intP20; intP21; intP22; intP23; intP24; intP25; intP26; intP3; intP4; intP5; intP6; intP7; intP8; intP9; intP94; intP95; intP96; intP97; intP98; intP99; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9; nosound; nosound.

    // Amplitude begins at  947.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 223,223,  0,     40,    40 }, // 254: hamM1; intM1; rickM1; DBlock; DBlock.i

    // Amplitude begins at 2090.6,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 224,224,  0,   1213,  1213 }, // 255: hamM2; intM2; rickM2; GClean; GClean.i

    // Amplitude begins at 1555.8, peaks 1691.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 225,225,  0,    126,   126 }, // 256: hamM4; intM4; rickM4; DToms; DToms.in

    // Amplitude begins at  733.0, peaks  761.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 226,226,  0,  40000,    20 }, // 257: hamM7; intM7; rickM7; rickM84; GOverD; GOverD.i; Guit_fz2

    // Amplitude begins at 1258.5, peaks 1452.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 227,227,  0,  40000,     6 }, // 258: hamM8; intM8; rickM8; GMetal; GMetal.i

    // Amplitude begins at 2589.3, peaks 2634.3 at 0.0s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 228,228,  0,    706,   706 }, // 259: hamM9; intM9; rickM9; BPick; BPick.in

    // Amplitude begins at 1402.0, peaks 2135.8 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 229,229,  0,    446,   446 }, // 260: hamM10; intM10; rickM10; BSlap; BSlap.in

    // Amplitude begins at 3040.5, peaks 3072.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 230,230,  0,    440,   440 }, // 261: hamM11; intM11; rickM11; BSynth1; BSynth1.

    // Amplitude begins at 2530.9, peaks 2840.4 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 231,231,  0,    626,   626 }, // 262: hamM12; intM12; rickM12; BSynth2; BSynth2.

    // Amplitude begins at   39.5, peaks 1089.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 232,232,  0,  40000,    26 }, // 263: hamM15; intM15; rickM15; PSoft; PSoft.in

    // Amplitude begins at    0.0, peaks 1440.0 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 233,233,  0,  40000,    26 }, // 264: hamM18; intM18; rickM18; PRonStr1

    // Amplitude begins at    0.0, peaks 1417.7 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 234,234,  0,  40000,    26 }, // 265: hamM19; intM19; rickM19; PRonStr2

    // Amplitude begins at   73.3, peaks 1758.3 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 235,235,  0,   1940,  1940 }, // 266: hamM25; intM25; rickM25; LTrap; LTrap.in

    // Amplitude begins at 2192.9, peaks 3442.1 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 236,236,  0,   1886,  1886 }, // 267: hamM26; intM26; rickM26; LSaw; LSaw.ins

    // Amplitude begins at 1119.0, peaks 1254.3 at 0.1s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 237,237,  0,   1733,  1733 }, // 268: hamM27; intM27; rickM27; PolySyn; PolySyn.

    // Amplitude begins at 2355.1, peaks 3374.2 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 238,238,  0,  40000,     0 }, // 269: hamM28; intM28; rickM28; Pobo; Pobo.ins

    // Amplitude begins at  679.7, peaks 2757.2 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 239,239,  0,  40000,    20 }, // 270: hamM29; intM29; rickM29; PSweep2; PSweep2.

    // Amplitude begins at 1971.4, peaks 2094.8 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 240,240,  0,   1386,  1386 }, // 271: hamM30; intM30; rickM30; LBright; LBright.

    // Amplitude begins at    0.0, peaks  857.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 241,241,  0,  40000,    73 }, // 272: hamM31; intM31; rickM31; SynStrin

    // Amplitude begins at  901.8, peaks  974.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 242,242,  0,  40000,   106 }, // 273: hamM32; intM32; rickM32; SynStr2; SynStr2.

    // Amplitude begins at 2098.8,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 243,243,  0,    560,   560 }, // 274: hamM33; intM33; rickM33; low_blub

    // Amplitude begins at  978.8, peaks 2443.9 at 1.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.4s.
    { 244,244,  0,  40000,  2446 }, // 275: hamM34; intM34; rickM34; DInsect; DInsect.

    // Amplitude begins at  426.5, peaks  829.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 245,245,  0,    140,   140 }, // 276: hamM35; intM35; rickM35; hardshak

    // Amplitude begins at  982.9, peaks 2185.9 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 246,246,  0,    260,   260 }, // 277: hamM37; intM37; rickM37; WUMP; WUMP.ins

    // Amplitude begins at  557.9,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 247,247,  0,    160,   160 }, // 278: hamM38; intM38; rickM38; DSnare; DSnare.i

    // Amplitude begins at 2110.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 248,248,  0,     20,    20 }, // 279: hamM39; intM39; rickM39; DTimp; DTimp.in

    // Amplitude begins at    0.0, peaks  830.0 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 249,249,  0,  40000,     0 }, // 280: hamM40; intM40; rickM40; DRevCym; DRevCym.

    // Amplitude begins at    0.0, peaks 1668.5 at 0.2s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 250,250,  0,    780,   780 }, // 281: hamM41; intM41; rickM41; Dorky; Dorky.in

    // Amplitude begins at    4.1, peaks 2699.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 251,251,  0,     73,    73 }, // 282: hamM42; intM42; rickM42; DFlab; DFlab.in

    // Amplitude begins at 1995.3, peaks 3400.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 252,252,  0,  40000,    60 }, // 283: hamM43; intM43; rickM43; DInsect2

    // Amplitude begins at    0.0, peaks  792.4 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 253,253,  0,  40000,   406 }, // 284: hamM44; intM44; rickM44; DChopper

    // Amplitude begins at  787.3, peaks  848.0 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 254,254,  0,    306,   306 }, // 285: hamM45; intM45; rickM45; DShot; DShot.in

    // Amplitude begins at 2940.9, peaks 3003.1 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 255,255,  0,   1780,  1780 }, // 286: hamM46; intM46; rickM46; KickAss; KickAss.

    // Amplitude begins at 1679.1, peaks 1782.4 at 0.1s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 256,256,  0,   1693,  1693 }, // 287: hamM47; intM47; rickM47; RVisCool

    // Amplitude begins at 1214.6, peaks 1237.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 257,257,  0,     80,    80 }, // 288: hamM48; intM48; rickM48; DSpring; DSpring.

    // Amplitude begins at    0.0, peaks 3049.5 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 258,258,  0,  40000,    20 }, // 289: intM49; Chorar22

    // Amplitude begins at 1401.3, peaks 2203.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 259,259, 36,     73,    73 }, // 290: hamP27; intP27; rickP27; timpani; timpani.

    // Amplitude begins at 1410.5, peaks 1600.6 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 260,260, 50,    580,   580 }, // 291: hamP28; intP28; rickP28; timpanib

    // Amplitude begins at 1845.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 261,261, 37,     33,    33 }, // 292: hamP29; intP29; rickP29; APS043; APS043.i

    // Amplitude begins at 1267.8, peaks 1286.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 262,262, 39,    166,   166 }, // 293: hamP30; intP30; rickP30; mgun3; mgun3.in

    // Amplitude begins at 1247.2, peaks 1331.5 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 263,263, 39,     53,    53 }, // 294: hamP31; intP31; rickP31; kick4r; kick4r.i

    // Amplitude begins at 1432.2, peaks 1442.6 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 264,264, 86,   1826,  1826 }, // 295: hamP32; intP32; rickP32; timb1r; timb1r.i

    // Amplitude begins at  996.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 265,265, 43,     33,    33 }, // 296: hamP33; intP33; rickP33; timb2r; timb2r.i

    // Amplitude begins at 1331.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 127,127, 24,      6,     6 }, // 297: hamP34; intP34; rickP34; apo035; apo035.i

    // Amplitude begins at 1324.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 127,127, 29,     53,    53 }, // 298: hamP35; intP35; rickP35; apo035; apo035.i

    // Amplitude begins at 1604.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 266,266, 50,    193,   193 }, // 299: hamP36; intP36; rickP36; hartbeat

    // Amplitude begins at 1443.1, peaks 1537.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 267,267, 30,    100,   100 }, // 300: hamP37; intP37; rickP37; tom1r; tom1r.in

    // Amplitude begins at 1351.4, peaks 1552.4 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 267,267, 33,    320,   320 }, // 301: hamP38; intP38; rickP38; tom1r; tom1r.in

    // Amplitude begins at 1362.2, peaks 1455.5 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 267,267, 38,   1646,  1646 }, // 302: hamP39; intP39; rickP39; tom1r; tom1r.in

    // Amplitude begins at 1292.1, peaks 1445.6 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 267,267, 42,   1706,  1706 }, // 303: hamP40; intP40; rickP40; tom1r; tom1r.in

    // Amplitude begins at 2139.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 268,268, 24,     46,    46 }, // 304: intP41; rickP41; tom2; tom2.ins

    // Amplitude begins at 2100.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 27,     53,    53 }, // 305: intP42; rickP42; tom2; tom2.ins

    // Amplitude begins at 2145.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 29,     53,    53 }, // 306: intP43; rickP43; tom2; tom2.ins

    // Amplitude begins at 2252.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 268,268, 32,     53,    53 }, // 307: intP44; rickP44; tom2; tom2.ins

    // Amplitude begins at 1018.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 269,269, 32,     13,    13 }, // 308: hamP45; intP45; rickP45; tom; tom.ins

    // Amplitude begins at  837.0,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 270,270, 53,     80,    80 }, // 309: hamP46; intP46; rickP46; conga; conga.in

    // Amplitude begins at  763.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 270,270, 57,     80,    80 }, // 310: hamP47; intP47; rickP47; conga; conga.in

    // Amplitude begins at  640.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 271,271, 60,     80,    80 }, // 311: hamP48; intP48; rickP48; snare01r

    // Amplitude begins at  969.1,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 272,272, 55,    153,   153 }, // 312: hamP49; intP49; rickP49; slap; slap.ins

    // Amplitude begins at  801.4, peaks  829.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 254,254, 85,    260,   260 }, // 313: hamP50; intP50; rickP50; shot; shot.ins

    // Amplitude begins at  882.9, peaks  890.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 273,273, 90,    473,   473 }, // 314: hamP51; intP51; rickP51; snrsust; snrsust.

    // Amplitude begins at  766.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 274,274, 84,     73,    73 }, // 315: intP52; rickP52; snare; snare.in

    // Amplitude begins at  791.4,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 275,275, 48,    166,   166 }, // 316: hamP53; intP53; rickP53; synsnar; synsnar.

    // Amplitude begins at  715.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 276,276, 48,     40,    40 }, // 317: hamP54; intP54; rickP54; synsnr1; synsnr1.

    // Amplitude begins at  198.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 72,     13,    13 }, // 318: hamP55; intP55; rickP55; aps042; aps042.i

    // Amplitude begins at 1290.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 277,277, 72,     13,    13 }, // 319: hamP56; intP56; rickP56; rimshotb

    // Amplitude begins at  931.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 278,278, 72,     13,    13 }, // 320: hamP57; intP57; rickP57; rimshot; rimshot.

    // Amplitude begins at  413.6,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 279,279, 63,    580,   580 }, // 321: hamP58; intP58; rickP58; crash; crash.in

    // Amplitude begins at  407.6,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 279,279, 65,    586,   586 }, // 322: hamP59; intP59; rickP59; crash; crash.in

    // Amplitude begins at  377.2, peaks  377.3 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 280,280, 79,    506,   506 }, // 323: intP60; rickP60; cymbal; cymbal.i

    // Amplitude begins at  102.7, peaks  423.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 281,281, 38,    113,   113 }, // 324: hamP61; intP61; rickP61; cymbals; cymbals.

    // Amplitude begins at  507.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 282,282, 94,    100,   100 }, // 325: hamP62; intP62; rickP62; hammer5r

    // Amplitude begins at  640.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 283,283, 87,    120,   120 }, // 326: hamP63; intP63; rickP63; hammer3; hammer3.

    // Amplitude begins at  611.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 283,283, 94,    106,   106 }, // 327: hamP64; intP64; rickP64; hammer3; hammer3.

    // Amplitude begins at  861.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 284,284, 80,     60,    60 }, // 328: hamP65; intP65; rickP65; ride2; ride2.in

    // Amplitude begins at  753.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 285,285, 47,    140,   140 }, // 329: hamP66; intP66; rickP66; hammer1; hammer1.

    // Amplitude begins at  747.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 286,286, 61,     80,    80 }, // 330: intP67; rickP67; tambour; tambour.

    // Amplitude begins at  691.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 286,286, 68,     73,    73 }, // 331: intP68; rickP68; tambour; tambour.

    // Amplitude begins at  771.0,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 287,287, 61,    160,   160 }, // 332: hamP69; intP69; rickP69; tambou2; tambou2.

    // Amplitude begins at  716.0, peaks  730.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 287,287, 68,    133,   133 }, // 333: hamP70; intP70; rickP70; tambou2; tambou2.

    // Amplitude begins at 2362.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 268,268, 60,     40,    40 }, // 334: hamP71; intP71; rickP71; woodbloc

    // Amplitude begins at  912.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 288,288, 60,     60,    60 }, // 335: hamP72; intP72; rickP72; woodblok

    // Amplitude begins at 1238.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 289,289, 36,     53,    53 }, // 336: intP73; rickP73; claves; claves.i

    // Amplitude begins at 1289.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 289,289, 60,     40,    40 }, // 337: intP74; rickP74; claves; claves.i

    // Amplitude begins at  885.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 290,290, 60,     40,    40 }, // 338: hamP75; intP75; rickP75; claves2; claves2.

    // Amplitude begins at  931.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 291,291, 60,     40,    40 }, // 339: hamP76; intP76; rickP76; claves3; claves3.

    // Amplitude begins at 2083.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 292,292, 68,     20,    20 }, // 340: hamP77; intP77; rickP77; clave; clave.in

    // Amplitude begins at 2344.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 293,293, 71,     33,    33 }, // 341: hamP78; intP78; rickP78; agogob4; agogob4.

    // Amplitude begins at 2487.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 293,293, 72,     33,    33 }, // 342: hamP79; intP79; rickP79; agogob4; agogob4.

    // Amplitude begins at 1824.6, peaks 1952.1 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 294,294,101,   1153,  1153 }, // 343: hamP80; intP80; rickP80; clarion; clarion.

    // Amplitude begins at 2039.7, peaks 2300.6 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 295,295, 36,   1466,  1466 }, // 344: hamP81; intP81; rickP81; trainbel

    // Amplitude begins at 1466.4, peaks 1476.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.3s.
    { 296,296, 25,  40000,  2280 }, // 345: hamP82; intP82; rickP82; gong; gong.ins

    // Amplitude begins at  495.7, peaks  730.8 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 297,297, 37,    420,   420 }, // 346: hamP83; intP83; rickP83; kalimbai

    // Amplitude begins at 2307.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 36,     40,    40 }, // 347: hamP84; intP84; rickP84; xylo1; xylo1.in

    // Amplitude begins at 2435.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298, 41,     40,    40 }, // 348: hamP85; intP85; rickP85; xylo1; xylo1.in

    // Amplitude begins at    0.0, peaks  445.7 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 299,299, 84,     80,    80 }, // 349: hamP86; intP86; rickP86; match; match.in

    // Amplitude begins at    0.0, peaks  840.6 at 0.2s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 300,300, 54,   1173,  1173 }, // 350: hamP87; intP87; rickP87; breathi; breathi.

    // Amplitude begins at    0.0, peaks  847.3 at 0.2s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 301,301, 36,    186,   186 }, // 351: intP88; rickP88; scratch; scratch.

    // Amplitude begins at    0.0, peaks  870.4 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 302,302, 60,  40000,     0 }, // 352: hamP89; intP89; rickP89; crowd; crowd.in

    // Amplitude begins at  739.8, peaks 1007.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 303,303, 37,     60,    60 }, // 353: intP90; rickP90; taiko; taiko.in

    // Amplitude begins at  844.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 304,304, 36,     46,    46 }, // 354: hamP91; intP91; rickP91; rlog; rlog.ins

    // Amplitude begins at 1100.9, peaks 1403.9 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 305,305, 32,     40,    40 }, // 355: hamP92; intP92; rickP92; knock; knock.in

    // Amplitude begins at    0.0, peaks  833.9 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 249,249, 48,  40000,     0 }, // 356: intP93; drevcym

    // Amplitude begins at 2453.1, peaks 2577.6 at 0.0s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 306,306,  0,   2440,  2440 }, // 357: hamM52; rickM94; Fantasy1; fantasy1

    // Amplitude begins at 1016.1, peaks 1517.4 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 307,307,  0,    573,   573 }, // 358: hamM53; guitar1

    // Amplitude begins at 1040.8, peaks 1086.3 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.0s.
    { 308,308,  0,    240,     6 }, // 359: hamM55; hamatmos

    // Amplitude begins at    0.4, peaks 3008.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 309,309,  0,  40000,    26 }, // 360: hamM56; hamcalio

    // Amplitude begins at 1031.7, peaks 1690.4 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 310,310,  0,  40000,     6 }, // 361: MGM37; hamM59; oGM37; Slap Bass 2; moon

    // Amplitude begins at    4.0, peaks 1614.7 at 0.0s,
    // fades to 20% at 4.3s, keyoff fades to 20% in 0.0s.
    { 311,311,  0,   4260,    13 }, // 362: hamM62; Polyham3

    // Amplitude begins at    7.3, peaks 1683.7 at 0.1s,
    // fades to 20% at 4.1s, keyoff fades to 20% in 4.1s.
    { 312,312,  0,   4133,  4133 }, // 363: hamM63; Polyham

    // Amplitude begins at 1500.1, peaks 1587.3 at 0.0s,
    // fades to 20% at 4.8s, keyoff fades to 20% in 4.8s.
    { 313,313,  0,   4826,  4826 }, // 364: hamM64; sitar2

    // Amplitude begins at 1572.2, peaks 1680.2 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 314,314,  0,   1753,  1753 }, // 365: hamM70; weird1a

    // Amplitude begins at 1374.4, peaks 1624.9 at 0.0s,
    // fades to 20% at 4.2s, keyoff fades to 20% in 0.0s.
    { 315,315,  0,   4180,    13 }, // 366: hamM71; Polyham4

    // Amplitude begins at 3264.2, peaks 3369.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 316,316,  0,  40000,    13 }, // 367: hamM72; hamsynbs

    // Amplitude begins at 1730.5, peaks 2255.7 at 20.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 317,317,  0,  40000,    66 }, // 368: hamM73; Ocasynth

    // Amplitude begins at 1728.4, peaks 3093.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 318,318,  0,    126,   126 }, // 369: hamM120; hambass1

    // Amplitude begins at  647.4, peaks  662.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 319,319,  0,  40000,    13 }, // 370: hamM121; hamguit1

    // Amplitude begins at 2441.6, peaks 2524.9 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 320,320,  0,  40000,   560 }, // 371: hamM122; hamharm2

    // Amplitude begins at    0.3, peaks 4366.8 at 20.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.7s.
    { 321,321,  0,  40000,   666 }, // 372: hamM123; hamvox1

    // Amplitude begins at    0.0, peaks 2319.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 322,322,  0,  40000,   500 }, // 373: hamM124; hamgob1

    // Amplitude begins at    0.8, peaks 2668.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 323,323,  0,  40000,    40 }, // 374: hamM125; hamblow1

    // Amplitude begins at  338.4, peaks  390.0 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 135,135, 49,   1153,  1153 }, // 375: hamP0; crash1

    // Amplitude begins at  825.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 324,324, 72,     53,    53 }, // 376: hamP9; cowbell

    // Amplitude begins at 1824.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 325,325, 74,     33,    33 }, // 377: hamP10; rickP100; conghi; conghi.i

    // Amplitude begins at 1672.4, peaks 2077.7 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 326,326, 35,     40,    40 }, // 378: hamP12; rickP15; hamkick; kick3.in

    // Amplitude begins at 1250.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 327,327, 41,      6,     6 }, // 379: hamP13; rimshot2

    // Amplitude begins at  845.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 328,328, 38,    100,   100 }, // 380: hamP14; rickP16; hamsnr1; snr1.ins

    // Amplitude begins at  771.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 329,329, 39,     26,    26 }, // 381: hamP15; handclap

    // Amplitude begins at  312.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 330,330, 49,     40,    40 }, // 382: hamP16; smallsnr

    // Amplitude begins at  914.5, peaks 2240.8 at 15.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 331,331, 83,  40000,     0 }, // 383: hamP17; rickP95; clsdhhat

    // Amplitude begins at 1289.5, peaks 1329.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 332,332, 59,  40000,  1093 }, // 384: hamP18; openhht2

    // Amplitude begins at 2642.4, peaks 2809.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 333,333, 24,     93,    93 }, // 385: hamP41; tom2

    // Amplitude begins at 2645.3, peaks 3034.8 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 333,333, 27,    133,   133 }, // 386: hamP42; tom2

    // Amplitude begins at 2593.0, peaks 2757.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.0s.
    { 333,333, 29,    126,    13 }, // 387: hamP43; tom2

    // Amplitude begins at 2513.8, peaks 3102.7 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.0s.
    { 333,333, 32,    126,    13 }, // 388: hamP44; tom2

    // Amplitude begins at  827.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 334,334, 84,    113,   113 }, // 389: hamP52; snare

    // Amplitude begins at  252.7, peaks  435.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 281,281, 79,     73,    73 }, // 390: hamP60; cymbal

    // Amplitude begins at   21.4, peaks  414.1 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 335,335, 61,    166,   166 }, // 391: hamP67; tambour

    // Amplitude begins at   53.7, peaks  464.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 335,335, 68,    140,   140 }, // 392: hamP68; tambour

    // Amplitude begins at  901.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 336,336, 36,     33,    33 }, // 393: hamP73; claves

    // Amplitude begins at  729.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 336,336, 60,     20,    20 }, // 394: hamP74; claves

    // Amplitude begins at    0.0, peaks  834.2 at 0.2s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 337,337, 36,    186,   186 }, // 395: hamP88; scratch

    // Amplitude begins at 1400.8, peaks 1786.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,115, 37,     73,    73 }, // 396: hamP90; taiko

    // Amplitude begins at 1209.9,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 338,338,  0,   1126,  1126 }, // 397: rickM76; Bass.ins

    // Amplitude begins at  852.4,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 339,339,  0,    286,   286 }, // 398: rickM77; Basnor04

    // Amplitude begins at    2.4, peaks 1046.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 340,340,  0,  40000,     0 }, // 399: rickM78; Synbass1

    // Amplitude begins at 1467.7, peaks 1896.9 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 341,341,  0,   1973,  1973 }, // 400: rickM79; Synbass2

    // Amplitude begins at 1146.6, peaks 1164.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 342,342,  0,    133,   133 }, // 401: rickM80; Pickbass

    // Amplitude begins at 1697.4, peaks 2169.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 343,343,  0,  40000,     0 }, // 402: rickM82; Harpsi1.

    // Amplitude begins at 1470.4, peaks 1727.4 at 4.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 344,344,  0,  40000,    33 }, // 403: rickM83; Guit_el3

    // Amplitude begins at    0.3, peaks 1205.9 at 0.1s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 345,345,  0,    400,   400 }, // 404: rickM88; Orchit2.

    // Amplitude begins at   36.8, peaks 4149.2 at 0.1s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 346,346,  0,   2920,  2920 }, // 405: rickM89; Brass11.

    // Amplitude begins at   42.9, peaks 3542.1 at 0.1s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 347,347,  0,   1520,  1520 }, // 406: rickM90; Brass2.i

    // Amplitude begins at   29.8, peaks 2195.8 at 0.1s,
    // fades to 20% at 3.9s, keyoff fades to 20% in 3.9s.
    { 348,348,  0,   3853,  3853 }, // 407: rickM91; Brass3.i

    // Amplitude begins at  251.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 349,349,  0,      6,     6 }, // 408: rickM92; Squ_wave

    // Amplitude begins at 3164.4,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 350,350,  0,   2313,  2313 }, // 409: rickM99; Agogo.in

    // Amplitude begins at 1354.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 351,351, 35,     66,    66 }, // 410: rickP13; kick1.in

    // Amplitude begins at  879.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 352,352, 38,     53,    53 }, // 411: rickP17; rickP19; snare1.i; snare4.i

    // Amplitude begins at  672.5, peaks  773.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 353,353, 38,    106,   106 }, // 412: rickP18; rickP20; snare2.i; snare5.i

    // Amplitude begins at 1555.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 31,     26,    26 }, // 413: rickP21; rocktom.

    // Amplitude begins at 2153.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 35,     40,    40 }, // 414: rickP22; rocktom.

    // Amplitude begins at 2439.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 38,     33,    33 }, // 415: rickP23; rocktom.

    // Amplitude begins at 2350.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 41,     40,    40 }, // 416: rickP24; rocktom.

    // Amplitude begins at 2086.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 354,354, 45,     40,    40 }, // 417: rickP25; rocktom.

    // Amplitude begins at 2389.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 354,354, 50,     53,    53 }, // 418: rickP26; rocktom.

    // Amplitude begins at    0.0, peaks  984.9 at 34.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 4.6s.
    { 355,355, 50,  40000,  4553 }, // 419: rickP93; openhht1

    // Amplitude begins at 1272.4, peaks 1316.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 332,332, 50,  40000,  1100 }, // 420: rickP94; openhht2

    // Amplitude begins at 2276.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 356,356, 72,     93,    93 }, // 421: rickP97; guirol.i

    // Amplitude begins at  878.1,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 148,148, 59,    153,   153 }, // 422: rickP99; timbale.

    // Amplitude begins at 1799.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 357,357, 64,     40,    40 }, // 423: rickP101; congas2.

    // Amplitude begins at 1958.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 357,357, 60,     33,    33 }, // 424: rickP102; congas2.

    // Amplitude begins at 2056.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 358,358, 72,     26,    26 }, // 425: rickP103; bongos.i

    // Amplitude begins at 1952.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 358,358, 62,     33,    33 }, // 426: rickP104; bongos.i

    // Amplitude begins at  616.5,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 359,359,  0,   1260,  1260 }, // 427: dMM0; hxMM0; Acoustic Grand Piano

    // Amplitude begins at  674.5,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 360,360,  0,   1733,  1733 }, // 428: dMM1; hxMM1; Bright Acoustic Piano

    // Amplitude begins at 2354.8, peaks 2448.9 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 361,362,  0,    873,   873 }, // 429: dMM2; hxMM2; Electric Grand Piano

    // Amplitude begins at 4021.1, peaks 5053.2 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 363,364,  0,   1286,  1286 }, // 430: dMM3; hxMM3; Honky-tonk Piano

    // Amplitude begins at 1643.0,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 365,365,  0,   1286,  1286 }, // 431: dMM4; hxMM4; Rhodes Paino

    // Amplitude begins at 4033.5, peaks 5133.2 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 366,367,  0,   1240,  1240 }, // 432: dMM5; hxMM5; Chorused Piano

    // Amplitude begins at 1822.4, peaks 2092.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 368,368,  0,  40000,  1133 }, // 433: dMM6; hxMM6; Harpsichord

    // Amplitude begins at  550.6, peaks  630.4 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 369,369,  0,    960,   960 }, // 434: dMM7; hxMM7; Clavinet

    // Amplitude begins at 2032.6, peaks 2475.9 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 370,370,  0,   1433,  1433 }, // 435: dMM8; hxMM8; Celesta

    // Amplitude begins at   59.9, peaks 1075.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 371,371,  0,    153,   153 }, // 436: dMM9; hxMM9; * Glockenspiel

    // Amplitude begins at 3882.8,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 372,372,  0,    906,   906 }, // 437: dMM10; hxMM10; * Music Box

    // Amplitude begins at 2597.9, peaks 2725.4 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 373,373,  0,   1460,  1460 }, // 438: dMM11; hxMM11; Vibraphone

    // Amplitude begins at 1033.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 374,374,  0,     40,    40 }, // 439: dMM12; hxMM12; Marimba

    // Amplitude begins at 2928.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 375,375,  0,     53,    53 }, // 440: dMM13; hxMM13; Xylophone

    // Amplitude begins at 1140.9, peaks 2041.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 376,376,  0,    466,   466 }, // 441: dMM14; hxMM14; * Tubular-bell

    // Amplitude begins at 1166.8, peaks 3114.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 377,377,  0,  40000,   580 }, // 442: dMM15; hxMM15; * Dulcimer

    // Amplitude begins at  854.0, peaks  939.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 378,378,  0,  40000,    53 }, // 443: dMM16; hxMM16; Hammond Organ

    // Amplitude begins at 1555.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 379,379,  0,    226,   226 }, // 444: dMM17; hxMM17; Percussive Organ

    // Amplitude begins at 2503.7, peaks 3236.9 at 2.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 380,381,  0,  40000,    40 }, // 445: dMM18; hxMM18; Rock Organ

    // Amplitude begins at    0.7, peaks  722.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 382,382,  0,  40000,   186 }, // 446: dMM19; hxMM19; Church Organ

    // Amplitude begins at   77.6, peaks 2909.7 at 9.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 383,383,  0,  40000,    66 }, // 447: dMM20; hxMM20; Reed Organ

    // Amplitude begins at    0.0, peaks 1059.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 384,385,  0,  40000,    26 }, // 448: dMM21; hxMM21; Accordion

    // Amplitude begins at    0.0, peaks 1284.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 386,386,  0,  40000,    46 }, // 449: dMM22; hxMM22; Harmonica

    // Amplitude begins at    2.6, peaks 1923.6 at 8.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 387,388,  0,  40000,   106 }, // 450: dMM23; hxMM23; Tango Accordion

    // Amplitude begins at 4825.0, peaks 6311.8 at 0.0s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 2.8s.
    { 389,389,  0,   2766,  2766 }, // 451: dMM24; hxMM24; Acoustic Guitar (nylon)

    // Amplitude begins at 2029.6, peaks 2275.6 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 390,390,  0,    940,   940 }, // 452: dMM25; hxMM25; Acoustic Guitar (steel)

    // Amplitude begins at 1913.7,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 391,391,  0,   1060,  1060 }, // 453: dMM26; hxMM26; Electric Guitar (jazz)

    // Amplitude begins at 1565.5,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 392,392,  0,    680,   680 }, // 454: dMM27; hxMM27; * Electric Guitar (clean)

    // Amplitude begins at 2719.9,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 393,393,  0,    866,   866 }, // 455: dMM28; hxMM28; Electric Guitar (muted)

    // Amplitude begins at  662.3, peaks  763.4 at 5.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 394,394,  0,  40000,     0 }, // 456: dMM29; Overdriven Guitar

    // Amplitude begins at 2816.8, peaks 3292.7 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 395,395,  0,   1226,  1226 }, // 457: dMM30; Distortion Guitar

    // Amplitude begins at 3191.9, peaks 3287.9 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 396,396,  0,   1653,  1653 }, // 458: dMM31; hxMM31; * Guitar Harmonics

    // Amplitude begins at 2762.8, peaks 2935.2 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 397,397,  0,    413,   413 }, // 459: dMM32; Acoustic Bass

    // Amplitude begins at 1228.1, peaks 1273.0 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 398,398,  0,   1720,  1720 }, // 460: dMM33; hxMM33; Electric Bass (finger)

    // Amplitude begins at 2291.3, peaks 2717.4 at 0.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 399,399,  0,  40000,   493 }, // 461: dMM34; hxMM34; Electric Bass (pick)

    // Amplitude begins at  950.8, peaks 3651.4 at 0.3s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    { 400,401,  0,   2620,  2620 }, // 462: dMM35; Fretless Bass

    // Amplitude begins at 2558.1, peaks 2881.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 402,402,  0,  40000,     6 }, // 463: dMM36; * Slap Bass 1

    // Amplitude begins at 1474.2, peaks 1576.9 at 0.0s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 1.7s.
    { 403,403,  0,   1693,  1693 }, // 464: dMM37; hxMM37; Slap Bass 2

    // Amplitude begins at 1147.4, peaks 1232.1 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 404,404,  0,    233,   233 }, // 465: dMM38; hxMM38; Synth Bass 1

    // Amplitude begins at  803.7, peaks 2475.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 405,405,  0,  40000,    13 }, // 466: dMM39; hxMM39; Synth Bass 2

    // Amplitude begins at    0.3, peaks 2321.0 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 406,406,  0,  40000,   106 }, // 467: dMM40; hxMM40; Violin

    // Amplitude begins at   23.1, peaks 3988.7 at 12.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 407,407,  0,  40000,   160 }, // 468: dMM41; hxMM41; Viola

    // Amplitude begins at 1001.0, peaks 1631.3 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 408,408,  0,  40000,   193 }, // 469: dMM42; hxMM42; Cello

    // Amplitude begins at  570.1, peaks 1055.7 at 24.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 409,409,  0,  40000,   260 }, // 470: dMM43; hxMM43; Contrabass

    // Amplitude begins at 1225.0, peaks 4124.2 at 32.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 410,411,  0,  40000,   386 }, // 471: dMM44; hxMM44; Tremolo Strings

    // Amplitude begins at 1088.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 412,412,  0,     13,    13 }, // 472: dMM45; hxMM45; Pizzicato Strings

    // Amplitude begins at 2701.6, peaks 2794.5 at 0.1s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 413,413,  0,   1193,  1193 }, // 473: dMM46; hxMM46; Orchestral Harp

    // Amplitude begins at 1102.2, peaks 1241.9 at 0.0s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 414,414,  0,    680,   680 }, // 474: dMM47; hxMM47; * Timpani

    // Amplitude begins at 1149.7, peaks 2522.2 at 36.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 415,416,  0,  40000,   193 }, // 475: dMM48; hxMM48; String Ensemble 1

    // Amplitude begins at  132.3, peaks 2492.8 at 25.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 417,418,  0,  40000,   266 }, // 476: dMM49; hxMM49; String Ensemble 2

    // Amplitude begins at    3.2, peaks 1845.6 at 32.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 419,419,  0,  40000,   480 }, // 477: dMM50; hxMM50; Synth Strings 1

    // Amplitude begins at    0.0, peaks 3217.2 at 3.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 420,421,  0,  40000,   646 }, // 478: dMM51; hxMM51; Synth Strings 2

    // Amplitude begins at    7.2, peaks 4972.4 at 32.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 422,423,  0,  40000,   546 }, // 479: dMM52; hxMM52; Choir Aahs

    // Amplitude begins at 1100.3, peaks 4805.2 at 0.1s,
    // fades to 20% at 2.5s, keyoff fades to 20% in 2.5s.
    { 424,424,  0,   2533,  2533 }, // 480: dMM53; hxMM53; Voice Oohs

    // Amplitude begins at    0.0, peaks 3115.2 at 26.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 425,425,  0,  40000,   213 }, // 481: dMM54; hxMM54; Synth Voice

    // Amplitude begins at    1.3, peaks 4556.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 426,427,  0,  40000,   640 }, // 482: dMM55; hxMM55; Orchestra Hit

    // Amplitude begins at    0.9, peaks 1367.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 428,428,  0,  40000,    13 }, // 483: dMM56; hxMM56; Trumpet

    // Amplitude begins at    1.8, peaks  873.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 429,429,  0,  40000,     0 }, // 484: dMM57; hxMM57; Trombone

    // Amplitude begins at  260.9, peaks  558.8 at 0.0s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 430,430,  0,   2393,  2393 }, // 485: dMM58; Tuba

    // Amplitude begins at   11.2, peaks 1622.8 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 431,431,  0,  40000,    86 }, // 486: dMM59; hxMM59; Muted Trumpet

    // Amplitude begins at    4.9, peaks 3060.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 432,433,  0,  40000,    20 }, // 487: dMM60; hxMM60; French Horn

    // Amplitude begins at    1.3, peaks 1502.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 434,435,  0,  40000,    20 }, // 488: dMM61; hxMM61; Brass Section

    // Amplitude begins at  749.0, peaks 1624.5 at 0.1s,
    // fades to 20% at 3.0s, keyoff fades to 20% in 3.0s.
    { 436,437,  0,   2966,  2966 }, // 489: dMM62; hxMM62; Synth Brass 1

    // Amplitude begins at   14.4, peaks  901.7 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 438,439,  0,   1566,  1566 }, // 490: dMM63; hxMM63; Synth Bass 2

    // Amplitude begins at    1.0, peaks  943.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 440,440,  0,  40000,    40 }, // 491: dMM64; hxMM64; Soprano Sax

    // Amplitude begins at    2.4, peaks 2220.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 441,441,  0,  40000,    86 }, // 492: dMM65; hxMM65; Alto Sax

    // Amplitude begins at   94.5, peaks  726.7 at 30.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 442,442,  0,  40000,     6 }, // 493: dMM66; hxMM66; Tenor Sax

    // Amplitude begins at  178.3, peaks 2252.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 443,443,  0,  40000,     6 }, // 494: dMM67; hxMM67; Baritone Sax

    // Amplitude begins at    0.0, peaks 1786.2 at 38.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 444,444,  0,  40000,    33 }, // 495: dMM68; hxMM68; Oboe

    // Amplitude begins at 2886.1, peaks 3448.9 at 14.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 445,445,  0,  40000,    73 }, // 496: dMM69; hxMM69; English Horn

    // Amplitude begins at 1493.5, peaks 2801.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 446,446,  0,  40000,     6 }, // 497: dMM70; hxMM70; Bassoon

    // Amplitude begins at 1806.2, peaks 2146.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 447,447,  0,  40000,    20 }, // 498: dMM71; hxMM71; Clarinet

    // Amplitude begins at    0.0, peaks  745.7 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 448,448,  0,  40000,     0 }, // 499: dMM72; hxMM72; Piccolo

    // Amplitude begins at    0.0, peaks 3494.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 449,449,  0,  40000,    20 }, // 500: dMM73; hxMM73; Flute

    // Amplitude begins at    2.8, peaks 1080.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 450,450,  0,  40000,    46 }, // 501: dMM74; hxMM74; Recorder

    // Amplitude begins at    0.0, peaks 1241.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 451,451,  0,  40000,    20 }, // 502: dMM75; hxMM75; Pan Flute

    // Amplitude begins at    0.0, peaks 3619.6 at 20.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 452,452,  0,  40000,   400 }, // 503: dMM76; hxMM76; Bottle Blow

    // Amplitude begins at    0.0, peaks  817.4 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 453,453,  0,    140,   140 }, // 504: dMM77; hxMM77; * Shakuhachi

    // Amplitude begins at    0.0, peaks 4087.8 at 0.2s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.0s.
    { 454,454,  0,    240,     6 }, // 505: dMM78; hxMM78; Whistle

    // Amplitude begins at    1.8, peaks 4167.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 455,455,  0,  40000,    73 }, // 506: dMM79; hxMM79; Ocarina

    // Amplitude begins at 1515.8, peaks 3351.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 456,457,  0,  40000,    86 }, // 507: dMM80; hxMM80; Lead 1 (square)

    // Amplitude begins at 2949.4, peaks 4563.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 458,459,  0,  40000,    60 }, // 508: dMM81; hxMM81; Lead 2 (sawtooth)

    // Amplitude begins at    0.0, peaks  915.0 at 37.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 460,460,  0,  40000,    73 }, // 509: dMM82; hxMM82; Lead 3 (calliope)

    // Amplitude begins at 1585.2, peaks 3647.5 at 26.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 461,462,  0,  40000,   513 }, // 510: dMM83; hxMM83; Lead 4 (chiffer)

    // Amplitude begins at  789.1, peaks  848.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 463,463,  0,  40000,     6 }, // 511: dMM84; hxMM84; Lead 5 (charang)

    // Amplitude begins at 2971.5, peaks 9330.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 464,465,  0,  40000,   246 }, // 512: dMM85; hxMM85; Lead 6 (voice)

    // Amplitude begins at 1121.6, peaks 3864.6 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 466,467,  0,    313,   313 }, // 513: dMM86; hxMM86; Lead 7 (5th sawtooth)

    // Amplitude begins at  263.0, peaks  310.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 468,468,  0,  40000,    20 }, // 514: dMM87; dMM88; hxMM87; hxMM88; * Lead 8 (bass & lead)

    // Amplitude begins at    0.0, peaks 2982.5 at 1.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 469,470,  0,  40000,   246 }, // 515: dMM89; hxMM89; Pad 2 (warm)

    // Amplitude begins at 1450.1,
    // fades to 20% at 3.9s, keyoff fades to 20% in 3.9s.
    { 471,471,  0,   3886,  3886 }, // 516: dMM90; hxMM90; Pad 3 (polysynth)

    // Amplitude begins at  121.8, peaks 3944.0 at 0.1s,
    // fades to 20% at 4.2s, keyoff fades to 20% in 4.2s.
    { 472,473,  0,   4193,  4193 }, // 517: dMM91; hxMM91; Pad 4 (choir)

    // Amplitude begins at    0.0, peaks 1612.5 at 0.2s,
    // fades to 20% at 2.5s, keyoff fades to 20% in 2.5s.
    { 474,474,  0,   2473,  2473 }, // 518: dMM92; hxMM92; Pad 5 (bowed glass)

    // Amplitude begins at   83.2, peaks 1154.9 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.1s.
    { 475,475,  0,  40000,  2100 }, // 519: dMM93; hxMM93; Pad 6 (metal)

    // Amplitude begins at  198.7, peaks 3847.6 at 0.2s,
    // fades to 20% at 3.4s, keyoff fades to 20% in 0.0s.
    { 476,401,  0,   3373,     6 }, // 520: dMM94; hxMM94; Pad 7 (halo)

    // Amplitude begins at    5.9, peaks  912.6 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 477,477,  0,  40000,   840 }, // 521: dMM95; hxMM95; Pad 8 (sweep)

    // Amplitude begins at  978.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 478,478,  0,     26,    26 }, // 522: dMM96; hxMM96; FX 1 (rain)

    // Amplitude begins at    0.0, peaks  696.6 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 479,480,  0,  40000,    46 }, // 523: dMM97; hxMM97; FX 2 (soundtrack)

    // Amplitude begins at 1791.1, peaks 2994.3 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 481,481,  0,   1046,  1046 }, // 524: dMM98; hxMM98; * FX 3 (crystal)

    // Amplitude begins at 3717.1, peaks 5220.9 at 0.2s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 482,483,  0,   1866,  1866 }, // 525: dMM99; hxMM99; FX 4 (atmosphere)

    // Amplitude begins at 3835.5, peaks 4843.6 at 0.1s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    { 484,485,  0,   2220,  2220 }, // 526: dMM100; hxMM100; FX 5 (brightness)

    // Amplitude begins at    0.0, peaks 1268.0 at 0.3s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 0.2s.
    { 486,486,  0,   1160,   186 }, // 527: dMM101; hxMM101; FX 6 (goblin)

    // Amplitude begins at    0.0, peaks 1649.2 at 0.2s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 487,487,  0,    746,   746 }, // 528: dMM102; hxMM102; FX 7 (echo drops)

    // Amplitude begins at    0.0, peaks 1255.5 at 0.3s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    { 488,488,  0,   2646,  2646 }, // 529: dMM103; hxMM103; * FX 8 (star-theme)

    // Amplitude begins at    0.3, peaks 2711.1 at 0.1s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 489,489,  0,   1206,  1206 }, // 530: dMM104; hxMM104; Sitar

    // Amplitude begins at 1221.5, peaks 2663.6 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 490,490,  0,  40000,   406 }, // 531: dMM105; hxMM105; Banjo

    // Amplitude begins at 1658.6,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 491,492,  0,    406,   406 }, // 532: dMM106; hxMM106; Shamisen

    // Amplitude begins at 1657.4, peaks 2263.2 at 0.1s,
    // fades to 20% at 3.6s, keyoff fades to 20% in 3.6s.
    { 493,493,  0,   3566,  3566 }, // 533: dMM107; hxMM107; Koto

    // Amplitude begins at 2222.4,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 494,494,  0,   2940,  2940 }, // 534: dMM108; hxMM108; Kalimba

    // Amplitude begins at    0.2, peaks  554.9 at 8.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 495,495,  0,  40000,    73 }, // 535: dMM109; hxMM109; Bag Pipe

    // Amplitude begins at 2646.1, peaks 3358.8 at 33.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 496,496,  0,  40000,   420 }, // 536: dMM110; hxMM110; Fiddle

    // Amplitude begins at    1.4, peaks 2985.9 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 497,497,  0,  40000,   246 }, // 537: dMM111; hxMM111; Shanai

    // Amplitude begins at 1438.2, peaks 1485.7 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 498,498,  0,   1220,  1220 }, // 538: dMM112; hxMM112; Tinkle Bell

    // Amplitude begins at 3327.5, peaks 3459.2 at 0.0s,
    // fades to 20% at 1.4s, keyoff fades to 20% in 1.4s.
    { 499,499,  0,   1440,  1440 }, // 539: dMM113; hxMM113; Agogo

    // Amplitude begins at 1245.4, peaks 2279.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 500,500,  0,  40000,   213 }, // 540: dMM114; hxMM114; Steel Drums

    // Amplitude begins at 1567.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 501,501,  0,    113,   113 }, // 541: dMM115; hxMM115; Woodblock

    // Amplitude begins at 2897.0, peaks 3350.1 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 502,502,  0,     40,    40 }, // 542: dMM116; hxMM116; Taiko Drum

    // Amplitude begins at 3180.9, peaks 3477.5 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 503,503,  0,    120,   120 }, // 543: dMM117; hxMM117; Melodic Tom

    // Amplitude begins at 4290.3, peaks 4378.4 at 0.0s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 504,504,  0,    680,   680 }, // 544: dMM118; hxMM118; Synth Drum

    // Amplitude begins at    0.0, peaks 2636.1 at 0.4s,
    // fades to 20% at 4.6s, keyoff fades to 20% in 4.6s.
    { 505,505,  0,   4586,  4586 }, // 545: dMM119; hxMM119; Reverse Cymbal

    // Amplitude begins at    0.3, peaks 1731.3 at 0.2s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 506,506,  0,    426,   426 }, // 546: dMM120; hxMM120; Guitar Fret Noise

    // Amplitude begins at    0.0, peaks 3379.5 at 0.2s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 507,507,  0,    360,   360 }, // 547: dMM121; hxMM121; Breath Noise

    // Amplitude begins at    0.0, peaks  365.3 at 2.3s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 508,508,  0,   2926,  2926 }, // 548: dMM122; hxMM122; Seashore

    // Amplitude begins at    0.0, peaks 1372.7 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 509,509,  0,    306,   306 }, // 549: dMM123; hxMM123; Bird Tweet

    // Amplitude begins at  708.2, peaks  797.4 at 30.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 510,510,  0,  40000,    73 }, // 550: dMM124; hxMM124; Telephone Ring

    // Amplitude begins at    0.0, peaks  764.4 at 1.7s,
    // fades to 20% at 1.7s, keyoff fades to 20% in 0.0s.
    { 511,511, 17,   1686,     6 }, // 551: dMM125; hxMM125; Helicopter

    // Amplitude begins at    0.0, peaks  356.0 at 27.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.9s.
    { 512,512, 65,  40000,  1873 }, // 552: dMM126; hxMM126; Applause

    // Amplitude begins at  830.5,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 513,513,  0,    160,   160 }, // 553: dMM127; hxMM127; Gun Shot

    // Amplitude begins at 1420.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 514,514, 38,     60,    60 }, // 554: dMP35; hxMP35; Acoustic Bass Drum

    // Amplitude begins at 1343.2, peaks 1616.4 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 515,515, 25,     33,    33 }, // 555: dMP36; hxMP36; Acoustic Bass Drum

    // Amplitude begins at 1457.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 516,516, 83,     60,    60 }, // 556: dMP37; hxMP37; Slide Stick

    // Amplitude begins at 1244.0,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 517,517, 32,    506,   506 }, // 557: dMP38; hxMP38; Acoustic Snare

    // Amplitude begins at  339.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 518,518, 60,     40,    40 }, // 558: dMP39; hxMP39; Hand Clap

    // Amplitude begins at  707.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 519,520, 36,     46,    46 }, // 559: dMP40; Electric Snare

    // Amplitude begins at  867.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 521,521, 15,     73,    73 }, // 560: dMP41; hxMP41; Low Floor Tom

    // Amplitude begins at  396.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 522,522, 88,    113,   113 }, // 561: dMP42; hxMP42; Closed High-Hat

    // Amplitude begins at  779.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 523,523, 19,    100,   100 }, // 562: dMP43; hxMP43; High Floor Tom

    // Amplitude begins at   77.1, peaks  358.4 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 524,524, 88,     20,    20 }, // 563: dMP44; dMP69; hxMP44; hxMP69; Cabasa

    // Amplitude begins at 1217.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 525,525, 21,     93,    93 }, // 564: dMP45; hxMP45; Low Tom

    // Amplitude begins at 1359.4,
    // fades to 20% at 3.3s, keyoff fades to 20% in 3.3s.
    { 526,526, 79,   3346,  3346 }, // 565: dMP46; hxMP46; Open High Hat

    // Amplitude begins at 1204.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 525,525, 26,    100,   100 }, // 566: dMP47; hxMP47; Low-Mid Tom

    // Amplitude begins at 1197.1,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 525,525, 28,    100,   100 }, // 567: dMP48; hxMP48; High-Mid Tom

    // Amplitude begins at  391.2,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 527,527, 60,    293,   293 }, // 568: dMP49; dMP57; hxMP49; hxMP57; Crash Cymbal 1

    // Amplitude begins at 1166.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 525,525, 32,     80,    80 }, // 569: dMP50; hxMP50; High Tom

    // Amplitude begins at  475.1,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 528,528, 60,    266,   266 }, // 570: dMP51; dMP59; hxMP51; hxMP59; Ride Cymbal 1

    // Amplitude begins at  348.2,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 529,529, 96,    193,   193 }, // 571: dMP52; hxMP52; Chinses Cymbal

    // Amplitude begins at  426.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 530,530, 72,    133,   133 }, // 572: dMP53; hxMP53; Ride Bell

    // Amplitude begins at  289.3, peaks 1225.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 531,531, 79,    140,   140 }, // 573: dMP54; hxMP54; Tambourine

    // Amplitude begins at 1313.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 532,532, 69,    126,   126 }, // 574: dMP55; Splash Cymbal

    // Amplitude begins at 1403.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 533,533, 71,    146,   146 }, // 575: dMP56; hxMP56; Cowbell

    // Amplitude begins at 1222.3,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 534,534,  0,    200,   200 }, // 576: dMP58; hxMP58; Vibraslap

    // Amplitude begins at 1193.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 535,535, 60,     93,    93 }, // 577: dMP60; hxMP60; High Bongo

    // Amplitude begins at 1365.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 535,535, 54,    106,   106 }, // 578: dMP61; hxMP61; Low Bango

    // Amplitude begins at 2621.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 536,536, 72,     66,    66 }, // 579: dMP62; hxMP62; Mute High Conga

    // Amplitude begins at 2596.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 536,536, 67,     66,    66 }, // 580: dMP63; hxMP63; Open High Conga

    // Amplitude begins at 2472.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 536,536, 60,     66,    66 }, // 581: dMP64; hxMP64; Low Conga

    // Amplitude begins at 2936.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 537,537, 55,     66,    66 }, // 582: dMP65; hxMP65; High Timbale

    // Amplitude begins at 3142.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 537,537, 48,     80,    80 }, // 583: dMP66; hxMP66; Low Timbale

    // Amplitude begins at 1311.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 538,538, 77,    133,   133 }, // 584: dMP67; hxMP67; High Agogo

    // Amplitude begins at 1337.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 538,538, 72,    133,   133 }, // 585: dMP68; hxMP68; Low Agogo

    // Amplitude begins at  189.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 539,539,  0,      6,     6 }, // 586: dMP70; hxMP70; Maracas

    // Amplitude begins at  252.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 540,540, 49,     20,    20 }, // 587: dMP71; dMP72; dMP73; dMP74; dMP79; hxMP71; hxMP72; hxMP73; hxMP74; hxMP79; Long Guiro

    // Amplitude begins at 1479.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 541,541, 73,     60,    60 }, // 588: dMP75; hxMP75; Claves

    // Amplitude begins at 1491.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 541,541, 68,     60,    60 }, // 589: dMP76; hxMP76; High Wood Block

    // Amplitude begins at 1618.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 541,541, 61,     80,    80 }, // 590: dMP77; hxMP77; Low Wood Block

    // Amplitude begins at  189.2, peaks  380.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 542,542,  0,    146,   146 }, // 591: dMP78; hxMP78; Mute Cuica

    // Amplitude begins at 1698.0,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 543,543, 90,    760,   760 }, // 592: dMP80; hxMP80; Mute Triangle

    // Amplitude begins at 1556.4,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 544,544, 90,    433,   433 }, // 593: dMP81; hxMP81; Open Triangle

    // Amplitude begins at 1237.2,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 545,545,  0,  40000,    40 }, // 594: hxMM29; Overdriven Guitar

    // Amplitude begins at  763.4, peaks  782.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 546,546,  0,  40000,    60 }, // 595: hxMM30; Distortion Guitar

    // Amplitude begins at  990.9, peaks 1053.6 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 547,547,  0,   2273,  2273 }, // 596: hxMM32; Acoustic Bass

    // Amplitude begins at  681.3, peaks 1488.4 at 0.0s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 548,548,  0,   1346,  1346 }, // 597: hxMM35; Fretless Bass

    // Amplitude begins at 2940.0, peaks 3034.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 549,550,  0,  40000,     6 }, // 598: hxMM36; * Slap Bass 1

    // Amplitude begins at   66.1, peaks  600.7 at 0.0s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 551,551,  0,   2260,  2260 }, // 599: hxMM58; Tuba

    // Amplitude begins at 2159.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 552,553, 36,     40,    40 }, // 600: hxMP40; Electric Snare

    // Amplitude begins at 1148.7, peaks 1298.7 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 554,554, 69,    513,   513 }, // 601: hxMP55; Splash Cymbal

    // Amplitude begins at  893.0, peaks  914.4 at 0.0s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 555,555,  0,    660,   660 }, // 602: sGM6; Harpsichord

    // Amplitude begins at 3114.7, peaks 3373.0 at 0.0s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 556,556,  0,    820,   820 }, // 603: sGM9; Glockenspiel

    // Amplitude begins at 1173.2, peaks 1746.9 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 557,557,  0,    946,   946 }, // 604: sGM14; Tubular Bells

    // Amplitude begins at  520.4, peaks 1595.5 at 20.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 558,558,  0,  40000,   140 }, // 605: sGM19; Church Organ

    // Amplitude begins at 1154.6, peaks 1469.3 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 559,559,  0,    906,   906 }, // 606: sGM24; Acoustic Guitar1

    // Amplitude begins at   84.3, peaks 3105.0 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 560,560,  0,  40000,     0 }, // 607: sGM44; Tremulo Strings

    // Amplitude begins at 2434.5, peaks 2872.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 561,561,  0,    206,   206 }, // 608: sGM45; Pizzicato String

    // Amplitude begins at 2497.2, peaks 3945.4 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 562,562,  0,   1633,  1633 }, // 609: sGM46; Orchestral Harp

    // Amplitude begins at 1574.7, peaks 1635.3 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 563,563,  0,    260,   260 }, // 610: sGM47; Timpany

    // Amplitude begins at  362.3, peaks 3088.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 564,564,  0,  40000,     0 }, // 611: sGM48; String Ensemble1

    // Amplitude begins at    0.0, peaks  969.5 at 0.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 565,565,  0,  40000,   233 }, // 612: sGM49; String Ensemble2

    // Amplitude begins at 2000.6, peaks 3290.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 566,566,  0,  40000,  1106 }, // 613: sGM50; Synth Strings 1

    // Amplitude begins at 1903.9, peaks 3244.2 at 35.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 567,567,  0,  40000,     0 }, // 614: sGM52; Choir Aahs

    // Amplitude begins at  462.4, peaks 2679.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 568,568,  0,    126,   126 }, // 615: sGM55; Orchestra Hit

    // Amplitude begins at   42.7, peaks  937.1 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 569,569,  0,  40000,    40 }, // 616: sGM56; Trumpet

    // Amplitude begins at   49.7, peaks 3958.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 570,570,  0,  40000,    73 }, // 617: sGM57; Trombone

    // Amplitude begins at   42.8, peaks 1043.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 571,571,  0,  40000,    26 }, // 618: sGM58; Tuba

    // Amplitude begins at    3.1, peaks 1099.6 at 0.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 572,572,  0,  40000,    73 }, // 619: sGM60; French Horn

    // Amplitude begins at   52.8, peaks 3225.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 573,573,  0,  40000,    26 }, // 620: sGM61; Brass Section

    // Amplitude begins at   52.0, peaks 1298.7 at 23.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 574,574,  0,  40000,     6 }, // 621: sGM68; Oboe

    // Amplitude begins at  577.9, peaks 1638.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 575,575,  0,  40000,     6 }, // 622: sGM70; Bassoon

    // Amplitude begins at    5.6, peaks 1972.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 576,576,  0,  40000,    33 }, // 623: sGM71; Clarinet

    // Amplitude begins at   41.5, peaks 3936.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 577,577,  0,  40000,     0 }, // 624: sGM72; Piccolo

    // Amplitude begins at    6.8, peaks 2790.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 578,578,  0,  40000,    13 }, // 625: sGM73; Flute

    // Amplitude begins at    0.0, peaks  798.5 at 2.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 579,579,  0,  40000,    46 }, // 626: sGM95; Pad 8 sweep

    // Amplitude begins at 1628.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 580,580,  0,    120,   120 }, // 627: sGM116; Taiko Drum

    // Amplitude begins at 1481.7,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 581,581,  0,   2260,  2260 }, // 628: sGM118; Synth Drum

    // Amplitude begins at    0.0, peaks  488.6 at 2.3s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 582,582,  0,   2366,  2366 }, // 629: sGM119; Reverse Cymbal

    // Amplitude begins at 1523.0, peaks 1718.4 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 583,583, 16,     20,    20 }, // 630: sGP35; sGP36; Ac Bass Drum; Bass Drum 1

    // Amplitude begins at  649.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 584,584,  6,      6,     6 }, // 631: sGP37; Side Stick

    // Amplitude begins at 1254.1,
    // fades to 20% at 3.1s, keyoff fades to 20% in 3.1s.
    { 581,581, 14,   3060,  3060 }, // 632: sGP38; sGP40; Acoustic Snare; Electric Snare

    // Amplitude begins at  600.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 585,585, 14,     53,    53 }, // 633: sGP39; Hand Clap

    // Amplitude begins at 1914.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 586,586,  0,    106,   106 }, // 634: sGP41; sGP43; sGP45; sGP47; sGP48; sGP50; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom

    // Amplitude begins at  265.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 587,587, 12,     40,    40 }, // 635: sGP42; Closed High Hat

    // Amplitude begins at  267.6,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 588,588, 12,    266,   266 }, // 636: sGP44; Pedal High Hat

    // Amplitude begins at  398.8,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 589,589, 12,    893,   893 }, // 637: sGP46; Open High Hat

    // Amplitude begins at  133.6, peaks  393.7 at 0.0s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 590,590, 14,    753,   753 }, // 638: sGP49; Crash Cymbal 1

    // Amplitude begins at  480.6,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 136,136, 14,    686,   686 }, // 639: mGP51; mGP59; sGP51; sGP59; Ride Cymbal 1; Ride Cymbal 2

    // Amplitude begins at  180.7, peaks  720.4 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 591,591, 14,   1466,  1466 }, // 640: sGP52; Chinese Cymbal

    // Amplitude begins at  913.7, peaks  929.0 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 138,138, 14,    353,   353 }, // 641: mGP53; sGP53; Ride Bell

    // Amplitude begins at   24.4, peaks 1209.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 592,592, 14,    113,   113 }, // 642: sGP54; Tambourine

    // Amplitude begins at  362.1,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 593,593, 78,    280,   280 }, // 643: sGP55; Splash Cymbal

    // Amplitude begins at  675.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 141,141, 17,     20,    20 }, // 644: mGP56; sGP56; Cow Bell

    // Amplitude begins at  133.6, peaks  399.9 at 0.0s,
    // fades to 20% at 2.9s, keyoff fades to 20% in 2.9s.
    { 594,594, 14,   2946,  2946 }, // 645: sGP57; Crash Cymbal 2

    // Amplitude begins at  601.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 595,595,  0,    126,   126 }, // 646: sGP58; Vibraslap

    // Amplitude begins at  494.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 143,143,  6,      6,     6 }, // 647: mGP60; sGP60; High Bongo

    // Amplitude begins at 1333.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 144,144,  1,      6,     6 }, // 648: mGP61; oGP61; sGP61; Low Bongo

    // Amplitude begins at  272.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 145,145,  1,     20,    20 }, // 649: mGP62; sGP62; Mute High Conga

    // Amplitude begins at 1581.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 146,146,  1,      6,     6 }, // 650: mGP63; sGP63; Open High Conga

    // Amplitude begins at  852.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 147,147,  1,      6,     6 }, // 651: mGP64; sGP64; Low Conga

    // Amplitude begins at  694.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 148,148,  1,     20,    20 }, // 652: mGP65; sGP65; High Timbale

    // Amplitude begins at  840.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 149,149,  0,     40,    40 }, // 653: mGP66; sGP66; Low Timbale

    // Amplitude begins at  776.7,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 150,150,  3,    186,   186 }, // 654: mGP67; sGP67; High Agogo

    // Amplitude begins at  860.2, peaks 1114.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 151,151,  3,    126,   126 }, // 655: mGP68; sGP68; Low Agogo

    // Amplitude begins at    0.2, peaks  358.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 596,596, 14,    106,   106 }, // 656: sGP69; Cabasa

    // Amplitude begins at  134.4, peaks  332.5 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 153,153, 14,     20,    20 }, // 657: mGP70; sGP70; Maracas

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 154,154,215,      0,     0 }, // 658: mGP71; sGP71; Short Whistle

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 155,155,215,      0,     0 }, // 659: mGP72; sGP72; Long Whistle

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 156,156,128,      0,     0 }, // 660: sGP73; Short Guiro

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 157,157,128,      0,     0 }, // 661: sGP74; Long Guiro

    // Amplitude begins at  959.9, peaks 1702.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 158,158,  6,     53,    53 }, // 662: mGP75; oGP75; sGP75; Claves

    // Amplitude begins at  398.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 597,597,  2,      6,     6 }, // 663: sGP76; High Wood Block

    // Amplitude begins at  814.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 160,160,  6,      6,     6 }, // 664: mGP77; sGP77; Low Wood Block

    // Amplitude begins at    2.0, peaks 1722.0 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 161,161,  1,     93,    93 }, // 665: mGP78; sGP78; Mute Cuica

    // Amplitude begins at 1277.6, peaks 2872.7 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 162,162, 65,    346,   346 }, // 666: sGP79; Open Cuica

    // Amplitude begins at 1146.2,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 163,163, 10,    386,   386 }, // 667: mGP80; sGP80; Mute Triangle

    // Amplitude begins at 1179.8, peaks 1188.2 at 0.0s,
    // fades to 20% at 3.0s, keyoff fades to 20% in 3.0s.
    { 164,164, 10,   3046,  3046 }, // 668: mGP81; sGP81; Open Triangle

    // Amplitude begins at    0.2, peaks  373.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 165,165, 14,     53,    53 }, // 669: mGP82; sGP82; Shaker

    // Amplitude begins at    0.0, peaks  959.9 at 0.2s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 166,166, 14,    440,   440 }, // 670: mGP83; sGP83; Jingle Bell

    // Amplitude begins at 1388.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 167,167,  5,      6,     6 }, // 671: mGP84; sGP84; Bell Tree

    // Amplitude begins at  566.5, peaks  860.2 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 168,168,  2,     46,    46 }, // 672: mGP85; sGP85; Castanets

    // Amplitude begins at 1845.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 169,169,  1,     13,    13 }, // 673: mGP86; sGP86; Mute Surdu

    // Amplitude begins at 1953.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 131,131,  0,     40,    40 }, // 674: mGP41; mGP43; mGP45; mGP47; mGP48; mGP50; mGP87; sGP87; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; Open Surdu

    // Amplitude begins at 4477.5, peaks 6751.4 at 0.1s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 598,599,  0,   1800,  1800 }, // 675: qGM0; AcouGrandPiano

    // Amplitude begins at 3079.2, peaks 4137.9 at 0.3s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 600,601,  0,   1486,  1486 }, // 676: qGM1; BrightAcouGrand

    // Amplitude begins at 1132.6, peaks 1399.6 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 602,603,  0,   1940,  1940 }, // 677: qGM2; ElecGrandPiano

    // Amplitude begins at 3269.8, peaks 3838.6 at 0.1s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 604,605,  0,    673,   673 }, // 678: qGM3; Honky-tonkPiano

    // Amplitude begins at 2750.2, peaks 3130.0 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 606,607,  0,   1080,  1080 }, // 679: qGM4; Rhodes Piano

    // Amplitude begins at 3945.7, peaks 5578.5 at 0.1s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 608,609,  0,   1133,  1133 }, // 680: qGM5; Chorused Piano

    // Amplitude begins at 1928.3, peaks 2129.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 610,611,  0,  40000,     6 }, // 681: qGM6; Harpsichord

    // Amplitude begins at 2293.1, peaks 2446.0 at 0.0s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 612,613,  0,    780,   780 }, // 682: qGM7; Clavinet

    // Amplitude begins at 5791.8, peaks 6530.2 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 614,615,  0,    626,   626 }, // 683: qGM8; Celesta

    // Amplitude begins at 1985.4, peaks 2087.5 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 616,617,  0,   1180,  1180 }, // 684: qGM9; Glockenspiel

    // Amplitude begins at 2286.5, peaks 4615.4 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 180,618,  0,    333,   333 }, // 685: qGM10; Music box

    // Amplitude begins at 1568.6, peaks 1706.8 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 619,620,  0,   1226,  1226 }, // 686: qGM11; Vibraphone

    // Amplitude begins at 2801.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 621,622,  0,    140,   140 }, // 687: qGM12; Marimba

    // Amplitude begins at 3397.2,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 623,624,  0,     80,    80 }, // 688: qGM13; Xylophone

    // Amplitude begins at 2842.0, peaks 4706.3 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 625,626,  0,    893,   893 }, // 689: qGM14; Tubular Bells

    // Amplitude begins at    2.6, peaks  923.3 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 627,628,  0,    426,   426 }, // 690: qGM15; Dulcimer

    // Amplitude begins at 1957.2, peaks 2420.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 629,630,  0,  40000,     6 }, // 691: qGM16; Hammond Organ

    // Amplitude begins at 2298.8, peaks 3038.1 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.0s.
    { 631,632,  0,    160,     6 }, // 692: qGM17; Percussive Organ

    // Amplitude begins at  791.1, peaks 2905.2 at 34.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 633,634,  0,  40000,     6 }, // 693: qGM18; Rock Organ

    // Amplitude begins at 1807.8, peaks 2893.6 at 10.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.8s.
    { 635,636,  0,  40000,  1846 }, // 694: qGM19; Church Organ

    // Amplitude begins at 2133.4, peaks 2785.7 at 28.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 637,638,  0,  40000,   213 }, // 695: qGM20; Reed Organ

    // Amplitude begins at    7.9, peaks 3918.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 639,640,  0,  40000,     6 }, // 696: qGM21; Accordion

    // Amplitude begins at 2247.2, peaks 3389.7 at 7.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 641,642,  0,  40000,   226 }, // 697: qGM22; Harmonica

    // Amplitude begins at    0.9, peaks 1919.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 643,644,  0,  40000,     6 }, // 698: qGM23; Tango Accordion

    // Amplitude begins at 2100.3,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 645,646,  0,    886,   886 }, // 699: qGM24; Acoustic Guitar1

    // Amplitude begins at 3850.9,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 647,648,  0,   1966,  1966 }, // 700: qGM25; Acoustic Guitar2

    // Amplitude begins at 3321.6, peaks 4754.4 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 649,650,  0,   1993,  1993 }, // 701: qGM26; Electric Guitar1

    // Amplitude begins at 2748.4, peaks 3648.4 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 651,652,  0,   1466,  1466 }, // 702: qGM27; Electric Guitar2

    // Amplitude begins at 3478.2, peaks 3876.2 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 653,654,  0,    393,   393 }, // 703: qGM28; Electric Guitar3

    // Amplitude begins at  751.6, peaks 3648.5 at 0.0s,
    // fades to 20% at 5.3s, keyoff fades to 20% in 5.3s.
    { 655,656,  0,   5333,  5333 }, // 704: qGM29; Overdrive Guitar

    // Amplitude begins at  927.9, peaks 5296.4 at 0.1s,
    // fades to 20% at 3.1s, keyoff fades to 20% in 3.1s.
    { 657,658,  0,   3053,  3053 }, // 705: qGM30; Distorton Guitar

    // Amplitude begins at 2689.5, peaks 2713.1 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 659,660,  0,  40000,     0 }, // 706: qGM31; Guitar Harmonics

    // Amplitude begins at 2624.8, peaks 8922.0 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 661,662,  0,   1520,  1520 }, // 707: qGM32; Acoustic Bass

    // Amplitude begins at 3365.5, peaks 4867.6 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 663,664,  0,   1993,  1993 }, // 708: qGM33; Electric Bass 1

    // Amplitude begins at 2384.1, peaks 2910.0 at 0.7s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    { 665,666,  0,   2153,  2153 }, // 709: qGM34; Electric Bass 2

    // Amplitude begins at  101.2, peaks 5000.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  35,667,  0,  40000,     6 }, // 710: qGM35; Fretless Bass

    // Amplitude begins at 2970.7, peaks 4061.8 at 0.0s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    {  36,668,  0,   2446,  2446 }, // 711: qGM36; Slap Bass 1

    // Amplitude begins at 3523.2, peaks 5695.3 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 669,670,  0,   1886,  1886 }, // 712: qGM37; Slap Bass 2

    // Amplitude begins at 1205.3, peaks 1758.8 at 0.1s,
    // fades to 20% at 2.5s, keyoff fades to 20% in 2.5s.
    { 671,672,  0,   2500,  2500 }, // 713: qGM38; Synth Bass 1

    // Amplitude begins at 1685.3, peaks 2295.6 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 673,674,  0,   1220,  1220 }, // 714: qGM39; Synth Bass 2

    // Amplitude begins at  951.6, peaks 2721.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    {  39,675,  0,  40000,   106 }, // 715: qGM40; Violin

    // Amplitude begins at  956.5, peaks 2756.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 676,675,  0,  40000,   106 }, // 716: qGM41; Viola

    // Amplitude begins at 1099.9, peaks 2643.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 677,678,  0,  40000,   106 }, // 717: qGM42; Cello

    // Amplitude begins at 1823.4, peaks 3093.6 at 5.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 679,680,  0,  40000,    73 }, // 718: qGM43; Contrabass

    // Amplitude begins at 1975.3, peaks 3200.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 681,682,  0,  40000,   193 }, // 719: qGM44; Tremulo Strings

    // Amplitude begins at  207.0, peaks 3891.5 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 683,684,  0,   2000,  2000 }, // 720: qGM45; Pizzicato String

    // Amplitude begins at  207.0, peaks 3923.0 at 0.0s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 683,685,  0,   2000,  2000 }, // 721: qGM46; Orchestral Harp

    // Amplitude begins at 1802.1, peaks 4031.7 at 0.1s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    {  46,686,  0,    833,   833 }, // 722: qGM47; Timpany

    // Amplitude begins at    0.0, peaks  701.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 687,688,  0,  40000,    60 }, // 723: qGM48; String Ensemble1

    // Amplitude begins at    0.0, peaks  502.5 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 689,690,  0,  40000,    60 }, // 724: qGM49; String Ensemble2

    // Amplitude begins at 2323.6, peaks 3680.8 at 0.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 691,692,  0,  40000,  1053 }, // 725: qGM50; Synth Strings 1

    // Amplitude begins at  243.7, peaks 1681.2 at 1.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    {  50,693,  0,  40000,   206 }, // 726: qGM51; SynthStrings 2

    // Amplitude begins at  664.1, peaks 3359.8 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 694,695,  0,  40000,    73 }, // 727: qGM52; Choir Aahs

    // Amplitude begins at  621.8, peaks  941.6 at 0.0s,
    // fades to 20% at 3.6s, keyoff fades to 20% in 3.6s.
    { 696,697,  0,   3646,  3646 }, // 728: qGM53; Voice Oohs

    // Amplitude begins at  527.1, peaks 2564.7 at 9.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 698,699,  0,  40000,   193 }, // 729: qGM54; Synth Voice

    // Amplitude begins at  492.6, peaks  753.4 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 700,701,  0,    446,   446 }, // 730: qGM55; Orchestra Hit

    // Amplitude begins at  535.3, peaks 2440.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  55,702,  0,  40000,    40 }, // 731: qGM56; Trumpet

    // Amplitude begins at  555.3, peaks 2190.6 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 703,704,  0,  40000,    40 }, // 732: qGM57; Trombone

    // Amplitude begins at   43.6, peaks 3911.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 705,706,  0,  40000,    20 }, // 733: qGM58; Tuba

    // Amplitude begins at  312.2, peaks 2472.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 707,708,  0,  40000,     0 }, // 734: qGM59; Muted Trumpet

    // Amplitude begins at    4.9, peaks 4013.5 at 33.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 709,710,  0,  40000,     6 }, // 735: qGM60; French Horn

    // Amplitude begins at    8.0, peaks 1895.6 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 711,712,  0,  40000,    20 }, // 736: qGM61; Brass Section

    // Amplitude begins at  623.8, peaks 1248.5 at 26.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 713,714,  0,  40000,    20 }, // 737: qGM62; Synth Brass 1

    // Amplitude begins at   89.4, peaks 6618.9 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 715,716,  0,  40000,     6 }, // 738: qGM63; Synth Brass 2

    // Amplitude begins at  337.4, peaks 1677.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 717,718,  0,  40000,     6 }, // 739: qGM64; Soprano Sax

    // Amplitude begins at 2680.4, peaks 3836.9 at 38.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 719,720,  0,  40000,   126 }, // 740: qGM65; Alto Sax

    // Amplitude begins at  463.6, peaks 5305.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 721,722,  0,  40000,    40 }, // 741: qGM66; Tenor Sax

    // Amplitude begins at  957.9, peaks 5365.3 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 723,724,  0,  40000,    93 }, // 742: qGM67; Baritone Sax

    // Amplitude begins at 1119.7, peaks 1528.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 725,726,  0,  40000,    46 }, // 743: qGM68; Oboe

    // Amplitude begins at   25.3, peaks 4103.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 727,728,  0,  40000,    73 }, // 744: qGM69; English Horn

    // Amplitude begins at  112.1, peaks 3903.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 729,730,  0,  40000,     0 }, // 745: qGM70; Bassoon

    // Amplitude begins at    3.4, peaks 1731.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 731,732,  0,  40000,    60 }, // 746: qGM71; Clarinet

    // Amplitude begins at  617.1, peaks 2979.6 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 733,734,  0,    400,   400 }, // 747: qGM72; Piccolo

    // Amplitude begins at  604.7, peaks 1152.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 733,735,  0,  40000,     6 }, // 748: qGM73; Flute

    // Amplitude begins at    0.0, peaks 1280.1 at 32.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 736,737,  0,  40000,    33 }, // 749: qGM74; Recorder

    // Amplitude begins at  632.7, peaks 1184.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 738,739,  0,  40000,     6 }, // 750: qGM75; Pan Flute

    // Amplitude begins at   31.5, peaks  820.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 740,741,  0,  40000,    86 }, // 751: qGM76; Bottle Blow

    // Amplitude begins at    2.3, peaks 3151.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 742,743,  0,  40000,    26 }, // 752: qGM77; Shakuhachi

    // Amplitude begins at    0.6, peaks 3576.6 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 744,745,  0,  40000,   193 }, // 753: qGM78; Whistle

    // Amplitude begins at    3.5, peaks 1352.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 746,747,  0,  40000,    53 }, // 754: qGM79; Ocarina

    // Amplitude begins at 3050.7, peaks 4161.3 at 15.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 748,749,  0,  40000,     0 }, // 755: qGM80; Lead 1 squareea

    // Amplitude begins at 2318.4, peaks 3311.5 at 13.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 750,751,  0,  40000,     6 }, // 756: qGM81; Lead 2 sawtooth

    // Amplitude begins at  616.9, peaks  945.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 752,753,  0,  40000,    26 }, // 757: qGM82; Lead 3 calliope

    // Amplitude begins at  392.8, peaks 5203.3 at 0.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 754,755,  0,  40000,   546 }, // 758: qGM83; Lead 4 chiff

    // Amplitude begins at  189.1, peaks 3425.8 at 0.0s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 0.0s.
    { 756,757,  0,   2813,     6 }, // 759: qGM84; Lead 5 charang

    // Amplitude begins at 1900.9, peaks 2235.9 at 38.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 758,759,  0,  40000,   220 }, // 760: qGM85; Lead 6 voice

    // Amplitude begins at 1819.9, peaks 4126.0 at 3.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 760,761,  0,  40000,    46 }, // 761: qGM86; Lead 7 fifths

    // Amplitude begins at 3300.9, peaks 4254.8 at 1.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    {  86,762,  0,  40000,    20 }, // 762: qGM87; Lead 8 brass

    // Amplitude begins at 2408.7, peaks 4961.6 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 763,764,  0,  40000,   106 }, // 763: qGM88; Pad 1 new age

    // Amplitude begins at 2312.6, peaks 6658.2 at 0.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.7s.
    {  88,765,  0,  40000,   733 }, // 764: qGM89; Pad 2 warm

    // Amplitude begins at  874.6, peaks 4344.4 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 766,767,  0,  40000,   266 }, // 765: qGM90; Pad 3 polysynth

    // Amplitude begins at    1.4, peaks 3589.2 at 35.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.6s.
    { 768,769,  0,  40000,  1613 }, // 766: qGM91; Pad 4 choir

    // Amplitude begins at    0.0, peaks 6038.6 at 1.1s,
    // fades to 20% at 4.2s, keyoff fades to 20% in 0.0s.
    { 770,771,  0,   4200,     6 }, // 767: qGM92; Pad 5 bowedpad

    // Amplitude begins at    0.6, peaks 3074.5 at 0.4s,
    // fades to 20% at 2.7s, keyoff fades to 20% in 0.1s.
    { 772,773,  0,   2660,    80 }, // 768: qGM93; Pad 6 metallic

    // Amplitude begins at    0.0, peaks 3056.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 774,775,  0,  40000,   160 }, // 769: qGM94; Pad 7 halo

    // Amplitude begins at 2050.9, peaks 5452.2 at 0.2s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.0s.
    { 776,777,  0,    853,    40 }, // 770: qGM95; Pad 8 sweep

    // Amplitude begins at 1270.0, peaks 3305.2 at 22.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 778,779,  0,  40000,   933 }, // 771: qGM96; FX 1 rain

    // Amplitude begins at    0.0, peaks 2711.4 at 0.6s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.2s.
    { 780,781,  0,    846,   240 }, // 772: qGM97; FX 2 soundtrack

    // Amplitude begins at  904.9, peaks 4456.3 at 0.1s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 782,783,  0,    446,   446 }, // 773: qGM98; FX 3 crystal

    // Amplitude begins at  892.4, peaks 1649.3 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 784,785,  0,  40000,   193 }, // 774: qGM99; FX 4 atmosphere

    // Amplitude begins at 1961.6, peaks 2942.3 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 786,787,  0,   1520,  1520 }, // 775: qGM100; FX 5 brightness

    // Amplitude begins at    0.0, peaks 1884.4 at 2.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 788,789,  0,  40000,   573 }, // 776: qGM101; FX 6 goblins

    // Amplitude begins at  910.1, peaks 4894.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 790,791,  0,  40000,   180 }, // 777: qGM102; FX 7 echoes

    // Amplitude begins at 3545.0, peaks 4825.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 792,793,  0,  40000,   280 }, // 778: qGM103; FX 8 sci-fi

    // Amplitude begins at 2966.4, peaks 4015.4 at 0.3s,
    // fades to 20% at 2.1s, keyoff fades to 20% in 2.1s.
    { 794,795,  0,   2060,  2060 }, // 779: qGM104; Sitar

    // Amplitude begins at 2432.7,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 796,797,  0,    766,   766 }, // 780: qGM105; Banjo

    // Amplitude begins at 2810.8, peaks 3728.2 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 798,799,  0,    853,   853 }, // 781: qGM106; Shamisen

    // Amplitude begins at 4230.8,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 800,801,  0,    706,   706 }, // 782: qGM107; Koto

    // Amplitude begins at 2440.6,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 107,802,  0,    206,   206 }, // 783: qGM108; Kalimba

    // Amplitude begins at    9.6, peaks 3869.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 108,803,  0,  40000,     0 }, // 784: qGM109; Bagpipe

    // Amplitude begins at  902.3, peaks 2558.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 109,804,  0,  40000,   106 }, // 785: qGM110; Fiddle

    // Amplitude begins at 2604.4, peaks 4428.1 at 19.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 805,806,  0,  40000,   180 }, // 786: qGM111; Shanai

    // Amplitude begins at 2977.5,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 111,807,  0,    813,   813 }, // 787: qGM112; Tinkle Bell

    // Amplitude begins at 2744.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 808,809,  0,    126,   126 }, // 788: qGM113; Agogo Bells

    // Amplitude begins at 2624.2, peaks 4406.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 810,811,  0,  40000,   126 }, // 789: qGM114; Steel Drums

    // Amplitude begins at 2351.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 812,813,  0,     13,    13 }, // 790: qGM115; Woodblock

    // Amplitude begins at 2091.1, peaks 2357.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 115,814,  0,    126,   126 }, // 791: qGM116; Taiko Drum

    // Amplitude begins at 2400.6,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 815,816,  0,     80,    80 }, // 792: qGM117; Melodic Tom

    // Amplitude begins at 1038.6, peaks 1098.6 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 817,818,  0,    213,   213 }, // 793: qGM118; Synth Drum

    // Amplitude begins at    0.0, peaks  647.7 at 2.3s,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 819,820,  0,   2333,  2333 }, // 794: qGM119; Reverse Cymbal

    // Amplitude begins at    0.0, peaks  536.5 at 0.1s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 821,822,  0,    286,   286 }, // 795: qGM120; Guitar FretNoise

    // Amplitude begins at    0.0, peaks  492.0 at 0.3s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 823,824,  0,    593,   593 }, // 796: qGM121; Breath Noise

    // Amplitude begins at    0.0, peaks  478.3 at 2.3s,
    // fades to 20% at 4.7s, keyoff fades to 20% in 4.7s.
    { 825,824,  0,   4720,  4720 }, // 797: qGM122; Seashore

    // Amplitude begins at    0.0, peaks 1857.1 at 0.3s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 826,827,  0,    446,   446 }, // 798: qGM123; Bird Tweet

    // Amplitude begins at 1695.1,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 828,829,  0,    153,   153 }, // 799: qGM124; Telephone

    // Amplitude begins at    0.0, peaks 1278.1 at 1.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 830,831,  0,  40000,    80 }, // 800: qGM125; Helicopter

    // Amplitude begins at    0.0, peaks  470.5 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 832,824,  0,  40000,   113 }, // 801: qGM126; Applause/Noise

    // Amplitude begins at  872.0, peaks 1001.5 at 2.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 833,834,  0,  40000,     0 }, // 802: qGM127; Gunshot

    // Amplitude begins at 1410.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 127,127, 16,     40,    40 }, // 803: mGP35; mGP36; qGP35; qGP36; Ac Bass Drum; Bass Drum 1

    // Amplitude begins at 2531.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 835,836, 18,     33,    33 }, // 804: qGP37; Side Stick

    // Amplitude begins at 1853.6, peaks 1993.4 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,837,  0,     66,    66 }, // 805: qGP38; Acoustic Snare

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 838,839,130,      0,     0 }, // 806: qGP39; Hand Clap

    // Amplitude begins at  934.2, peaks 1528.4 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,840,  0,     60,    60 }, // 807: qGP40; Electric Snare

    // Amplitude begins at  449.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 841,842,  1,     33,    33 }, // 808: qGP41; qGP43; qGP45; qGP47; qGP48; qGP50; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom

    // Amplitude begins at  720.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,843, 12,     33,    33 }, // 809: qGP42; Closed High Hat

    // Amplitude begins at    1.7, peaks  918.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 844,845, 12,     66,    66 }, // 810: qGP44; Pedal High Hat

    // Amplitude begins at  641.3,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 134,846, 12,    266,   266 }, // 811: qGP46; Open High Hat

    // Amplitude begins at 1072.2, peaks 1332.9 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 847,848,  1,    193,   193 }, // 812: qGP49; Crash Cymbal 1

    // Amplitude begins at 1632.1,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 849,850, 15,    353,   353 }, // 813: qGP51; qGP59; Ride Cymbal 1; Ride Cymbal 2

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 851,852,129,      0,     0 }, // 814: qGP52; Chinese Cymbal

    // Amplitude begins at 1516.1,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 853,854, 15,    273,   273 }, // 815: qGP53; Ride Bell

    // Amplitude begins at 1433.5, peaks 2663.7 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 855,856,  6,    133,   133 }, // 816: qGP54; Tambourine

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 857,858,143,      0,     0 }, // 817: qGP55; Splash Cymbal

    // Amplitude begins at 1820.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 859,860, 17,      6,     6 }, // 818: qGP56; Cow Bell

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 861,862,129,      0,     0 }, // 819: qGP57; Crash Cymbal 2

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 863,864,128,      0,     0 }, // 820: qGP58; Vibraslap

    // Amplitude begins at 1170.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 865,866,  6,      6,     6 }, // 821: qGP60; High Bongo

    // Amplitude begins at 2056.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 867,868,  1,     13,    13 }, // 822: qGP61; Low Bongo

    // Amplitude begins at 1029.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 145,869,  1,      6,     6 }, // 823: qGP62; Mute High Conga

    // Amplitude begins at  950.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 870,871,  1,      6,     6 }, // 824: qGP63; Open High Conga

    // Amplitude begins at  985.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 872,873,  1,      6,     6 }, // 825: qGP64; Low Conga

    // Amplitude begins at  545.2,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 874,875,  1,     20,    20 }, // 826: qGP65; High Timbale

    // Amplitude begins at 1127.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 149,876,  0,    106,   106 }, // 827: qGP66; Low Timbale

    // Amplitude begins at 1411.8, peaks 1500.1 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 877,878,  3,    106,   106 }, // 828: qGP67; High Agogo

    // Amplitude begins at  969.4, peaks 1144.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 879,880,  3,    140,   140 }, // 829: qGP68; Low Agogo

    // Amplitude begins at    0.4, peaks  489.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 881,882, 15,    106,   106 }, // 830: qGP69; Cabasa

    // Amplitude begins at  275.0, peaks  459.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 883,884, 15,     20,    20 }, // 831: qGP70; Maracas

    // Amplitude begins at   99.7, peaks 1110.3 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 885,886, 87,    326,   326 }, // 832: qGP71; Short Whistle

    // Amplitude begins at  117.2, peaks 1263.9 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 887,888, 87,    446,   446 }, // 833: qGP72; Long Whistle

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 889,890,128,      0,     0 }, // 834: qGP73; Short Guiro

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 889,891,128,      0,     0 }, // 835: qGP74; Long Guiro

    // Amplitude begins at 2006.6, peaks 3559.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 892,893,  6,     53,    53 }, // 836: qGP75; Claves

    // Amplitude begins at 1701.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 159,894,  6,      6,     6 }, // 837: qGP76; qGP77; High Wood Block; Low Wood Block

    // Amplitude begins at    0.0, peaks 2132.1 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 895,896,  1,    146,   146 }, // 838: qGP78; Mute Cuica

    // Amplitude begins at    1.1, peaks 2855.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 897,898,  1,     46,    46 }, // 839: qGP79; Open Cuica

    // Amplitude begins at 1974.0,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 899,900, 10,    386,   386 }, // 840: qGP80; Mute Triangle

    // Amplitude begins at 2089.5,
    // fades to 20% at 2.3s, keyoff fades to 20% in 2.3s.
    { 901,902, 10,   2280,  2280 }, // 841: qGP81; Open Triangle

    // Amplitude begins at    0.4, peaks  465.5 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 903,904, 15,     53,    53 }, // 842: qGP82; Shaker

    // Amplitude begins at 2934.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 905,906,  5,     33,    33 }, // 843: qGP83; Jingle Bell

    // Amplitude begins at  815.5, peaks 1012.6 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 907,908,  3,    220,   220 }, // 844: qGP84; Bell Tree

    // Amplitude begins at 1070.0, peaks 1767.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 909,910,  3,     53,    53 }, // 845: qGP85; Castanets

    // Amplitude begins at 1916.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 911,912,  1,     13,    13 }, // 846: qGP86; Mute Surdu

    // Amplitude begins at 3658.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 913,914,  1,     13,    13 }, // 847: qGP87; Open Surdu

    // Amplitude begins at 2487.8, peaks 3203.1 at 0.1s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 915,915,  0,   1560,  1560 }, // 848: mGM0; AcouGrandPiano

    // Amplitude begins at 1763.6, peaks 2052.1 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 916,916,  0,  40000,     6 }, // 849: mGM3; Honky-tonkPiano

    // Amplitude begins at 2805.0, peaks 2835.6 at 0.0s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 917,917,  0,   1046,  1046 }, // 850: mGM8; Celesta

    // Amplitude begins at 2978.4, peaks 3030.5 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 918,918,  0,   1886,  1886 }, // 851: mGM11; Vibraphone

    // Amplitude begins at 2060.7, peaks 2301.9 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 919,919,  0,   1200,  1200 }, // 852: mGM14; Tubular Bells

    // Amplitude begins at 1677.3, peaks 2151.7 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 920,920,  0,    480,   480 }, // 853: mGM15; Dulcimer

    // Amplitude begins at 2317.1, peaks 2693.5 at 14.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 921,921,  0,  40000,    40 }, // 854: mGM16; Hammond Organ

    // Amplitude begins at 2304.4, peaks 2619.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 922,922,  0,  40000,    20 }, // 855: mGM17; Percussive Organ

    // Amplitude begins at  763.8, peaks 2173.7 at 13.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 923,923,  0,  40000,    20 }, // 856: mGM18; Rock Organ

    // Amplitude begins at  336.5, peaks 2196.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.4s.
    { 924,924,  0,  40000,   413 }, // 857: mGM19; Church Organ

    // Amplitude begins at  154.1, peaks 4544.5 at 29.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 925,925,  0,  40000,   280 }, // 858: mGM20; Reed Organ

    // Amplitude begins at    0.0, peaks 1007.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 926,926,  0,  40000,     6 }, // 859: mGM21; Accordion

    // Amplitude begins at  182.5, peaks 2999.2 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 927,927,  0,  40000,    86 }, // 860: mGM22; Harmonica

    // Amplitude begins at    3.2, peaks 3122.1 at 2.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 928,928,  0,  40000,    73 }, // 861: mGM23; Tango Accordion

    // Amplitude begins at  955.2, peaks 1149.5 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 929,929,  0,   1106,  1106 }, // 862: mGM24; Acoustic Guitar1

    // Amplitude begins at 1425.6, peaks 2713.1 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 930,930,  0,  40000,     0 }, // 863: mGM31; Guitar Harmonics

    // Amplitude begins at 2279.9, peaks 2476.2 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 931,931,  0,   1173,  1173 }, // 864: mGM32; Acoustic Bass

    // Amplitude begins at 1406.5, peaks 2877.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 932,932,  0,  40000,     6 }, // 865: mGM42; Cello

    // Amplitude begins at  845.0, peaks 1921.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 933,933,  0,  40000,    66 }, // 866: mGM48; String Ensemble1

    // Amplitude begins at    0.0, peaks 1568.9 at 0.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 934,934,  0,  40000,    60 }, // 867: mGM49; String Ensemble2

    // Amplitude begins at 2329.2, peaks 4198.0 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 935,935,  0,  40000,   860 }, // 868: mGM50; Synth Strings 1

    // Amplitude begins at    0.0, peaks 1003.3 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 936,936,  0,  40000,   260 }, // 869: mGM51; SynthStrings 2

    // Amplitude begins at  893.3, peaks 3768.9 at 4.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 937,937,  0,  40000,   133 }, // 870: mGM52; Choir Aahs

    // Amplitude begins at    6.6, peaks 2905.2 at 0.3s,
    // fades to 20% at 4.6s, keyoff fades to 20% in 4.6s.
    { 938,938,  0,   4626,  4626 }, // 871: mGM53; Voice Oohs

    // Amplitude begins at   49.0, peaks 3986.6 at 36.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 939,939,  0,  40000,    93 }, // 872: mGM54; Synth Voice

    // Amplitude begins at  896.8, peaks 1548.2 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 940,940,  0,    260,   260 }, // 873: mGM55; Orchestra Hit

    // Amplitude begins at   39.3, peaks 1038.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 941,941,  0,  40000,    20 }, // 874: mGM58; Tuba

    // Amplitude begins at    4.0, peaks 1721.0 at 2.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 942,942,  0,  40000,     0 }, // 875: mGM61; Brass Section

    // Amplitude begins at    7.2, peaks 3023.0 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 943,943,  0,  40000,    20 }, // 876: mGM62; Synth Brass 1

    // Amplitude begins at  674.0, peaks 3305.1 at 38.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 944,944,  0,  40000,   126 }, // 877: mGM64; Soprano Sax

    // Amplitude begins at    3.5, peaks 1691.3 at 38.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 719,719,  0,  40000,    13 }, // 878: mGM65; Alto Sax

    // Amplitude begins at  979.2, peaks 2996.3 at 23.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 945,945,  0,  40000,   106 }, // 879: mGM66; Tenor Sax

    // Amplitude begins at    2.9, peaks 1422.3 at 38.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 946,946,  0,  40000,     0 }, // 880: mGM67; Baritone Sax

    // Amplitude begins at    5.6, peaks 2629.6 at 28.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 947,947,  0,  40000,     6 }, // 881: mGM74; Recorder

    // Amplitude begins at    0.8, peaks 2911.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 948,948,  0,  40000,    46 }, // 882: mGM76; Bottle Blow

    // Amplitude begins at    7.6, peaks 2983.5 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 949,949,  0,  40000,    66 }, // 883: mGM77; Shakuhachi

    // Amplitude begins at    0.0, peaks 2941.2 at 34.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 950,950,  0,  40000,    66 }, // 884: mGM78; Whistle

    // Amplitude begins at    5.0, peaks 3464.3 at 12.6s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 951,951,  0,  40000,    66 }, // 885: mGM79; Ocarina

    // Amplitude begins at  871.5, peaks 6144.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 952,952,  0,  40000,   300 }, // 886: mGM83; Lead 4 chiff

    // Amplitude begins at    0.6, peaks 2279.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 953,953,  0,  40000,    66 }, // 887: mGM85; Lead 6 voice

    // Amplitude begins at  113.4, peaks 1067.6 at 35.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 954,954,  0,  40000,   126 }, // 888: mGM86; Lead 7 fifths

    // Amplitude begins at 1508.5, peaks 3780.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 955,955,  0,  40000,   160 }, // 889: mGM88; Pad 1 new age

    // Amplitude begins at 1578.4, peaks 3276.2 at 10.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.7s.
    { 956,956,  0,  40000,  2686 }, // 890: mGM89; Pad 2 warm

    // Amplitude begins at  638.3, peaks 3827.6 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 957,957,  0,  40000,    73 }, // 891: mGM90; Pad 3 polysynth

    // Amplitude begins at    7.2, peaks 4677.6 at 1.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.4s.
    { 958,958,  0,  40000,  1353 }, // 892: mGM91; Pad 4 choir

    // Amplitude begins at    0.0, peaks 3801.6 at 1.2s,
    // fades to 20% at 4.4s, keyoff fades to 20% in 0.0s.
    { 959,959,  0,   4393,    13 }, // 893: mGM92; Pad 5 bowedpad

    // Amplitude begins at    0.0, peaks 1718.3 at 0.6s,
    // fades to 20% at 2.5s, keyoff fades to 20% in 2.5s.
    { 960,960,  0,   2466,  2466 }, // 894: mGM93; Pad 6 metallic

    // Amplitude begins at    0.0, peaks 2997.7 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 961,961,  0,  40000,   193 }, // 895: mGM94; Pad 7 halo

    // Amplitude begins at 2050.9, peaks 4271.6 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 962,962,  0,  40000,    40 }, // 896: mGM95; Pad 8 sweep

    // Amplitude begins at  852.2, peaks 2458.1 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 963,963,  0,    906,   906 }, // 897: mGM96; FX 1 rain

    // Amplitude begins at  992.1, peaks 1022.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 964,964,  0,  40000,    93 }, // 898: mGM99; FX 4 atmosphere

    // Amplitude begins at 3085.5, peaks 3423.3 at 0.0s,
    // fades to 20% at 2.1s, keyoff fades to 20% in 2.1s.
    { 965,965,  0,   2060,  2060 }, // 899: mGM100; FX 5 brightness

    // Amplitude begins at    0.0, peaks 1743.8 at 2.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 966,966,  0,  40000,   886 }, // 900: mGM101; FX 6 goblins

    // Amplitude begins at    0.0, peaks 2012.9 at 4.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 967,967,  0,  40000,   946 }, // 901: mGM102; FX 7 echoes

    // Amplitude begins at   87.0, peaks 2071.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 968,968,  0,  40000,   506 }, // 902: mGM103; FX 8 sci-fi

    // Amplitude begins at 2863.4,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 969,969,  0,    706,   706 }, // 903: mGM107; Koto

    // Amplitude begins at  879.9,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 128,128,  2,     26,    26 }, // 904: mGP37; Side Stick

    // Amplitude begins at  574.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 129,129,  0,     86,    86 }, // 905: mGP38; mGP40; Acoustic Snare; Electric Snare

    // Amplitude begins at 1832.5,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 130,130,  0,     40,    40 }, // 906: mGP39; Hand Clap

    // Amplitude begins at  376.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 132,132, 12,     33,    33 }, // 907: mGP42; Closed High Hat

    // Amplitude begins at    1.1, peaks  526.8 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 133,133, 12,     60,    60 }, // 908: mGP44; Pedal High Hat

    // Amplitude begins at  398.8,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 134,134, 12,    486,   486 }, // 909: mGP46; Open High Hat

    // Amplitude begins at  133.6, peaks  391.6 at 0.0s,
    // fades to 20% at 2.8s, keyoff fades to 20% in 2.8s.
    { 135,135, 14,   2780,  2780 }, // 910: mGP49; mGP57; oGP49; Crash Cymbal 1; Crash Cymbal 2

    // Amplitude begins at  152.3, peaks  719.7 at 0.1s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    { 137,137, 14,   2566,  2566 }, // 911: mGP52; Chinese Cymbal

    // Amplitude begins at  598.6, peaks  657.2 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 139,139,  2,     53,    53 }, // 912: mGP54; Tambourine

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 970,970,142,      0,     0 }, // 913: mGP55; Splash Cymbal

    // Amplitude begins at  698.6,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 971,971, 64,    220,   220 }, // 914: mGP58; Vibraslap

    // Amplitude begins at    0.2, peaks  374.5 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 152,152, 14,    106,   106 }, // 915: mGP69; Cabasa

    // Amplitude begins at    6.1, peaks  445.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 972,972, 64,     53,    53 }, // 916: mGP73; Short Guiro

    // Amplitude begins at    0.0, peaks  412.6 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 973,973, 64,    166,   166 }, // 917: mGP74; Long Guiro

    // Amplitude begins at  887.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 159,159,  6,      6,     6 }, // 918: mGP76; High Wood Block

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 974,974,129,      0,     0 }, // 919: mGP79; Open Cuica

    // Amplitude begins at 3490.6, peaks 3837.1 at 0.1s,
    // fades to 20% at 3.4s, keyoff fades to 20% in 3.4s.
    { 975,975,  0,   3406,  3406 }, // 920: MGM0; MGM7; AcouGrandPiano; Clavinet

    // Amplitude begins at 2452.8, peaks 3110.0 at 0.1s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 976,976,  0,   2373,  2373 }, // 921: MGM1; MGM3; MGM5; BrightAcouGrand; Chorused Piano; Honky-tonkPiano

    // Amplitude begins at 3856.9, peaks 4825.6 at 0.0s,
    // fades to 20% at 3.0s, keyoff fades to 20% in 3.0s.
    { 977,977,  0,   2960,  2960 }, // 922: MGM2; ElecGrandPiano

    // Amplitude begins at  855.8,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 978,978,  0,    460,   460 }, // 923: MGM4; Rhodes Piano

    // Amplitude begins at 3363.0, peaks 3868.0 at 0.0s,
    // fades to 20% at 3.4s, keyoff fades to 20% in 3.4s.
    { 979,979,  0,   3406,  3406 }, // 924: MGM6; Harpsichord

    // Amplitude begins at 2169.1, peaks 2236.4 at 35.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 980,980,  0,  40000,     0 }, // 925: MGM8; Celesta

    // Amplitude begins at 2240.2, peaks 2483.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 981,981,  0,  40000,   153 }, // 926: MGM10; MGM9; Glockenspiel; Music box

    // Amplitude begins at 2439.6, peaks 3173.6 at 2.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 982,982,  0,  40000,     6 }, // 927: MGM11; oGM11; Vibraphone

    // Amplitude begins at 1872.8, peaks 2510.5 at 34.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 983,983,  0,  40000,   153 }, // 928: MGM12; MGM13; MGM14; Marimba; Tubular Bells; Xylophone

    // Amplitude begins at 2946.0, peaks 3521.3 at 10.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 984,984,  0,  40000,     6 }, // 929: MGM15; Dulcimer

    // Amplitude begins at 1290.2, peaks 1424.7 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 985,985,  0,   1586,  1586 }, // 930: MGM16; MGM17; MGM18; Hammond Organ; Percussive Organ; Rock Organ

    // Amplitude begins at 1803.4, peaks 1912.9 at 0.1s,
    // fades to 20% at 2.0s, keyoff fades to 20% in 2.0s.
    { 986,986,  0,   1973,  1973 }, // 931: MGM19; MGM20; MGM21; Accordion; Church Organ; Reed Organ

    // Amplitude begins at   66.3, peaks 1185.2 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 987,987,  0,    300,   300 }, // 932: MGM22; MGM23; Harmonica; Tango Accordion

    // Amplitude begins at   47.0, peaks 1224.0 at 0.0s,
    // fades to 20% at 4.4s, keyoff fades to 20% in 4.4s.
    { 988,988,  0,   4366,  4366 }, // 933: MGM24; MGM25; MGM26; MGM27; oGM26; Acoustic Guitar1; Acoustic Guitar2; Electric Guitar1; Electric Guitar2

    // Amplitude begins at 3708.6, peaks 4065.1 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 989,989,  0,   1560,  1560 }, // 934: MGM28; MGM29; MGM30; MGM31; MGM44; MGM45; MGM65; MGM66; MGM67; oGM28; oGM29; oGM30; oGM31; oGM44; oGM45; oGM65; oGM66; oGM67; Alto Sax; Baritone Sax; Distorton Guitar; Electric Guitar3; Guitar Harmonics; Overdrive Guitar; Pizzicato String; Tenor Sax; Tremulo Strings

    // Amplitude begins at 1726.9, peaks 2508.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 990,990,  0,  40000,  1073 }, // 935: MGM32; Acoustic Bass

    // Amplitude begins at    0.0, peaks 3087.2 at 0.5s,
    // fades to 20% at 4.9s, keyoff fades to 20% in 4.9s.
    { 991,991,  0,   4940,  4940 }, // 936: MGM125; MGM33; MGM36; Electric Bass 1; Helicopter; Slap Bass 1

    // Amplitude begins at    0.0, peaks 2866.8 at 0.5s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 992,992,  0,   1766,  1766 }, // 937: MGM34; Electric Bass 2

    // Amplitude begins at 2428.0, peaks 3095.4 at 0.0s,
    // fades to 20% at 4.1s, keyoff fades to 20% in 4.1s.
    { 993,993,  0,   4093,  4093 }, // 938: MGM35; Fretless Bass

    // Amplitude begins at 1541.1, peaks 2166.3 at 0.0s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 295,295,  0,   1066,  1066 }, // 939: MGM38; Synth Bass 1

    // Amplitude begins at    2.4, peaks 1006.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 994,994,  0,  40000,     0 }, // 940: MGM39; oGM39; Synth Bass 2

    // Amplitude begins at 3212.7, peaks 3424.1 at 0.0s,
    // fades to 20% at 2.2s, keyoff fades to 20% in 2.2s.
    { 995,995,  0,   2173,  2173 }, // 941: MGM126; MGM40; MGM41; Applause/Noise; Viola; Violin

    // Amplitude begins at  113.7, peaks 3474.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 996,996,  0,  40000,     0 }, // 942: MGM42; oGM42; Cello

    // Amplitude begins at    0.3, peaks 2860.8 at 1.1s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 1.3s.
    { 997,997,  0,   1300,  1300 }, // 943: MGM43; MGM70; MGM71; Bassoon; Clarinet; Contrabass

    // Amplitude begins at  433.0, peaks 2886.3 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 998,998,  0,    186,   186 }, // 944: MGM46; oGM46; Orchestral Harp

    // Amplitude begins at 1717.1, peaks 1784.1 at 0.0s,
    // fades to 20% at 2.6s, keyoff fades to 20% in 2.6s.
    { 999,999,  0,   2580,  2580 }, // 945: MGM47; Timpany

    // Amplitude begins at   73.6, peaks 2454.9 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.1s.
    { 1000,1000,  0,  40000,  1120 }, // 946: MGM48; MGM50; String Ensemble1; Synth Strings 1

    // Amplitude begins at 1287.6, peaks 1447.4 at 0.0s,
    // fades to 20% at 4.4s, keyoff fades to 20% in 4.4s.
    { 1001,1001,  0,   4386,  4386 }, // 947: MGM49; String Ensemble2

    // Amplitude begins at  796.4,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1002,1002,  0,     80,    80 }, // 948: MGM105; MGM51; oGM105; Banjo; SynthStrings 2

    // Amplitude begins at    0.0, peaks  930.5 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.7s.
    { 1003,1003,  0,  40000,   673 }, // 949: MGM52; MGM54; oGM52; oGM54; Choir Aahs; Synth Voice

    // Amplitude begins at    0.0, peaks  785.7 at 0.1s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 1004,1004,  0,    773,   773 }, // 950: MGM53; Voice Oohs

    // Amplitude begins at    0.3, peaks 4488.6 at 0.3s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 0.0s.
    { 1005,1005,  0,   1146,    13 }, // 951: MGM55; Orchestra Hit

    // Amplitude begins at  128.6, peaks 2935.1 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 1006,1006,  0,    620,   620 }, // 952: MGM56; Trumpet

    // Amplitude begins at 2532.5, peaks 3938.3 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 1007,1007,  0,   1466,  1466 }, // 953: MGM57; Trombone

    // Amplitude begins at 1166.3, peaks 1633.6 at 0.1s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 1008,1008,  0,    940,   940 }, // 954: MGM58; Tuba

    // Amplitude begins at 3466.4, peaks 4391.9 at 0.0s,
    // fades to 20% at 0.5s, keyoff fades to 20% in 0.5s.
    { 1009,1009,  0,    526,   526 }, // 955: MGM59; MGM60; French Horn; Muted Trumpet

    // Amplitude begins at  996.0,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 1010,1010,  0,    606,   606 }, // 956: MGM61; Brass Section

    // Amplitude begins at 1747.2, peaks 2389.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1011,1011,  0,  40000,    20 }, // 957: MGM62; Synth Brass 1

    // Amplitude begins at 2699.2, peaks 2820.5 at 0.4s,
    // fades to 20% at 4.9s, keyoff fades to 20% in 4.9s.
    { 1012,1012,  0,   4940,  4940 }, // 958: MGM63; Synth Brass 2

    // Amplitude begins at  874.8, peaks 1363.4 at 0.0s,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 1013,1013,  0,   1866,  1866 }, // 959: MGM64; MGM68; MGM69; oGM68; oGM69; English Horn; Oboe; Soprano Sax

    // Amplitude begins at    6.3, peaks 5038.4 at 0.1s,
    // fades to 20% at 1.0s, keyoff fades to 20% in 1.0s.
    { 1014,1014,  0,    960,   960 }, // 960: MGM72; MGM73; MGM74; MGM75; Flute; Pan Flute; Piccolo; Recorder

    // Amplitude begins at    0.0, peaks  763.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1015,1015,  0,  40000,     6 }, // 961: MGM110; MGM111; MGM76; MGM77; oGM110; oGM111; oGM77; Bottle Blow; Fiddle; Shakuhachi; Shanai

    // Amplitude begins at    6.6, peaks 3450.9 at 0.1s,
    // fades to 20% at 3.9s, keyoff fades to 20% in 3.9s.
    { 1016,1016,  0,   3860,  3860 }, // 962: MGM78; MGM79; MGM80; Lead 1 squareea; Ocarina; Whistle

    // Amplitude begins at    0.0, peaks  777.4 at 0.1s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 1017,1017,  0,   1206,  1206 }, // 963: MGM81; Lead 2 sawtooth

    // Amplitude begins at    1.5, peaks  890.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1018,1018,  0,    346,   346 }, // 964: MGM82; Lead 3 calliope

    // Amplitude begins at  633.4, peaks 1127.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1019,1019,  0,  40000,     0 }, // 965: MGM83; Lead 4 chiff

    // Amplitude begins at  113.3, peaks 3445.0 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1020,1020,  0,  40000,     0 }, // 966: MGM84; MGM85; Lead 5 charang; Lead 6 voice

    // Amplitude begins at 2049.5, peaks 2054.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1021,1021,  0,  40000,     6 }, // 967: MGM86; Lead 7 fifths

    // Amplitude begins at    6.2, peaks 2099.7 at 12.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 1022,1022,  0,  40000,   493 }, // 968: MGM87; oGM87; Lead 8 brass

    // Amplitude begins at  306.6, peaks 1034.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1023,1023,  0,  40000,     0 }, // 969: MGM88; MGM89; Pad 1 new age; Pad 2 warm

    // Amplitude begins at    0.0, peaks  989.5 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1024,1024,  0,  40000,     0 }, // 970: MGM90; Pad 3 polysynth

    // Amplitude begins at  257.8, peaks  893.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1025,1025,  0,  40000,     0 }, // 971: MGM91; Pad 4 choir

    // Amplitude begins at  559.2, peaks 2847.3 at 0.2s,
    // fades to 20% at 1.3s, keyoff fades to 20% in 0.0s.
    { 1026,1026,  0,   1346,     6 }, // 972: MGM92; Pad 5 bowedpad

    // Amplitude begins at    0.0, peaks 1655.3 at 0.6s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 1027,1027,  0,    706,   706 }, // 973: MGM93; Pad 6 metallic

    // Amplitude begins at   43.0, peaks 3082.4 at 0.3s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 1028,1028,  0,   1800,  1800 }, // 974: MGM94; Pad 7 halo

    // Amplitude begins at   33.0, peaks  892.4 at 0.0s,
    // fades to 20% at 4.5s, keyoff fades to 20% in 4.5s.
    { 1029,1029,  0,   4540,  4540 }, // 975: MGM95; oGM95; Pad 8 sweep

    // Amplitude begins at   40.6, peaks  870.6 at 0.1s,
    // fades to 20% at 7.1s, keyoff fades to 20% in 7.1s.
    { 1030,1030,  0,   7100,  7100 }, // 976: MGM96; oGM96; FX 1 rain

    // Amplitude begins at 3351.9, peaks 3641.0 at 0.0s,
    // fades to 20% at 1.6s, keyoff fades to 20% in 1.6s.
    { 1031,1031,  0,   1606,  1606 }, // 977: MGM97; FX 2 soundtrack

    // Amplitude begins at 1221.6,
    // fades to 20% at 1.1s, keyoff fades to 20% in 1.1s.
    { 1032,1032,  0,   1080,  1080 }, // 978: MGM98; FX 3 crystal

    // Amplitude begins at  969.7,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1033,1033,  0,    286,   286 }, // 979: MGM104; MGM99; oGM104; oGM99; FX 4 atmosphere; Sitar

    // Amplitude begins at 2290.2,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.4s.
    { 1034,1034,  0,  40000,  2433 }, // 980: MGM100; MGM101; MGM102; oGM100; oGM101; oGM102; FX 5 brightness; FX 6 goblins; FX 7 echoes

    // Amplitude begins at 2511.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 298,298,  0,     46,    46 }, // 981: MGM103; oGM103; FX 8 sci-fi

    // Amplitude begins at   12.4, peaks  523.1 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1035,1035,  0,  40000,     0 }, // 982: MGM106; oGM106; Shamisen

    // Amplitude begins at    1.8, peaks 1939.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1036,1036,  0,  40000,    46 }, // 983: MGM107; MGM108; MGM109; oGM108; oGM109; Bagpipe; Kalimba; Koto

    // Amplitude begins at 2505.8,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1037,1037,  0,     40,    40 }, // 984: MGM112; Tinkle Bell

    // Amplitude begins at  968.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 269,269,  0,     13,    13 }, // 985: MGM113; Agogo Bells

    // Amplitude begins at  826.3,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 275,275,  0,    153,   153 }, // 986: MGM114; MGM117; MGM118; oGM118; Melodic Tom; Steel Drums; Synth Drum

    // Amplitude begins at  748.3,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 276,276,  0,     40,    40 }, // 987: MGM115; MGM116; Taiko Drum; Woodblock

    // Amplitude begins at  191.7, peaks  439.6 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1038,1038,  0,     86,    86 }, // 988: MGM119; Reverse Cymbal

    // Amplitude begins at 1809.4,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1039,1039,  0,     40,    40 }, // 989: MGM120; Guitar FretNoise

    // Amplitude begins at 2887.7, peaks 2954.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1040,1040,  0,  40000,     0 }, // 990: MGM121; Breath Noise

    // Amplitude begins at    0.8, peaks 3183.7 at 0.1s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 1041,1041,  0,    626,   626 }, // 991: MGM122; Seashore

    // Amplitude begins at 1541.1, peaks 2190.1 at 0.0s,
    // fades to 20% at 1.8s, keyoff fades to 20% in 1.8s.
    { 1042,1042,  0,   1833,  1833 }, // 992: MGM123; oGM123; Bird Tweet

    // Amplitude begins at    0.0, peaks  808.9 at 0.1s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1043,1043,  0,    146,   146 }, // 993: MGM124; Telephone

    // Amplitude begins at  810.9,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1044,1044,  0,     80,    80 }, // 994: MGM127; oGM127; Gunshot

    // Amplitude begins at 1750.0, peaks 2606.4 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1045,1045,  0,     40,    40 }, // 995: MGP35; MGP36; Ac Bass Drum; Bass Drum 1

    // Amplitude begins at 1031.7,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 269,269,  1,      6,     6 }, // 996: MGP37; MGP41; MGP43; MGP45; MGP47; MGP48; MGP50; MGP56; MGP60; MGP61; MGP62; MGP63; MGP64; MGP65; MGP66; Cow Bell; High Bongo; High Floor Tom; High Timbale; High Tom; High-Mid Tom; Low Bongo; Low Conga; Low Floor Tom; Low Timbale; Low Tom; Low-Mid Tom; Mute High Conga; Open High Conga; Side Stick

    // Amplitude begins at  825.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1046,1046, 46,    133,   133 }, // 997: MGP38; MGP39; MGP40; MGP67; MGP68; Acoustic Snare; Electric Snare; Hand Clap; High Agogo; Low Agogo

    // Amplitude begins at  293.8, peaks  397.8 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1047,1047,100,     40,    40 }, // 998: MGP42; Closed High Hat

    // Amplitude begins at  295.7, peaks  429.0 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1048,1048,100,     66,    66 }, // 999: MGP44; MGP46; MGP51; MGP54; MGP69; MGP70; MGP71; MGP72; MGP73; MGP75; Cabasa; Claves; Long Whistle; Maracas; Open High Hat; Pedal High Hat; Ride Cymbal 1; Short Guiro; Short Whistle; Tambourine

    // Amplitude begins at  333.2, peaks  425.9 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1049,1049,100,    126,   126 }, // 1000: MGP49; Crash Cymbal 1

    // Amplitude begins at 4040.9,
    // fades to 20% at 3.0s, keyoff fades to 20% in 3.0s.
    { 1050,1050,  0,   3006,  3006 }, // 1001: oGM0; oGM1; oGM2; AcouGrandPiano; BrightAcouGrand; ElecGrandPiano

    // Amplitude begins at    0.0, peaks  895.7 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1051,1051,  0,  40000,    60 }, // 1002: oGM3; Honky-tonkPiano

    // Amplitude begins at    0.0, peaks 4135.7 at 2.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.2s.
    { 1052,1052,  0,  40000,  2200 }, // 1003: oGM4; Rhodes Piano

    // Amplitude begins at 3070.8, peaks 4485.8 at 0.0s,
    // fades to 20% at 3.2s, keyoff fades to 20% in 3.2s.
    { 1053,1053,  0,   3166,  3166 }, // 1004: oGM5; Chorused Piano

    // Amplitude begins at 1325.7,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1054,1054,  0,    126,   126 }, // 1005: oGM6; Harpsichord

    // Amplitude begins at    5.3, peaks 2690.6 at 32.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1055,1055,  0,  40000,     0 }, // 1006: oGM7; Clavinet

    // Amplitude begins at 2792.5, peaks 3282.2 at 32.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1056,1056,  0,  40000,    26 }, // 1007: oGM8; Celesta

    // Amplitude begins at 1160.1, peaks 1268.1 at 16.8s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1057,1057,  0,  40000,    73 }, // 1008: oGM9; Glockenspiel

    // Amplitude begins at 2922.9, peaks 3164.1 at 4.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 1058,1058,  0,  40000,   553 }, // 1009: oGM10; Music box

    // Amplitude begins at    0.0, peaks 2138.9 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.5s.
    { 1059,1059,  0,  40000,   473 }, // 1010: oGM12; Marimba

    // Amplitude begins at    0.0, peaks 2526.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 1060,1060,  0,  40000,   266 }, // 1011: oGM13; Xylophone

    // Amplitude begins at 2887.9, peaks 2895.3 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 1061,1061,  0,    906,   906 }, // 1012: oGM14; Tubular Bells

    // Amplitude begins at 2472.7, peaks 3115.9 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 1062,1062,  0,   1153,  1153 }, // 1013: oGM15; Dulcimer

    // Amplitude begins at 1406.2,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 1063,1063,  0,   1200,  1200 }, // 1014: oGM16; oGM17; oGM18; Hammond Organ; Percussive Organ; Rock Organ

    // Amplitude begins at 2459.3, peaks 4228.8 at 0.0s,
    // fades to 20% at 0.8s, keyoff fades to 20% in 0.8s.
    { 1064,1064,  0,    833,   833 }, // 1015: oGM19; oGM20; oGM21; Accordion; Church Organ; Reed Organ

    // Amplitude begins at 3987.9,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 1065,1065,  0,   1913,  1913 }, // 1016: oGM22; Harmonica

    // Amplitude begins at 3989.7,
    // fades to 20% at 1.9s, keyoff fades to 20% in 1.9s.
    { 1066,1066,  0,   1913,  1913 }, // 1017: oGM23; Tango Accordion

    // Amplitude begins at 1111.7, peaks 1175.7 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1067,1067,  0,  40000,     0 }, // 1018: oGM24; Acoustic Guitar1

    // Amplitude begins at   61.0, peaks 2410.2 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1068,1068,  0,  40000,     0 }, // 1019: oGM25; Acoustic Guitar2

    // Amplitude begins at  257.8, peaks 4199.6 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.8s.
    { 1069,1069,  0,  40000,   793 }, // 1020: oGM27; Electric Guitar2

    // Amplitude begins at   62.2, peaks 1400.8 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.2s.
    { 1070,1070,  0,  40000,   200 }, // 1021: oGM32; Acoustic Bass

    // Amplitude begins at    0.0, peaks 5876.2 at 39.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.6s.
    { 1071,1071,  0,  40000,   580 }, // 1022: oGM33; Electric Bass 1

    // Amplitude begins at    0.0, peaks 4171.5 at 20.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.7s.
    { 1072,1072,  0,  40000,   746 }, // 1023: oGM34; Electric Bass 2

    // Amplitude begins at    1.9, peaks  387.0 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 1073,1073,  0,    640,   640 }, // 1024: oGM35; Fretless Bass

    // Amplitude begins at    0.0, peaks 2349.8 at 1.1s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 1074,1074,  0,   2406,  2406 }, // 1025: oGM36; Slap Bass 1

    // Amplitude begins at 3212.7, peaks 3495.3 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 1075,1075,  0,   1160,  1160 }, // 1026: oGM38; Synth Bass 1

    // Amplitude begins at  122.7, peaks 2711.9 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 2.0s.
    { 1076,1076,  0,  40000,  2013 }, // 1027: oGM40; Violin

    // Amplitude begins at 2612.1, peaks 2672.8 at 0.5s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1077,1077,  0,  40000,     0 }, // 1028: oGM41; Viola

    // Amplitude begins at 1332.8, peaks 1638.2 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.9s.
    { 1078,1078,  0,    933,   933 }, // 1029: oGM43; Contrabass

    // Amplitude begins at 2354.7, peaks 2459.2 at 0.9s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1079,1079,  0,  40000,     0 }, // 1030: oGM47; Timpany

    // Amplitude begins at    0.4, peaks 2615.3 at 0.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1080,1080,  0,  40000,   146 }, // 1031: oGM48; String Ensemble1

    // Amplitude begins at    0.6, peaks 1949.8 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1081,1081,  0,  40000,   126 }, // 1032: oGM49; String Ensemble2

    // Amplitude begins at 1574.4, peaks 4048.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1082,1082,  0,  40000,   106 }, // 1033: oGM50; Synth Strings 1

    // Amplitude begins at 2765.2, peaks 2941.3 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1083,1083,  0,    313,   313 }, // 1034: oGM51; SynthStrings 2

    // Amplitude begins at    0.0, peaks  800.4 at 0.1s,
    // fades to 20% at 0.7s, keyoff fades to 20% in 0.7s.
    { 1084,1084,  0,    660,   660 }, // 1035: oGM53; Voice Oohs

    // Amplitude begins at    0.3, peaks 4368.1 at 0.3s,
    // fades to 20% at 1.1s, keyoff fades to 20% in 0.0s.
    { 1085,1085,  0,   1086,    13 }, // 1036: oGM55; Orchestra Hit

    // Amplitude begins at   84.8, peaks 2743.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1086,1086,  0,  40000,   133 }, // 1037: oGM56; Trumpet

    // Amplitude begins at 1246.0, peaks 1409.7 at 0.0s,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 1087,1087,  0,   2373,  2373 }, // 1038: oGM59; Muted Trumpet

    // Amplitude begins at 2607.3, peaks 2633.3 at 0.2s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1088,1088,  0,  40000,     0 }, // 1039: oGM60; French Horn

    // Amplitude begins at 2328.2, peaks 2462.5 at 28.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1089,1089,  0,  40000,     0 }, // 1040: oGM61; Brass Section

    // Amplitude begins at  573.8, peaks 1746.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1090,1090,  0,  40000,    13 }, // 1041: oGM62; Synth Brass 1

    // Amplitude begins at    0.4, peaks 1445.8 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 1091,1091,  0,    186,   186 }, // 1042: oGM63; Synth Brass 2

    // Amplitude begins at 2497.6, peaks 4330.5 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1092,1092,  0,  40000,     0 }, // 1043: oGM64; Soprano Sax

    // Amplitude begins at 1037.1, peaks 4206.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1093,1093,  0,  40000,     0 }, // 1044: oGM70; oGM71; Bassoon; Clarinet

    // Amplitude begins at    0.5, peaks 2356.5 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 1094,1094,  0,    220,   220 }, // 1045: oGM72; Piccolo

    // Amplitude begins at    6.1, peaks 2349.6 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.0s.
    { 1095,1095,  0,    273,     6 }, // 1046: oGM73; oGM74; Flute; Recorder

    // Amplitude begins at    5.3, peaks 2451.4 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1096,1096,  0,  40000,     6 }, // 1047: oGM75; Pan Flute

    // Amplitude begins at   13.1, peaks 5716.4 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1097,1097,  0,  40000,     0 }, // 1048: oGM76; Bottle Blow

    // Amplitude begins at  894.7, peaks 2825.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1098,1098,  0,  40000,     0 }, // 1049: oGM78; Whistle

    // Amplitude begins at    6.1, peaks 2314.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 1099,1099,  0,  40000,   286 }, // 1050: oGM79; Ocarina

    // Amplitude begins at 1391.8, peaks 1475.9 at 0.0s,
    // fades to 20% at 1.2s, keyoff fades to 20% in 1.2s.
    { 1100,1100,  0,   1213,  1213 }, // 1051: oGM80; Lead 1 squareea

    // Amplitude begins at  782.2, peaks 2219.2 at 0.0s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.3s.
    { 1101,1101,  0,  40000,   286 }, // 1052: oGM81; Lead 2 sawtooth

    // Amplitude begins at  136.9, peaks 3849.3 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 1102,1102,  0,  40000,   886 }, // 1053: oGM82; Lead 3 calliope

    // Amplitude begins at 1846.7, peaks 3840.6 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.9s.
    { 1103,1103,  0,  40000,   886 }, // 1054: oGM83; Lead 4 chiff

    // Amplitude begins at    0.8, peaks 3267.0 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.3s.
    { 1104,1104,  0,  40000,  1280 }, // 1055: oGM84; Lead 5 charang

    // Amplitude begins at   21.7, peaks 2598.8 at 5.4s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1105,1105,  0,  40000,    20 }, // 1056: oGM85; Lead 6 voice

    // Amplitude begins at 1170.1, peaks 2329.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1106,1106,  0,  40000,     6 }, // 1057: oGM86; Lead 7 fifths

    // Amplitude begins at    2.2, peaks  886.1 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1107,1107,  0,  40000,     0 }, // 1058: oGM88; oGM89; Pad 1 new age; Pad 2 warm

    // Amplitude begins at    2.6, peaks 1053.7 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1108,1108,  0,  40000,     0 }, // 1059: oGM90; oGM91; Pad 3 polysynth; Pad 4 choir

    // Amplitude begins at    6.0, peaks 2470.4 at 6.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1109,1109,  0,  40000,     6 }, // 1060: oGM92; oGM93; Pad 5 bowedpad; Pad 6 metallic

    // Amplitude begins at  336.9, peaks 3387.9 at 0.1s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 1110,1110,  0,    166,   166 }, // 1061: oGM94; Pad 7 halo

    // Amplitude begins at 2371.9, peaks 2431.7 at 0.0s,
    // fades to 20% at 0.9s, keyoff fades to 20% in 0.0s.
    { 1111,1111,  0,    926,     6 }, // 1062: oGM97; oGM98; FX 2 soundtrack; FX 3 crystal

    // Amplitude begins at    0.5, peaks 2356.5 at 0.1s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.0s.
    { 1112,1112,  0,  40000,     6 }, // 1063: oGM107; Koto

    // Amplitude begins at 1533.4, peaks 1569.9 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1113,1113,  0,    253,   253 }, // 1064: oGM112; Tinkle Bell

    // Amplitude begins at 1334.5,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 351,351,  0,     66,    66 }, // 1065: oGM113; Agogo Bells

    // Amplitude begins at  424.6,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1114,1114,  0,     46,    46 }, // 1066: oGM114; Steel Drums

    // Amplitude begins at  107.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1115,1115,  0,     33,    33 }, // 1067: oGM115; Woodblock

    // Amplitude begins at 2758.0, peaks 2814.3 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1116,1116,  0,    313,   313 }, // 1068: oGM116; oGM119; Reverse Cymbal; Taiko Drum

    // Amplitude begins at 2664.9, peaks 3050.5 at 0.0s,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1117,1117,  0,     40,    40 }, // 1069: oGM117; oGM120; oGP37; oGP39; oGP41; oGP43; oGP45; oGP47; oGP48; oGP50; Guitar FretNoise; Hand Clap; High Floor Tom; High Tom; High-Mid Tom; Low Floor Tom; Low Tom; Low-Mid Tom; Melodic Tom; Side Stick

    // Amplitude begins at  483.6,
    // fades to 20% at 2.4s, keyoff fades to 20% in 2.4s.
    { 1118,1118,  0,   2373,  2373 }, // 1070: oGM121; Breath Noise

    // Amplitude begins at    3.1, peaks 1168.0 at 0.0s,
    // fades to 20% at 0.2s, keyoff fades to 20% in 0.2s.
    { 1119,1119,  0,    180,   180 }, // 1071: oGM122; Seashore

    // Amplitude begins at   62.2, peaks 1394.8 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1120,1120,  0,    346,   346 }, // 1072: oGM124; Telephone

    // Amplitude begins at    0.8, peaks 2609.7 at 0.3s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 1.6s.
    { 1121,1121,  0,  40000,  1560 }, // 1073: oGM125; Helicopter

    // Amplitude begins at  885.6, peaks 2615.3 at 0.7s,
    // fades to 20% at 40.0s, keyoff fades to 20% in 0.1s.
    { 1122,1122,  0,  40000,   146 }, // 1074: oGM126; Applause/Noise

    // Amplitude begins at 1184.0, peaks 1368.3 at 0.0s,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 351,351, 17,     53,    53 }, // 1075: oGP35; oGP36; Ac Bass Drum; Bass Drum 1

    // Amplitude begins at  433.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1114,1114, 14,     60,    60 }, // 1076: oGP38; oGP40; Acoustic Snare; Electric Snare

    // Amplitude begins at  153.3,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1115,1115, 12,     66,    66 }, // 1077: oGP42; Closed High Hat

    // Amplitude begins at  133.6, peaks  391.6 at 0.0s,
    // fades to 20% at 0.4s, keyoff fades to 20% in 0.4s.
    { 1123,1123, 14,    393,   393 }, // 1078: oGP44; Pedal High Hat

    // Amplitude begins at  133.6, peaks  399.8 at 0.0s,
    // fades to 20% at 1.5s, keyoff fades to 20% in 1.5s.
    { 1124,1124, 14,   1473,  1473 }, // 1079: oGP46; Open High Hat

    // Amplitude begins at 2203.5, peaks 2773.5 at 0.0s,
    // fades to 20% at 0.3s, keyoff fades to 20% in 0.3s.
    { 1116,1116,  6,    346,   346 }, // 1080: oGP51; Ride Cymbal 1

    // Amplitude begins at    0.0,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1125,1125,158,      0,     0 }, // 1081: oGP54; Tambourine

    // Amplitude begins at 1401.1,
    // fades to 20% at 0.0s, keyoff fades to 20% in 0.0s.
    { 1126,1126,  1,      6,     6 }, // 1082: oGP60; High Bongo

    // Amplitude begins at 3020.7, peaks 3310.6 at 0.0s,
    // fades to 20% at 0.6s, keyoff fades to 20% in 0.6s.
    { 1127,1127,  1,    626,   626 }, // 1083: oGP62; oGP63; oGP64; Low Conga; Mute High Conga; Open High Conga

    // Amplitude begins at  356.8,
    // fades to 20% at 0.1s, keyoff fades to 20% in 0.1s.
    { 1128,1128, 14,     60,    60 }, // 1084: oGP69; oGP70; Cabasa; Maracas

};
const unsigned short banks[12][256] =
{
    { // bank 0, Miles SC3
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 33, 39, 40, 41, 42, 43, 44, 45, 46,
 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,
111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,127,127,128,129,130,131,132,133,134,135,136,137,138,
139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,
155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,
171,172,173,174,175,176,177,178,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 1, HMI GM
179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,
 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,195, 28,179, 30, 31,
 32, 33, 34, 35, 36,196, 38, 33, 39, 40, 41, 42, 43, 44, 45, 46,
 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,197, 62,
 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,
111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,199,200,201,202,203,
204,205,206,207,208,209,210,211,212,213,214,215,214,216,217,218,
219,220,221,222,220,222,223,220,224,220,225,226,227,228,229,230,
231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,
247,248,236,249,250,251,209,252,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 2, HMI int
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,268,269,270,271,272,
273,274,275,276,119,277,278,279,280,281,282,283,284,285,286,287,
288,289,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,290,291,292,293,294,
295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,
311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,
327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,
343,344,345,346,347,348,349,350,351,352,353,354,355,356,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 3, HMI ham
253,254,255, 28,256, 31, 30,257,258,259,260,261,262, 38, 46,263,
 79, 84,264,265,253,253,253,253,253,253,253,268,269,270,271,272,
273,274,275,276,119,277,278,279,280,281,282,283,284,285,286,287,
288,253,112, 99,357,358, 93,359,360,107,116,361, 28, 78,362,363,
364, 79, 94, 38, 33,115,365,366,367,368,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,198,
375,132,134,136,138,139,141,173,156,376,377,127,378,379,380,381,
382,383,384,143,198,198,198,198,198,198,198,290,291,292,293,294,
295,296,297,298,299,300,301,302,303,385,386,387,388,308,309,310,
311,312,313,314,389,316,317,318,319,320,321,322,390,324,325,326,
327,328,329,391,392,332,333,334,335,393,394,338,339,340,341,342,
343,344,345,346,347,348,349,350,395,352,396,354,355,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 4, HMI rick
253,253,253,253,253,253,253,253,253,253,253,253,253, 38, 46,263,
 79, 84,264,265, 49, 89, 92, 93,105,266,267,268,269,270,271,272,
273,274,275,276,119,277,278,279,280,281,282,283,284,285,286,287,
288,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,397,398,399,400,
401, 34,402,403,257, 51, 52, 84,404,405,406,407,408, 85,357, 90,
 93, 94,101,409,114,119,253,253,253,253,253,253,253,253,253,253,
253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,253,
198,198,198,198,198,198,198,198,198,198,198,198,198,410,127,378,
380,411,412,411,412,413,414,415,416,417,418,290,291,292,293,294,
295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,
311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,
327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,
343,344,345,346,347,348,349,350,351,352,353,354,355,419,420,383,
164,421,156,422,377,423,424,425,426,178,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 5, Doom
427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,
443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,
459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,
475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,
491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,
507,508,509,510,511,512,513,514,514,515,516,517,518,519,520,521,
522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,
538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,554,555,556,557,558,559,560,561,562,563,564,565,566,
567,568,569,570,571,572,573,574,575,568,576,570,577,578,579,580,
581,582,583,584,585,563,586,587,587,587,587,588,589,590,591,587,
592,593,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 6, Hexen
427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,
443,444,445,446,447,448,449,450,451,452,453,454,455,594,595,458,
596,460,461,597,598,464,465,466,467,468,469,470,471,472,473,474,
475,476,477,478,479,480,481,482,483,484,599,486,487,488,489,490,
491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,
507,508,509,510,511,512,513,514,514,515,516,517,518,519,520,521,
522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,
538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,554,555,556,557,558,600,560,561,562,563,564,565,566,
567,568,569,570,571,572,573,601,575,568,576,570,577,578,579,580,
581,582,583,584,585,563,586,587,587,587,587,588,589,590,591,587,
592,593,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 7, Miles Warcraft
  0,  1,  2,  3,  4,  5,602,  7,  8,603, 10, 11, 12, 13,604, 15,
 16, 17, 18,605, 20, 21, 22, 23,606, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 33, 39, 40, 41, 42,607,608,609,610,
611,612,613, 50,614, 52, 53,615,616,617,618, 58,619,620, 61, 62,
 63, 64, 65, 66,621, 68,622,623,624,625, 73, 74, 75, 76, 77, 78,
 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93,626,
 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,
111,112,113,114,627,116,628,629,119,120,121,122,123,124,125,126,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,630,630,631,632,633,632,634,635,634,636,634,637,634,
634,638,634,639,640,641,642,643,644,645,646,639,647,648,649,650,
651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,
667,668,669,670,671,672,673,674,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 8, Miles QUAD-OP
675,676,677,678,679,680,681,682,683,684,685,686,687,688,689,690,
691,692,693,694,695,696,697,698,699,700,701,702,703,704,705,706,
707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,
723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,
739,740,741,742,743,744,745,746,747,748,749,750,751,752,753,754,
755,756,757,758,759,760,761,762,763,764,765,766,767,768,769,770,
771,772,773,774,775,776,777,778,779,780,781,782,783,784,785,786,
787,788,789,790,791,792,793,794,795,796,797,798,799,800,801,802,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,803,803,804,805,806,807,808,809,808,810,808,811,808,
808,812,808,813,814,815,816,817,818,819,820,813,821,822,823,824,
825,826,827,828,829,830,831,832,833,834,835,836,837,837,838,839,
840,841,842,843,844,845,846,847,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 9, Miles Simfarm
848,180,181,849,183,184,185,186,850,188,189,851,191,192,852,853,
854,855,856,857,858,859,860,861,862, 25, 26,195, 28,179, 30,863,
864, 33, 34, 35, 36,196, 38, 33, 39, 40,865, 42, 43, 44, 45, 46,
866,867,868,869,870,871,872,873, 55, 56,874, 58, 59,875,876, 62,
877,878,879,880, 67, 68, 69, 70, 71, 72,881, 74,882,883,884,885,
 79, 80, 81,886, 83,887,888, 86,889,890,891,892,893,894,895,896,
897, 96, 97,898,899,900,901,902,103,104,105,903,107,108,109,110,
111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,803,803,904,905,906,905,674,907,674,908,674,909,674,
674,910,674,639,911,641,912,913,644,910,914,639,647,648,649,650,
651,652,653,654,655,915,657,658,659,916,917,662,918,664,665,919,
667,668,669,670,671,672,673,674,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 10, Miles 2.14 OSS
920,921,922,921,923,921,924,920,925,926,926,927,928,928,928,929,
930,930,930,931,931,931,932,932,933,933,933,933,934,934,934,934,
935,936,937,938,936,361,939,940,941,941,942,943,934,934,944,945,
946,947,946,948,949,950,949,951,952,953,954,955,955,956,957,958,
959,934,934,934,959,959,943,943,960,960,960,960,961,961,962,962,
962,963,964,965,966,966,967,968,969,969,970,971,972,973,974,975,
976,977,978,979,980,980,980,981,979,948,982,983,983,983,961,961,
984,985,986,987,987,986,986,988,989,990,991,992,993,936,941,994,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,995,995,996,997,997,997,996,998,996,999,996,999,996,
996,1000,996,999,198,198,999,198,996,198,198,198,996,996,996,996,
996,996,996,997,997,999,999,999,999,999,198,999,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
    { // bank 11, Miles UW
1001,1001,1001,1002,1003,1004,1005,1006,1007,1008,1009,927,1010,1011,1012,1013,
1014,1014,1014,1015,1015,1015,1016,1017,1018,1019,933,1020,934,934,934,934,
1021,1022,1023,1024,1025,361,1026,940,1027,1028,942,1029,934,934,944,1030,
1031,1032,1033,1034,949,1035,949,1036,1037, 45, 45,1038,1039,1040,1041,1042,
1043,934,934,934,959,959,1044,1044,1045,1046,1046,1047,1048,961,1049,1050,
1051,1052,1053,1054,1055,1056,1057,968,1058,1058,1059,1059,1060,1060,1061,975,
976,1062,1062,979,980,980,980,981,979,948,982,1063,983,983,961,961,
1064,1065,1066,1067,1068,1069,986,1068,1069,1070,1071,992,1072,1073,1074,994,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,1075,1075,1069,1076,1069,1076,1069,1077,1069,1078,1069,1079,1069,
1069,910,1069,1080,198,198,1081,198,198,198,198,198,1082,648,1083,1083,
1083,198,198,198,198,1084,1084,198,198,198,198,662,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,198,
    },
};
