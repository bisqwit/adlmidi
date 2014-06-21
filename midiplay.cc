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

#include <assert.h>

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
# include <termio.h>
# include <fcntl.h>
# include <sys/ioctl.h>
#endif

#include <deque>
#include <algorithm>

#include <signal.h>

#include "fraction"

#ifndef __DJGPP__
#include "dbopl.h"

static const unsigned long PCM_RATE = 48000;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
#else // DJGPP
static const unsigned MaxCards = 1;
static const unsigned OPLBase = 0x388;
#endif
static unsigned AdlBank    = 0;
static unsigned NumFourOps = 7;
static unsigned NumCards   = 2;
static bool HighTremoloMode   = false;
static bool HighVibratoMode   = false;
static bool AdlPercussionMode = false;
static bool QuitFlag = false, FakeDOSshell = false;
static unsigned SkipForward = 0;
static bool DoingInstrumentTesting = false;
static bool QuitWithoutLooping = false;
static bool WritePCMfile = false;
static bool ScaleModulators = false;

static unsigned WinHeight()
{
    if(AdlPercussionMode)
        return std::min(2u, NumCards) * 23;
    return std::min(3u, NumCards) * 18;
}

#include "adldata.hh"

static const unsigned short Operators[23*2] =
    {0x000,0x003,0x001,0x004,0x002,0x005, // operators  0, 3,  1, 4,  2, 5
     0x008,0x00B,0x009,0x00C,0x00A,0x00D, // operators  6, 9,  7,10,  8,11
     0x010,0x013,0x011,0x014,0x012,0x015, // operators 12,15, 13,16, 14,17
     0x100,0x103,0x101,0x104,0x102,0x105, // operators 18,21, 19,22, 20,23
     0x108,0x10B,0x109,0x10C,0x10A,0x10D, // operators 24,27, 25,28, 26,29
     0x110,0x113,0x111,0x114,0x112,0x115, // operators 30,33, 31,34, 32,35
     0x010,0x013,   // operators 12,15
     0x014,0xFFF,   // operator 16
     0x012,0xFFF,   // operator 14
     0x015,0xFFF,   // operator 17
     0x011,0xFFF }; // operator 13

static const unsigned short Channels[23] =
    {0x000,0x001,0x002, 0x003,0x004,0x005, 0x006,0x007,0x008, // 0..8
     0x100,0x101,0x102, 0x103,0x104,0x105, 0x106,0x107,0x108, // 9..17 (secondary set)
     0x006,0x007,0x008,0xFFF,0xFFF }; // <- hw percussions, 0xFFF = no support for pitch/pan

/*
    In OPL3 mode:
         0    1    2    6    7    8     9   10   11    16   17   18
       op0  op1  op2 op12 op13 op14  op18 op19 op20  op30 op31 op32
       op3  op4  op5 op15 op16 op17  op21 op22 op23  op33 op34 op35
         3    4    5                   13   14   15
       op6  op7  op8                 op24 op25 op26
       op9 op10 op11                 op27 op28 op29
    Ports:
        +0   +1   +2  +10  +11  +12  +100 +101 +102  +110 +111 +112
        +3   +4   +5  +13  +14  +15  +103 +104 +105  +113 +114 +115
        +8   +9   +A                 +108 +109 +10A
        +B   +C   +D                 +10B +10C +10D

    Percussion:
      bassdrum = op(0): 0xBD bit 0x10, operators 12 (0x10) and 15 (0x13) / channels 6, 6b
      snare    = op(3): 0xBD bit 0x08, operators 16 (0x14)               / channels 7b
      tomtom   = op(4): 0xBD bit 0x04, operators 14 (0x12)               / channels 8
      cym      = op(5): 0xBD bit 0x02, operators 17 (0x17)               / channels 8b
      hihat    = op(2): 0xBD bit 0x01, operators 13 (0x11)               / channels 7


    In OPTi mode ("extended FM" in 82C924, 82C925, 82C931 chips):
         0   1   2    3    4    5    6    7     8    9   10   11   12   13   14   15   16   17
       op0 op4 op6 op10 op12 op16 op18 op22  op24 op28 op30 op34 op36 op38 op40 op42 op44 op46
       op1 op5 op7 op11 op13 op17 op19 op23  op25 op29 op31 op35 op37 op39 op41 op43 op45 op47
       op2     op8      op14      op20       op26      op32
       op3     op9      op15      op21       op27      op33    for a total of 6 quad + 12 dual
    Ports: ???
      

*/


struct OPL3
{
    unsigned NumChannels;

#ifndef __DJGPP__
    std::vector<DBOPL::Handler> cards;
#endif
private:
    std::vector<unsigned short> ins; // index to adl[], cached, needed by Touch()
    std::vector<unsigned char> pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<unsigned char> regBD;
public:
    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
                                        // 3 = percussion BassDrum
                                        // 4 = percussion Snare
                                        // 5 = percussion Tom
                                        // 6 = percussion Crash cymbal
                                        // 7 = percussion Hihat
                                        // 8 = percussion slave

    void Poke(unsigned card, unsigned index, unsigned value)
    {
#ifdef __DJGPP__
        unsigned o = index >> 8;
        unsigned port = OPLBase + o * 2;
        outportb(port, index);
        for(unsigned c=0; c<6; ++c) inportb(port);
        outportb(port+1, value);
        for(unsigned c=0; c<35; ++c) inportb(port);
#else
        cards[card].WriteReg(index, value);
#endif
    }
    void NoteOff(unsigned c)
    {
        unsigned card = c/23, cc = c%23;
        if(cc >= 18)
        {
            regBD[card] &= ~(0x10 >> (cc-18));
            Poke(card, 0xBD, regBD[card]);
            return;
        }
        Poke(card, 0xB0 + Channels[cc], pit[c] & 0xDF);
    }
    void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
    {
        unsigned card = c/23, cc = c%23;
        unsigned x = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x += 0x400; } // Calculate octave
        x += (int)(hertz + 0.5);
        unsigned chn = Channels[cc];
        if(cc >= 18)
        {
            regBD[card] |= (0x10 >> (cc-18));
            Poke(card, 0x0BD, regBD[card]);
            x &= ~0x2000;
            //x |= 0x800; // for test
        }
        if(chn != 0xFFF)
        {
            Poke(card, 0xA0 + chn, x & 0xFF);
            Poke(card, 0xB0 + chn, pit[c] = x >> 8);
        }
    }
    void Touch_Real(unsigned c, unsigned volume)
    {
        if(volume > 63) volume = 63;
        unsigned card = c/23, cc = c%23;
        unsigned i = ins[c], o1 = Operators[cc*2], o2 = Operators[cc*2+1];
        unsigned x = adl[i].modulator_40, y = adl[i].carrier_40;
        bool do_modulator;
        bool do_carrier;

        unsigned mode = 1; // 2-op AM
        if(four_op_category[c] == 0 || four_op_category[c] == 3)
        {
            mode = adl[i].feedconn & 1; // 2-op FM or 2-op AM
        }
        else if(four_op_category[c] == 1 || four_op_category[c] == 2)
        {
            unsigned i0, i1;
            if ( four_op_category[c] == 1 )
            {
                i0 = i;
                i1 = ins[c + 3];
                mode = 2; // 4-op xx-xx ops 1&2
            }
            else
            {
                i0 = ins[c - 3];
                i1 = i;
                mode = 6; // 4-op xx-xx ops 3&4
            }
            mode += (adl[i0].feedconn & 1) + (adl[i1].feedconn & 1) * 2;
        }
        static const bool do_ops[10][2] =
          { { false, true },  /* 2 op FM */
            { true,  true },  /* 2 op AM */
            { false, false }, /* 4 op FM-FM ops 1&2 */
            { true,  false }, /* 4 op AM-FM ops 1&2 */
            { false, true  }, /* 4 op FM-AM ops 1&2 */
            { true,  false }, /* 4 op AM-AM ops 1&2 */
            { false, true  }, /* 4 op FM-FM ops 3&4 */
            { false, true  }, /* 4 op AM-FM ops 3&4 */
            { false, true  }, /* 4 op FM-AM ops 3&4 */
            { true,  true  }  /* 4 op AM-AM ops 3&4 */
          };

        do_modulator = ScaleModulators ? true : do_ops[ mode ][ 0 ];
        do_carrier   = ScaleModulators ? true : do_ops[ mode ][ 1 ];

        Poke(card, 0x40+o1, do_modulator ? (x|63) - volume + volume*(x&63)/63 : x);
        if(o2 != 0xFFF)
        Poke(card, 0x40+o2, do_carrier   ? (y|63) - volume + volume*(y&63)/63 : y);
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
        unsigned card = c/23, cc = c%23;
        static const unsigned char data[4] = {0x20,0x60,0x80,0xE0};
        ins[c] = i;
        unsigned o1 = Operators[cc*2+0], o2 = Operators[cc*2+1];
        unsigned x = adl[i].modulator_E862, y = adl[i].carrier_E862;
        for(unsigned a=0; a<4; ++a)
        {
            Poke(card, data[a]+o1, x&0xFF); x>>=8;
            if(o2 != 0xFFF)
            Poke(card, data[a]+o2, y&0xFF); y>>=8;
        }
    }
    void Pan(unsigned c, unsigned value)
    {
        unsigned card = c/23, cc = c%23;
        if(Channels[cc] != 0xFFF)
            Poke(card, 0xC0 + Channels[cc], adl[ins[c]].feedconn | value);
    }
    void Silence() // Silence all OPL channels.
    {
        for(unsigned c=0; c<NumChannels; ++c) { NoteOff(c); Touch_Real(c,0); }
    }
    void Reset()
    {
#ifndef __DJGPP__
        cards.resize(NumCards);
#endif
        NumChannels = NumCards * 23;
        ins.resize(NumChannels,     189);
        pit.resize(NumChannels,       0);
        regBD.resize(NumCards);
        four_op_category.resize(NumChannels);
        for(unsigned p=0, a=0; a<NumCards; ++a)
        {
            for(unsigned b=0; b<18; ++b) four_op_category[p++] = 0;
            for(unsigned b=0; b< 5; ++b) four_op_category[p++] = 8;
        }

        static const short data[] =
        { 0x004,96, 0x004,128,        // Pulse timer
          0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable
          0x001,32, 0x105,1           // Enable wave, OPL3 extensions
        };
        unsigned fours = NumFourOps;
        for(unsigned card=0; card<NumCards; ++card)
        {
#ifndef __DJGPP__
            cards[card].Init(PCM_RATE);
#endif
            for(unsigned a=0; a< 18; ++a) Poke(card, 0xB0+Channels[a], 0x00);
            for(unsigned a=0; a< sizeof(data)/sizeof(*data); a+=2)
                Poke(card, data[a], data[a+1]);
            Poke(card, 0x0BD, regBD[card] = (HighTremoloMode*0x80
                                           + HighVibratoMode*0x40
                                           + AdlPercussionMode*0x20) );
            unsigned fours_this_card = std::min(fours, 6u);
            Poke(card, 0x104, (1 << fours_this_card) - 1);
            //fprintf(stderr, "Card %u: %u four-ops.\n", card, fours_this_card);
            fours -= fours_this_card;
        }

        // Mark all channels that are reserved for four-operator function
        if(AdlPercussionMode)
            for(unsigned a=0; a<NumCards; ++a)
            {
                for(unsigned b=0; b<5; ++b) four_op_category[a*23 + 18 + b] = b+3;
                for(unsigned b=0; b<3; ++b) four_op_category[a*23 + 6  + b] = 8;
            }

        unsigned nextfour = 0;
        for(unsigned a=0; a<NumFourOps; ++a)
        {
            four_op_category[nextfour  ] = 1;
            four_op_category[nextfour+3] = 2;
            switch(a % 6)
            {
                case 0: case 1: nextfour += 1; break;
                case 2:         nextfour += 9-2; break;
                case 3: case 4: nextfour += 1; break;
                case 5:         nextfour += 23-9-2; break;
            }
        }

        /**/
        fprintf(stdout, "Channels used as:\n");
        for(size_t a=0; a<four_op_category.size(); ++a)
        {
            fprintf(stdout, " %d", four_op_category[a]);
            if(a%23 == 22) fprintf(stdout, "\n");
        }
        fflush(stdout);
        /**/
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
"PPPPPPhcckmvmxbd"  // Ins  0-15
"oooooahoGGGGGGGG"  // Ins 16-31
"BBBBBBBBVVVVVHHM"  // Ins 32-47
"SSSSOOOcTTTTTTTT"  // Ins 48-63
"XXXXTTTFFFFFFFFF"  // Ins 64-79
"LLLLLLLLpppppppp"  // Ins 80-95
"XXXXXXXXGGGGGTSS"  // Ins 96-111
"bbbbMMMcGXXXXXXX"  // Ins 112-127
"????????????????"  // Prc 0-15
"????????????????"  // Prc 16-31
"???DDshMhhhCCCbM"  // Prc 32-47
"CBDMMDDDMMDDDDDD"  // Prc 48-63
"DDDDDDDDDDDDDD??"  // Prc 64-79
"????????????????"  // Prc 80-95
"????????????????"  // Prc 96-111
"????????????????"; // Prc 112-127

static const char PercussionMap[256] =
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GM
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 3 = bass drum
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 4 = snare
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 5 = tom
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 6 = cymbal
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 7 = hihat
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP0
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP16
//2 3 4 5 6 7 8 940 1 2 3 4 5 6 7
"\0\0\0\3\3\7\4\7\4\5\7\5\7\5\7\5"//GP32
//8 950 1 2 3 4 5 6 7 8 960 1 2 3
"\5\6\5\6\6\0\5\6\0\6\0\6\5\5\5\5"//GP48
//4 5 6 7 8 970 1 2 3 4 5 6 7 8 9
"\5\0\0\0\0\0\7\0\0\0\0\0\0\0\0\0"//GP64
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

class Input
{
#ifdef __WIN32__
    void* inhandle;
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
    struct termio back;
#endif
public:
    Input()
    {
#ifdef __WIN32__
        inhandle = GetStdHandle(STD_INPUT_HANDLE);
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        ioctl(0, TCGETA, &back);
        struct termio term = back;
        term.c_lflag &= ~(ICANON|ECHO);
        term.c_cc[VMIN] = 0; // 0=no block, 1=do block
        if(ioctl(0, TCSETA, &term) < 0)
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
#endif
    }
    ~Input()
    {
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        if(ioctl(0, TCSETA, &back) < 0)
            fcntl(0, F_SETFL, fcntl(0, F_GETFL) &~ O_NONBLOCK);
#endif
    }

    char PeekInput()
    {
#ifdef __DJGPP__
        if(kbhit()) { int c = getch(); return c ? c : getch(); }
#endif
#ifdef __WIN32__
        DWORD nread=0;
        INPUT_RECORD inbuf[1];
        while(PeekConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread)&&nread)
        {
            ReadConsoleInput(inhandle,inbuf,sizeof(inbuf)/sizeof(*inbuf),&nread);
            if(inbuf[0].EventType==KEY_EVENT
            && inbuf[0].Event.KeyEvent.bKeyDown)
            {
                char c = inbuf[0].Event.KeyEvent.uChar.AsciiChar;
                unsigned s = inbuf[0].Event.KeyEvent.wVirtualScanCode;
                if(c == 0) c = s;
                return c;
        }   }
#endif
#if (!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__)
        char c = 0;
        if(read(0, &c, 1) == 1) return c;
#endif
        return '\0';
    }
} Input;

class UI
{
public:
  #ifdef __WIN32__
    void* handle;
  #endif
    int x, y, color, txtline, maxy;
    char background[80][1 + 23*MaxCards];
    char slots[80][1 + 23*MaxCards];
    unsigned char slotcolors[80][1 + 23*MaxCards];
    bool cursor_visible;
public:
    #ifdef __DJGPP__
    # define prn cprintf
    #endif
    UI(): x(0), y(0), color(-1), txtline(1),
          maxy(0), cursor_visible(true)
    {
      #ifdef __WIN32__
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
        GotoXY(41,13);
        CONSOLE_SCREEN_BUFFER_INFO tmp;
        GetConsoleScreenBufferInfo(handle,&tmp);
        if(tmp.dwCursorPosition.X != 41)
        {
            // Console is not obeying controls! Probably cygwin xterm.
            handle = 0;
        }
        else
        {
            //COORD size = { 80, 23*NumCards+5 };
            //SetConsoleScreenBufferSize(handle,size);
        }
      #endif
      #ifdef __DJGPP__
        color = 7;
      #endif
        std::memset(slots, '.',      sizeof(slots));
        std::memset(background, '.', sizeof(background));
        std::fputc('\r', stderr); // Ensure cursor is at x=0
        GotoXY(0,0); Color(15);
        prn("Hit Ctrl-C to quit\r");
    }
    void HideCursor()
    {
        if(!cursor_visible) return;
        cursor_visible = false;
      #ifdef __WIN32__
        if(handle)
        {
          const CONSOLE_CURSOR_INFO info = {100,false};
          SetConsoleCursorInfo(handle,&info);
          if(!DoingInstrumentTesting)
              CheckTetris();
          return;
        }
      #endif
        if(!DoingInstrumentTesting)
            CheckTetris();
#ifdef __DJGPP__
        { _setcursortype(_NOCURSOR);return;  }
#endif
        prn("\33[?25l"); // hide cursor
    }
    void ShowCursor()
    {
        if(cursor_visible) return;
        cursor_visible = true;
        GotoXY(0,maxy); Color(7);
      #ifdef __WIN32__
        if(handle)
        {
          const CONSOLE_CURSOR_INFO info = {100,true};
          SetConsoleCursorInfo(handle,&info);
          return;
        }
      #endif
#ifdef __DJGPP__
        { _setcursortype(_NORMALCURSOR);return;  }
#endif
        prn("\33[?25h"); // show cursor
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
        int nchars = std::vsprintf(Line, fmt, ap); /* SECURITY: POSSIBLE BUFFER OVERFLOW */
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
        #ifdef __DJGPP__
            putch( background[x][txtline] = Line[x-beginx] );
        #else
            std::fputc( background[x][txtline] = Line[x-beginx], stderr);
        #endif
        }
        for(int tx=x; tx<80; ++tx)
        {
            if(background[tx][txtline]!='.' && slots[tx][txtline]=='.')
            {
                GotoXY(tx,txtline);
                Color(1);
             #ifdef __DJGPP__
                putch(background[tx][txtline] = '.');
             #else
                std::fputc(background[tx][txtline] = '.', stderr);
             #endif
                ++x;
            }
        }
        std::fflush(stderr);

        txtline=(1 + txtline) % WinHeight();
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
    {
        HideCursor();
        int notex = 2 + (note+55)%77;
        int notey = 1 + adlchn % WinHeight();
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
        Draw(notex,notey,
            pressure?AllocateColor(ins):
            (illustrate_char=='.'?1:
             illustrate_char=='&'?1: 8),
            illustrate_char);
    }

    void Draw(int notex,int notey, int color, char ch)
    {
        if(slots[notex][notey] != ch
        || slotcolors[notex][notey] != color)
        {
            slots[notex][notey] = ch;
            slotcolors[notex][notey] = color;
            GotoXY(notex, notey);
            Color(color);
        #ifdef __WIN32__
            if(handle) WriteConsole(handle,&ch,1, 0,0);
            else
        #endif
        #ifdef __DJGPP__
            if(1) putch(ch); else
        #endif
            {
              std::fputc(ch, stderr);
              std::fflush(stderr);
            }
            ++x;
        }
    }

    void IllustrateVolumes(double left, double right)
    {
        const unsigned maxy = WinHeight();
        const unsigned white_threshold  = maxy/23;
        const unsigned red_threshold    = maxy*4/23;
        const unsigned yellow_threshold = maxy*8/23;

        double amp[2] = {left*maxy, right*maxy};
        for(unsigned y=0; y<maxy; ++y)
            for(unsigned w=0; w<2; ++w)
            {
                char c = amp[w] > (maxy-1)-y ? '|' : background[w][y+1];
                Draw(w,y+1,
                     c=='|' ? y<white_threshold ? 15
                            : y<red_threshold ? 12
                            : y<yellow_threshold ? 14
                            : 10 : (c=='.' ? 1 : 8),
                     c);
            }
    }

    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy)
    {
        if(newy > maxy) maxy = newy;
        while(newy > y)
        {
            std::fputc('\n', stderr); y+=1; x=0;
        }
      #ifdef __WIN32__
        if(handle)
        {
          CONSOLE_SCREEN_BUFFER_INFO tmp;
          GetConsoleScreenBufferInfo(handle, &tmp);
          COORD tmp2 = { x = newx, tmp.dwCursorPosition.Y } ;
          if(newy < y) { tmp2.Y -= (y-newy); y = newy; }
          SetConsoleCursorPosition(handle, tmp2);
        }
      #endif
#ifdef __DJGPP__
        { gotoxy(x=newx, wherey()-(y-newy)); y=newy; return; }
#endif
        if(newy < y) { prn("\33[%dA", y-newy); y = newy; }
        if(newx != x)
        {
            if(newx == 0 || (newx<10 && std::abs(newx-x)>=10))
                { std::fputc('\r', stderr); x = 0; }
            if(newx < x) prn("\33[%dD", x-newx);
            if(newx > x) prn("\33[%dC", newx-x);
            x = newx;
        }
    }
    // Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor)
    {
        if(color != newcolor)
        {
          #ifdef __WIN32__
            if(handle)
              SetConsoleTextAttribute(handle, newcolor);
            else
          #endif
#ifdef __DJGPP__
            textattr(newcolor);
            if(0)
#endif
            {
              static const char map[8+1] = "04261537";
              prn("\33[0;%s40;3%c",
                  (newcolor&8) ? "1;" : "", map[newcolor&7]);
              // If xterm-256color is used, try using improved colors:
              //        Translate 8 (dark gray) into #003366 (bluish dark cyan)
              //        Translate 1 (dark blue) into #000033 (darker blue)
              if(newcolor==8) prn(";38;5;24;25");
              if(newcolor==1) prn(";38;5;17;25");
              std::fputc('m', stderr);
            }
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

    void CheckTetris()
    {
        static const unsigned MH = 17;
        extern UI UI;
        static char area[12][MH]={{0}};
        static int emptycount;
        static const char empty[5][13]=
            {"!..........!", "!..SINGLE..!", "!..DOUBLE..!",
             "!..TRIPLE..!", "!..TETRIS..!"};
        static struct Tetris
        {
            static int block(int bl,int rot,int x,int y)
            {
                static const unsigned short shapes[4][7] =
                {{0xCC,0x8C4,0x4444,0x264,0xE4, 0x8E, 0x2E},
                 {0xCC,0x6C, 0xF0,  0xC6, 0x4C4,0xC88,0x88C},
                 {0xCC,0x8C4,0x4444,0x264,0x4E0,0xE2, 0xE8},
                 {0xCC,0x6C, 0xF0,  0xC6, 0x464,0x226,0x622}};
                return shapes[rot][bl] & (1 << (y*4+x));
            }
            static inline bool bounds(int x,int y,bool fix=true)
                { return x>=0 && y>=0 && (x<12 || !fix) && y < (int)MH; }
            static bool edge(int x,int y) { return x==0||x==11||y >= (int)(MH-1); }
            static void init_area()
                { for(int x=12; x-->0; ) for(int y=MH; y-->0; ) area[x][y]=edge(x,y)?2:0; }
            static void plotp(int bl,int rot, int x,int y, int color, bool fix)
                { for(int by=0; by<4; ++by) for(int bx=0; bx<4; ++bx)
                    if(bounds(x+bx,y+by,fix)&&block(bl,rot,bx,by)) setp(x+bx,y+by,color, fix); }
            static bool testp(int bl,int rot, int x,int y)
                { for(int by=0; by<4; ++by) for(int bx=0; bx<4; ++bx)
                    if(block(bl,rot,bx,by)
                    && (x+bx<=0||x+bx>10
                    || (bounds(x+bx,y+by) && area[x+bx][y+by])))
                      return false;
                  return true; }
            static void setp(int x,int y, int color, bool fix=true)
            {
                if(fix) area[x][y] = color;
                static int counter=0; ++counter;
                int c = color==2&&y<(int)(MH-1) ? (counter<700?':':'&')
                       :color==2                ? (counter<500?'-':'&')
                       :color==-1? '+' // shadow
                       :color    ? '#'
                       : (x<12?empty[emptycount][x]:'.');
                x+=2; y+=std::max(0,(int)WinHeight()-25);
                UI.background[x][y]=c;
                UI.Draw(x, y, color>2?color:1, c);
            }
            static void make_full_lines_empty()
            {
                bool fullline[MH];
                emptycount=0;
                for(int y=1; !edge(6,y); ++y)
                {
                    int x=1; while(x<=10 && area[x][y]>0) ++x;
                    if((fullline[y] = x>10)) ++emptycount;
                }
                for(int y=1; !edge(6,y); ++y)
                    if(fullline[y]) for(int x=1; x<=10; ++x) setp(x,y,0);
            }
            static void cascade_lines()
            {
                emptycount=0;
                int y=MH-2; while(edge(6,y)) --y;
                for(int srcy=y; srcy>0; --srcy)
                {
                    int x=1; while(x<=10 && !area[x][srcy]) ++x;
                    bool empty=x>10;
                    if(srcy!=y) for(int x=1; x<=10; ++x) setp(x,y,area[x][srcy]);
                    if(!empty) --y;
                }
                for(; y>1; --y) for(int x=1; x<=10; ++x) setp(x,y,0);
            }
            static int calcshadowy(int bl,int rot,int x,int y)
            {
                while(testp(bl,rot,x,y+1)) ++y;
                return y;
            }
        } tetris;
        static int gamestate=-1, curblock, currot, curx, cury, dropping;
        static int delaycounter=-1, score, lines, combo=0;
    #ifdef __DJGPP__
        static long delaytime=0;
    #endif
        static int scorewipe=0, focuswipe=0, shadowy=0, next1=0, next2=0;
        if(NumCards < 2) gamestate=1;
        switch(gamestate)
        {
            case -1: case 53: // reset game
                tetris.init_area(); gamestate=0; score=lines=0;
                next1 = std::rand()%7; next2 = std::rand()%7;
                shadowy = 25; break;
            case 0: // spawn new block
                curblock=next1; next1=next2; currot = 0;
                curx = 4; cury = delaycounter<0 ? -10 : 0; gamestate=1;
                dropping=delaycounter=emptycount=0;
                if(!tetris.testp(curblock,currot,curx,cury)) { cury=0; gamestate=50; break; }
                if(cury>=0) // keep the shadow "secret" until the game really starts
                {
                    shadowy=tetris.calcshadowy(curblock,currot,curx,cury);
                    tetris.plotp(curblock,currot,curx,shadowy,-1,0);
                }
                tetris.plotp(curblock,currot,curx,cury, 8,0);
            #ifdef __DJGPP__
                delaytime=BIOStimer;
            #endif
                //passthru
            case 1: // handle input
            {
                int level = 50 - (lines/10)/5;
                if(1)
                {
                    int y=std::rand()%MH, x=tetris.edge(6,y)?std::rand()%12:(std::rand()%2)*11;
                    tetris.setp(x,y,area[x][y]);
                }
                for(;;)
                {
                    int mr=currot,mx=curx,my=cury, move=0;
                    for(;;)
                    {
                        char ch = Input.PeekInput();
                        if(!ch) break;
                        switch(ch)
                        {
                            case 'b':
                                FakeDOSshell = true;
                                //passthru
                            case 'q': case 'Q': case 3:
                            #if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
                            case 27:
                            #endif
                                QuitFlag=true; break;
                            case '!': SkipForward = 30; break;
                            case 'w':case 'H':case 'A': mr=(currot+1)%4; move=1; break;
                            case 'a':case 'K':case 'D': mx=curx-1; move=1; break;
                            case 'd':case 'M':case 'C': mx=curx+1; move=1; break;
                            case 's':case 'P':case 'B': case ' ':
                                dropping = (ch == ' ') ? 2 : 1;
                        }
                        if(move) break;
                    }
                #ifdef __DJGPP__
                    delaycounter = (BIOStimer-delaytime) > NewTimerFreq*level/50 ? level : 0;
                #endif
                    if(!move && (dropping || ++delaycounter>=level))
                        { my=cury+1; move=1; delaycounter=0; }
                    if(scorewipe>0 && --scorewipe==0)
                    {
                        UI.GotoXY(15,(int)WinHeight());
                        fprintf(stderr,"%*s",7,""); std::fflush(stderr);
                        UI.GotoXY(15,(int)WinHeight()+1);
                        fprintf(stderr,"%*s",5,""); std::fflush(stderr);
                    }
                    if(focuswipe>0 && --focuswipe==0)
                        for(unsigned y=0; y<24 && y<MH; ++y)
                            for(int x=1; x<=10; ++x)
                                if(area[x][y]==11)
                                    tetris.setp(x,y,3); // Color of affixed blocks
                    if(!move) break;
                    if(tetris.testp(curblock,mr,mx,my))
                    {
                        bool x_changed = curx != mx || currot != mr;
                        if(x_changed && shadowy < (int)(MH-1))
                        {
                            tetris.plotp(curblock,currot,curx,shadowy,0,0); // hide shadow from old location
                        }
                        tetris.plotp(curblock,currot,curx,cury,0,0); // remove block from old location
                        currot=mr; curx=mx; cury=my;
                        if(x_changed)
                        {
                            shadowy=tetris.calcshadowy(curblock,currot,curx,cury);
                            tetris.plotp(curblock,currot,curx,shadowy,-1,0); // show shadow
                        }
                        tetris.plotp(curblock,currot,curx,cury,8,0); // show block in new location
                    }
                    else if(my==cury+1)
                    {
                        tetris.plotp(curblock,currot,curx,shadowy,0,0); // remove the shadow
                        tetris.plotp(curblock,currot,curx,cury,11,1); // affix the block
                        gamestate=2; focuswipe=3;
                        break;
                    }
                    if(dropping==1) dropping=0; else break;
                }
                break;
            }
            case 2: tetris.make_full_lines_empty(); gamestate=3;
            case 3: case 52:
            {
                int increment = "\0\1\4\11\31"[emptycount]*100;
                if(emptycount) increment += combo++ * 50; else combo=0;
                increment += cury*2; score += increment; lines += emptycount;
                int yb = std::max(0,(int)WinHeight()-25) + MH;
                UI.GotoXY(2,yb);
                UI.Color(15); prn("Score:"); std::fflush(stderr);
                UI.Color(14); prn("%6u",score); std::fflush(stderr);
                UI.GotoXY(2,yb+1);
                UI.Color(15); prn("Lines:"); std::fflush(stderr);
                UI.Color(14); prn("%6u",lines); std::fflush(stderr);
                UI.GotoXY(15,yb);
                UI.Color(10); prn("%+d",increment); std::fflush(stderr);
                UI.GotoXY(15, yb-5);
                UI.Color(15); prn("Next:"); std::fflush(stderr);
                next2=std::rand()%7;
                tetris.plotp(next1,0, 13,MH-4, 0,0);
                tetris.plotp(next2,0, 13,MH-4, 8,0);
                if(emptycount)
                {
                    UI.GotoXY(15,MH);
                    prn("%+d",emptycount); std::fflush(stderr);
                }
                scorewipe=14;
                // fallthru (set next state 4, state 53)
            }
            default: ++gamestate; break;
            case 12: if(emptycount) tetris.cascade_lines(); emptycount=gamestate=0; break;
            case 50:
                if(cury < (int)(MH-1))
                    { for(int x=1; x<=10; ++x) tetris.setp(x,cury,4); ++cury; break; }
                cury=-4; gamestate=51; break;
            case 51:
                if(cury < (int)(MH-1))
                    { for(int x=1; x<=10; ++x) tetris.setp(x,cury,0); ++cury; break; }
                emptycount=combo=score=lines=cury=0;
                gamestate=52; // reset score display
                break;
        }
    }
private:
    #ifndef __DJGPP__
    void prn(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    #endif
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

    std::map<std::string, unsigned> devices;
    std::map<unsigned/*track*/, unsigned/*channel begin index*/> current_device;

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
            // Current pressure
            unsigned char  vol;
            // Tone selected on noteon:
            unsigned short tone;
            // Patch selected on noteon; index to banks[AdlBank][]
            unsigned char midiins;
            // Index to physical adlib data structure, adlins[]
            unsigned short insmeta;
            // List of adlib channels it is currently occupying.
            std::map<unsigned short/*adlchn*/,
                     unsigned short/*ins, inde to adl[]*/
                    > phys;
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
    };
    std::vector<MIDIchannel> Ch;

    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For collisions
        struct Location
        {
            unsigned short MidCh;
            unsigned char  note;
            bool operator==(const Location&b) const
                { return MidCh==b.MidCh && note==b.note; }
            bool operator< (const Location&b) const
                { return MidCh<b.MidCh || (MidCh==b.MidCh&& note<b.note); }
        };
        struct LocationData
        {
            bool sustained;
            unsigned short ins;  // a copy of that in phys[]
            long kon_time_until_neglible;
            long vibdelay;
        };
        typedef std::map<Location, LocationData> users_t;
        users_t users;

        // If the channel is keyoff'd
        long koff_time_until_neglible;
        // For channel allocation:
        AdlChannel(): users(), koff_time_until_neglible(0) { }

        void AddAge(long ms)
        {
            if(users.empty())
                koff_time_until_neglible =
                    std::max(koff_time_until_neglible-ms, -0x1FFFFFFFl);
            else
            {
                koff_time_until_neglible = 0;
                for(users_t::iterator i = users.begin(); i != users.end(); ++i)
                {
                    i->second.kon_time_until_neglible =
                    std::max(i->second.kon_time_until_neglible-ms, -0x1FFFFFFFl);
                    i->second.vibdelay += ms;
                }
            }
        }
    };
    std::vector<AdlChannel> ch;

    std::vector< std::vector<unsigned char> > TrackData;
public:
    fraction<long> InvDeltaTicks, Tempo;
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
        std::FILE* fp = std::fopen(filename.c_str(), "rb");
        if(!fp) { std::perror(filename.c_str()); return false; }
        char HeaderBuf[4+4+2+2+2]="";
    riffskip:;
        std::fread(HeaderBuf, 1, 4+4+2+2+2, fp);
        if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
            { std::fseek(fp, 6, SEEK_CUR); goto riffskip; }
        size_t DeltaTicks=192, TrackCount=1;

        bool is_GMF = false, is_MUS = false, is_IMF = false;
        std::vector<unsigned char> MUS_instrumentList;

        if(std::memcmp(HeaderBuf, "GMF\1", 4) == 0)
        {
            // GMD/MUS files (ScummVM)
            std::fseek(fp, 7-(4+4+2+2+2), SEEK_CUR);
            is_GMF = true;
        }
        else if(std::memcmp(HeaderBuf, "MUS\1x1A", 4) == 0)
        {
            // MUS/DMX files (Doom)
            std::fseek(fp, 8-(4+4+2+2+2), SEEK_CUR);
            is_MUS = true;
            unsigned start = std::fgetc(fp); start += (std::fgetc(fp) << 8);
            std::fseek(fp, -8+start, SEEK_CUR);
        }
        else
        {
            // Try parsing as an IMF file
           {
            unsigned end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];
            if(!end || (end & 3)) goto not_imf;

            long backup_pos = std::ftell(fp);
            unsigned sum1 = 0, sum2 = 0;
            std::fseek(fp, 2, SEEK_SET);
            for(unsigned n=0; n<42; ++n)
            {
                unsigned value1 = std::fgetc(fp); value1 += std::fgetc(fp) << 8; sum1 += value1;
                unsigned value2 = std::fgetc(fp); value2 += std::fgetc(fp) << 8; sum2 += value2;
            }
            std::fseek(fp, backup_pos, SEEK_SET);
            if(sum1 > sum2)
            {
                is_IMF = true;
                DeltaTicks = 1;
            }
           }

            if(!is_IMF)
            {
            not_imf:
                if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
                { InvFmt:
                    std::fclose(fp);
                    std::fprintf(stderr, "%s: Invalid format\n", filename.c_str());
                    return false;
                }
                /*size_t  Fmt =*/ ReadBEInt(HeaderBuf+8,  2);
                TrackCount = ReadBEInt(HeaderBuf+10, 2);
                DeltaTicks = ReadBEInt(HeaderBuf+12, 2);
            }
        }
        TrackData.resize(TrackCount);
        CurrentPosition.track.resize(TrackCount);
        InvDeltaTicks = fraction<long>(1, 1000000l * DeltaTicks);
        //Tempo       = 1000000l * InvDeltaTicks;
        Tempo         = fraction<long>(1,            DeltaTicks);

        static const unsigned char EndTag[4] = {0xFF,0x2F,0x00,0x00};

        for(size_t tk = 0; tk < TrackCount; ++tk)
        {
            // Read track header
            size_t TrackLength;
            if(is_IMF)
            {
                //std::fprintf(stderr, "Reading IMF file...\n");
                long end = (unsigned char)HeaderBuf[0] + 256*(unsigned char)HeaderBuf[1];

                unsigned IMF_tempo = 1428;
                static const unsigned char imf_tempo[] = {0xFF,0x51,0x4,
                    (unsigned char)(IMF_tempo>>24),
                    (unsigned char)(IMF_tempo>>16),
                    (unsigned char)(IMF_tempo>>8),
                    (unsigned char)(IMF_tempo)};
                TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
                TrackData[tk].push_back(0x00);

                std::fseek(fp, 2, SEEK_SET);
                while(std::ftell(fp) < end)
                {
                    unsigned char special_event_buf[5];
                    special_event_buf[0] = 0xFF;
                    special_event_buf[1] = 0xE3;
                    special_event_buf[2] = 0x02;
                    special_event_buf[3] = std::fgetc(fp); // port index
                    special_event_buf[4] = std::fgetc(fp); // port value
                    unsigned delay = std::fgetc(fp); delay += 256 * std::fgetc(fp);

                    //if(special_event_buf[3] <= 8) continue;

                    //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);

                    TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf+5);
                    //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                    if(delay>>14) TrackData[tk].push_back( 0x80 | ((delay>>14) & 0x7F ) );
                    if(delay>> 7) TrackData[tk].push_back( 0x80 | ((delay>> 7) & 0x7F ) );
                    TrackData[tk].push_back( ((delay>>0) & 0x7F ) );
                }
                TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                CurrentPosition.track[tk].delay = 0;
                CurrentPosition.began = true;
                //std::fprintf(stderr, "Done reading IMF file\n");
            }
            else
            {
                if(is_GMF)
                {
                    long pos = std::ftell(fp);
                    std::fseek(fp, 0, SEEK_END);
                    TrackLength = ftell(fp) - pos;
                    std::fseek(fp, pos, SEEK_SET);
                }
                else if(is_MUS)
                {
                    long pos = std::ftell(fp);
                    std::fseek(fp, 4, SEEK_SET);
                    TrackLength = std::fgetc(fp); TrackLength += (std::fgetc(fp) << 8);
                    std::fseek(fp, pos, SEEK_SET);
                }
                else
                {
                    std::fread(HeaderBuf, 1, 8, fp);
                    if(std::memcmp(HeaderBuf, "MTrk", 4) != 0) goto InvFmt;
                    TrackLength = ReadBEInt(HeaderBuf+4, 4);
                }
                // Read track data
                TrackData[tk].resize(TrackLength);
                std::fread(&TrackData[tk][0], 1, TrackLength, fp);
                if(is_GMF || is_MUS)
                {
                    TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
                }
                // Read next event time
                CurrentPosition.track[tk].delay = ReadVarLen(tk);
            }
        }
        loopStart = true;

        opl.Reset(); // Reset AdLib
        //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
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
        while(CurrentPosition.wait <= granularity * 0.5)
        {
            //std::fprintf(stderr, "wait = %g...\n", CurrentPosition.wait);
            ProcessEvents();
        }

        for(unsigned c = 0; c < opl.NumChannels; ++c)
            ch[c].AddAge(s * 1000);

        UpdateVibrato(s);
        UpdateArpeggio(s);
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
         unsigned props_mask,
         int select_adlchn = -1)
    {
        MIDIchannel::NoteInfo& info = i->second;
        const int tone    = info.tone;
        const int vol     = info.vol;
        const int midiins = info.midiins;
        const int insmeta = info.insmeta;

        AdlChannel::Location my_loc;
        my_loc.MidCh = MidCh;
        my_loc.note  = i->first;

        for(std::map<unsigned short,unsigned short>::iterator
            jnext = info.phys.begin();
            jnext != info.phys.end();
           )
        {
            std::map<unsigned short,unsigned short>::iterator j(jnext++);
            int c   = j->first;
            int ins = j->second;
            if(select_adlchn >= 0 && c != select_adlchn) continue;

            if(props_mask & Upd_Patch)
            {
                opl.Patch(c, ins);
                AdlChannel::LocationData& d = ch[c].users[my_loc];
                d.sustained = false; // inserts if necessary
                d.vibdelay  = 0;
                d.kon_time_until_neglible = adlins[insmeta].ms_sound_kon;
                d.ins       = ins;
            }
        }
        for(std::map<unsigned short,unsigned short>::iterator
            jnext = info.phys.begin();
            jnext != info.phys.end();
           )
        {
            std::map<unsigned short,unsigned short>::iterator j(jnext++);
            int c   = j->first;
            int ins = j->second;
            if(select_adlchn >= 0 && c != select_adlchn) continue;

            if(props_mask & Upd_Off) // note off
            {
                if(Ch[MidCh].sustain == 0)
                {
                    AdlChannel::users_t::iterator k = ch[c].users.find(my_loc);
                    if(k != ch[c].users.end())
                        ch[c].users.erase(k);
                    UI.IllustrateNote(c, tone, midiins, 0, 0.0);

                    if(ch[c].users.empty())
                    {
                        opl.NoteOff(c);
                        ch[c].koff_time_until_neglible =
                            adlins[insmeta].ms_sound_koff;
                    }
                }
                else
                {
                    // Sustain: Forget about the note, but don't key it off.
                    //          Also will avoid overwriting it very soon.
                    AdlChannel::LocationData& d = ch[c].users[my_loc];
                    d.sustained = true; // note: not erased!
                    UI.IllustrateNote(c, tone, midiins, -1, 0.0);
                }
                info.phys.erase(j);
                continue;
            }
            if(props_mask & Upd_Pan)
            {
                opl.Pan(c, Ch[MidCh].panning);
            }
            if(props_mask & Upd_Volume)
            {
                int volume = vol * Ch[MidCh].volume * Ch[MidCh].expression;
                /* If the channel has arpeggio, the effective volume of
                 * *this* instrument is actually lower due to timesharing.
                 * To compensate, add extra volume that corresponds to the
                 * time this note is *not* heard.
                 * Empirical tests however show that a full equal-proportion
                 * increment sounds wrong. Therefore, using the square root.
                 */
                //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));
                opl.Touch(c, volume);
            }
            if(props_mask & Upd_Pitch)
            {
                AdlChannel::LocationData& d = ch[c].users[my_loc];
                // Don't bend a sustained note
                if(!d.sustained)
                {
                    double bend = Ch[MidCh].bend + adl[ins].finetune;
                    double phase = 0.0;

                    if((adlins[insmeta].flags & adlinsdata::Flag_Pseudo4op) && ins == adlins[insmeta].adlno2)
                    {
                        phase = 0.125; // Detune the note slightly (this is what Doom does)
                    }

                    if(Ch[MidCh].vibrato && d.vibdelay >= Ch[MidCh].vibdelay)
                        bend += Ch[MidCh].vibrato * Ch[MidCh].vibdepth * std::sin(Ch[MidCh].vibpos);
                    opl.NoteOn(c, 172.00093 * std::exp(0.057762265 * (tone + bend + phase)));
                    UI.IllustrateNote(c, tone, midiins, vol, Ch[MidCh].bend);
                }
            }
        }
        if(info.phys.empty())
            Ch[MidCh].activenotes.erase(i);
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

        fraction<long> t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t.valuel();

        //if(shortest > 0) UI.PrintLn("Delay %ld (%g)", shortest, (double)t.valuel());

        /*
        if(CurrentPosition.track[0].ptr > 8119) loopEnd = true;
        // ^HACK: CHRONO TRIGGER LOOP
        */

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
            if(QuitWithoutLooping)
            {
                QuitFlag = true;
                //^ HACK: QUIT WITHOUT LOOPING
            }
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
            if(evtype == 0x51) { Tempo = InvDeltaTicks * fraction<long>( (long) ReadBEInt(data.data(), data.size())); return; }
            if(evtype == 6 && data == "loopStart") loopStart = true;
            if(evtype == 6 && data == "loopEnd"  ) loopEnd   = true;
            if(evtype == 9) current_device[tk] = ChooseDevice(data);
            if(evtype >= 1 && evtype <= 6)
                UI.PrintLn("Meta %d: %s", evtype, data.c_str());

            if(evtype == 0xE3) // Special non-spec ADLMIDI special for IMF playback: Direct poke to AdLib
            {
                unsigned char i = data[0], v = data[1];
                if( (i&0xF0) == 0xC0 ) v |= 0x30;
                //fprintf(stderr, "OPL poke %02X, %02X\n", i,v);
                opl.Poke(0, i,v);
            }
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
        MidCh += current_device[tk];

        CurrentPosition.track[tk].status = byte;
        switch(EvType)
        {
            case 0x8: // Note off
            case 0x9: // Note on
            {
                int note = TrackData[tk][CurrentPosition.track[tk].ptr++];
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                //if(MidCh != 9) note -= 12; // HACK
                NoteOff(MidCh, note);
                // On Note on, Keyoff the note first, just in case keyoff
                // was omitted; this fixes Dance of sugar-plum fairy
                // by Microsoft. Now that we've done a Keyoff,
                // check if we still need to do a Keyon.
                // vol=0 and event 8x are both Keyoff-only.
                if(vol == 0 || EvType == 0x8) break;

                unsigned midiins = Ch[MidCh].patch;
                if(MidCh%16 == 9) midiins = 128 + note; // Percussion instrument

                /*
                if(MidCh%16 == 9 || (midiins != 32 && midiins != 46 && midiins != 48 && midiins != 50))
                    break; // HACK
                if(midiins == 46) vol = (vol*7)/10;          // HACK
                if(midiins == 48 || midiins == 50) vol /= 4; // HACK
                */
                //if(midiins == 56) vol = vol*6/10; // HACK

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
                int tone = note;
                if(adlins[meta].tone)
                {
                    if(adlins[meta].tone < 20)
                        tone += adlins[meta].tone;
                    else if(adlins[meta].tone < 128)
                        tone = adlins[meta].tone;
                    else
                        tone -= adlins[meta].tone-128;
                }
                int i[2] = { adlins[meta].adlno1, adlins[meta].adlno2 };
                bool pseudo_4op = adlins[meta].flags & adlinsdata::Flag_Pseudo4op;

                if(AdlPercussionMode && PercussionMap[midiins & 0xFF]) i[1] = i[0];

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
                    long bs = -0x7FFFFFFFl;
                    for(int a = 0; a < (int)opl.NumChannels; ++a)
                    {
                        if(ccount == 1 && a == adlchannel[0]) continue;
                        // ^ Don't use the same channel for primary&secondary

                        if(i[0] == i[1] || pseudo_4op)
                        {
                            // Only use regular channels
                            int expected_mode = 0;
                            if(AdlPercussionMode)
                                expected_mode = PercussionMap[midiins & 0xFF];
                            if(opl.four_op_category[a] != expected_mode)
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

                        long s = CalculateAdlChannelGoodness(a, i[ccount], MidCh);
                        if(s > bs) { bs=s; c = a; } // Best candidate wins
                    }

                    if(c < 0)
                    {
                        //UI.PrintLn("ignored unplaceable note");
                        continue; // Could not play this note. Ignore it.
                    }
                    PrepareAdlChannelForNewNote(c, i[ccount]);
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
                ir.first->second.vol     = vol;
                ir.first->second.tone    = tone;
                ir.first->second.midiins = midiins;
                ir.first->second.insmeta = meta;
                for(unsigned ccount=0; ccount<2; ++ccount)
                {
                    int c = adlchannel[ccount];
                    if(c < 0) continue;
                    ir.first->second.phys[ adlchannel[ccount] ] = i[ccount];
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
                        if(!value) KillSustainingNotes( MidCh );
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
                        KillSustainingNotes(MidCh);
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

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    long CalculateAdlChannelGoodness
        (unsigned c, unsigned ins, unsigned /*MidCh*/) const
    {
        long s = -ch[c].koff_time_until_neglible;

        // Same midi-instrument = some stability
        //if(c == MidCh) s += 4;
        for(AdlChannel::users_t::const_iterator
            j = ch[c].users.begin();
            j != ch[c].users.end();
            ++j)
        {
            s -= 4000;
            if(!j->second.sustained)
                s -= j->second.kon_time_until_neglible;
            else
                s -= j->second.kon_time_until_neglible / 2;

            MIDIchannel::activenotemap_t::const_iterator
                k = Ch[j->first.MidCh].activenotes.find(j->first.note);
            if(k != Ch[j->first.MidCh].activenotes.end())
            {
                // Same instrument = good
                if(j->second.ins == ins)
                {
                    s += 300;
                    // Arpeggio candidate = even better
                    if(j->second.vibdelay < 70
                    || j->second.kon_time_until_neglible > 20000)
                        s += 0;
                }
                // Percussion is inferior to melody
                s += 50 * (k->second.midiins / 128);

                /*
                if(k->second.midiins >= 25
                && k->second.midiins < 40
                && j->second.ins != ins)
                {
                    s -= 14000; // HACK: Don't clobber the bass or the guitar
                }
                */
            }

            // If there is another channel to which this note
            // can be evacuated to in the case of congestion,
            // increase the score slightly.
            unsigned n_evacuation_stations = 0;
            for(unsigned c2 = 0; c2 < opl.NumChannels; ++c2)
            {
                if(c2 == c) continue;
                if(opl.four_op_category[c2]
                != opl.four_op_category[c]) continue;
                for(AdlChannel::users_t::const_iterator
                    m = ch[c2].users.begin();
                    m != ch[c2].users.end();
                    ++m)
                {
                    if(m->second.sustained)       continue;
                    if(m->second.vibdelay >= 200) continue;
                    if(m->second.ins != j->second.ins) continue;
                    n_evacuation_stations += 1;
                }
            }
            s += n_evacuation_stations * 4;
        }
        return s;
    }

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(int c, int ins)
    {
        if(ch[c].users.empty()) return; // Nothing to do
        //bool doing_arpeggio = false;
        for(AdlChannel::users_t::iterator
            jnext = ch[c].users.begin();
            jnext != ch[c].users.end();
            )
        {
            AdlChannel::users_t::iterator j(jnext++);
            if(!j->second.sustained)
            {
                // Collision: Kill old note,
                // UNLESS we're going to do arpeggio

                MIDIchannel::activenoteiterator i
                ( Ch[j->first.MidCh].activenotes.find( j->first.note ) );

                // Check if we can do arpeggio.
                if((j->second.vibdelay < 70
                 || j->second.kon_time_until_neglible > 20000)
                && j->second.ins == ins)
                {
                    // Do arpeggio together with this note.
                    //doing_arpeggio = true;
                    continue;
                }

                KillOrEvacuate(c,j,i);
                // ^ will also erase j from ch[c].users.
            }
        }

        // Kill all sustained notes on this channel
        // Don't keep them for arpeggio, because arpeggio requires
        // an intact "activenotes" record. This is a design flaw.
        KillSustainingNotes(-1, c);

        // Keyoff the channel so that it can be retriggered,
        // unless the new note will be introduced as just an arpeggio.
        if(ch[c].users.empty())
            opl.NoteOff(c);
    }

    void KillOrEvacuate(
        unsigned from_channel,
        AdlChannel::users_t::iterator j,
        MIDIchannel::activenoteiterator i)
    {
        // Before killing the note, check if it can be
        // evacuated to another channel as an arpeggio
        // instrument. This helps if e.g. all channels
        // are full of strings and we want to do percussion.
        // FIXME: This does not care about four-op entanglements.
        for(unsigned c = 0; c < opl.NumChannels; ++c)
        {
            if(c == from_channel) continue;
            if(opl.four_op_category[c]
            != opl.four_op_category[from_channel]
              ) continue;
            for(AdlChannel::users_t::iterator
                m = ch[c].users.begin();
                m != ch[c].users.end();
                ++m)
            {
                if(m->second.vibdelay >= 200
                && m->second.kon_time_until_neglible < 10000) continue;
                if(m->second.ins != j->second.ins) continue;

                // the note can be moved here!
                UI.IllustrateNote(
                    from_channel,
                    i->second.tone,
                    i->second.midiins, 0, 0.0);
                UI.IllustrateNote(
                    c,
                    i->second.tone,
                    i->second.midiins,
                    i->second.vol,
                    0.0);

                i->second.phys.erase(from_channel);
                i->second.phys[c] = j->second.ins;
                ch[c].users.insert( *j );
                ch[from_channel].users.erase( j );
                return;
            }
        }

        /*UI.PrintLn(
            "collision @%u: [%ld] <- ins[%3u]",
            c,
            //ch[c].midiins<128?'M':'P', ch[c].midiins&127,
            ch[c].age, //adlins[ch[c].insmeta].ms_sound_kon,
            ins
            );*/

        // Kill it
        NoteUpdate(j->first.MidCh,
                   i,
                   Upd_Off,
                   from_channel);
    }

    void KillSustainingNotes(int MidCh = -1, int this_adlchn = -1)
    {
        unsigned first=0, last=opl.NumChannels;
        if(this_adlchn >= 0) { first=this_adlchn; last=first+1; }
        for(unsigned c = first; c < last; ++c)
        {
            if(ch[c].users.empty()) continue; // Nothing to do
            for(AdlChannel::users_t::iterator
                jnext = ch[c].users.begin();
                jnext != ch[c].users.end();
                )
            {
                AdlChannel::users_t::iterator j(jnext++);
                if((MidCh < 0 || j->first.MidCh == MidCh)
                && j->second.sustained)
                {
                    int midiins = '?';
                    UI.IllustrateNote(c, j->first.note, midiins, 0, 0.0);
                    ch[c].users.erase(j);
                }
            }
            // Keyoff the channel, if there are no users left.
            if(ch[c].users.empty())
                opl.NoteOff(c);
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

    void UpdateVibrato(double amount)
    {
        for(unsigned a=0, b=Ch.size(); a<b; ++a)
            if(Ch[a].vibrato && !Ch[a].activenotes.empty())
            {
                NoteUpdate_All(a, Upd_Pitch);
                Ch[a].vibpos += amount * Ch[a].vibspeed;
            }
            else
                Ch[a].vibpos = 0.0;
    }

    void UpdateArpeggio(double /*amount*/) // amount = amount of time passed
    {
        // If there is an adlib channel that has multiple notes
        // simulated on the same channel, arpeggio them.
    #if 0
        const unsigned desired_arpeggio_rate = 40; // Hz (upper limit)
       #if 1
        static unsigned cache=0;
        amount=amount; // Ignore amount. Assume we get a constant rate.
        cache += MaxSamplesAtTime * desired_arpeggio_rate;
        if(cache < PCM_RATE) return;
        cache %= PCM_RATE;
      #else
        static double arpeggio_cache = 0;
        arpeggio_cache += amount * desired_arpeggio_rate;
        if(arpeggio_cache < 1.0) return;
        arpeggio_cache = 0.0;
      #endif
    #endif
        static unsigned arpeggio_counter = 0;
        ++arpeggio_counter;

        for(unsigned c = 0; c < opl.NumChannels; ++c)
        {
        retry_arpeggio:;
            size_t n_users = ch[c].users.size();
            /*if(true)
            {
                UI.GotoXY(64,c+1); UI.Color(2);
                std::fprintf(stderr, "%7ld/%7ld,%3u\r",
                    ch[c].keyoff,
                    (unsigned) n_users);
                UI.x = 0;
            }*/
            if(n_users > 1)
            {
                AdlChannel::users_t::const_iterator i = ch[c].users.begin();
                size_t rate_reduction = 3;
                if(n_users >= 3) rate_reduction = 2;
                if(n_users >= 4) rate_reduction = 1;
                std::advance(i, (arpeggio_counter / rate_reduction) % n_users);
                if(i->second.sustained == false)
                {
                    if(i->second.kon_time_until_neglible <= 0l)
                    {
                        NoteUpdate(
                            i->first.MidCh,
                            Ch[ i->first.MidCh ].activenotes.find( i->first.note ),
                            Upd_Off,
                            c);
                        goto retry_arpeggio;
                    }
                    NoteUpdate(
                        i->first.MidCh,
                        Ch[ i->first.MidCh ].activenotes.find( i->first.note ),
                        Upd_Pitch | Upd_Volume | Upd_Pan,
                        c);
                }
            }
        }
    }

public:
    unsigned ChooseDevice(const std::string& name)
    {
        std::map<std::string, unsigned>::iterator i = devices.find(name);
        if(i != devices.end()) return i->second;
        size_t n = devices.size() * 16;
        devices.insert( std::make_pair(name, n) );
        Ch.resize(n+16);
        return n;
    }
};

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
                6.0,  // wet_gain_dB  (-10..10)
                .7,   // room_scale   (0..1)
                .6,   // reverberance (0..1)
                .8,   // hf_damping   (0..1)
                .000, // pre_delay_s  (0.. 0.5)
                1,   // stereo_depth (0..1)
                MaxSamplesAtTime);
    }
} reverb_data;

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
static void SDL_AudioCallback(void*, Uint8* stream, int len)
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
    if(count % 2 == 1)
    {
        // An uneven number of samples? To avoid complicating matters,
        // just ignore the odd sample.
        count   -= 1;
        samples += 1;
    }
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
                .5 * (reverb_data.chan[0].out[w][p]
                    + reverb_data.chan[1].out[w][p])) * 32768.0f
                 + average_flt[w];
            AudioBuffer[pos+p*2+w] =
                out<-32768.f ? -32768 :
                out>32767.f ?  32767 : out;
        }
    if(WritePCMfile)
    {
        /* HACK: Cheat on DOSBox recording: Record audio separately on Windows. */
        static FILE* fp = 0;
        if(!fp)
        {
            fp = fopen("adlmidi.wav", "wb");
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
#ifndef __WIN32__
    AudioBuffer_lock.Unlock();
#else
    if(!WritePCMfile)
        WindowsAudio::Write( (const unsigned char*) &AudioBuffer[0], 2*AudioBuffer.size());
#endif
}
#endif /* not DJGPP */

class Tester
{
    unsigned cur_gm;
    unsigned ins_idx;
    std::vector<unsigned> adl_ins_list;
    OPL3& opl;
public:
    Tester(OPL3& o) : opl(o)
    {
        cur_gm   = 0;
        ins_idx  = 0;
    }
    ~Tester()
    {
    }

    // Find list of adlib instruments that supposedly implement this GM
    void FindAdlList()
    {
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);

        std::set<unsigned> adl_ins_set;
        for(unsigned bankno=0; bankno<NumBanks; ++bankno)
            adl_ins_set.insert(banks[bankno][cur_gm]);
        adl_ins_list.assign( adl_ins_set.begin(), adl_ins_set.end() );
        ins_idx = 0;
        NextAdl(0);
        opl.Silence();
    }

    void DoNote(int note)
    {
        if(adl_ins_list.empty()) FindAdlList();
        int meta = adl_ins_list[ins_idx];
        const adlinsdata& ains = adlins[meta];
        int tone = (cur_gm & 128) ? (cur_gm & 127) : (note+50);
        if(ains.tone)
        {
            if(ains.tone < 20)
                tone += ains.tone;
            else if(ains.tone < 128)
                tone = ains.tone;
            else
                tone -= ains.tone-128;
        }
        double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
        int i[2] = { ains.adlno1, ains.adlno2 };
        int adlchannel[2] = { 0, 3 };
        if(i[0] == i[1])
        {
            adlchannel[1] = -1;
            adlchannel[0] = 6; // single-op
            std::printf("noteon at %d(%d) for %g Hz\n",
                adlchannel[0], i[0], hertz);
        }
        else
        {
            std::printf("noteon at %d(%d) and %d(%d) for %g Hz\n",
                adlchannel[0], i[0], adlchannel[1], i[1], hertz);
        }

        opl.NoteOff(0); opl.NoteOff(3); opl.NoteOff(6);
        for(unsigned c=0; c<2; ++c)
        {
            if(adlchannel[c] < 0) continue;
            opl.Patch(adlchannel[c], i[c]);
            opl.Touch(adlchannel[c], 127*127*100);
            opl.Pan(adlchannel[c], 0x30);
            opl.NoteOn(adlchannel[c], hertz);
        }
    }

    void NextGM(int offset)
    {
        cur_gm = (cur_gm + 256 + offset) & 0xFF;
        FindAdlList();
    }

    void NextAdl(int offset)
    {
        if(adl_ins_list.empty()) FindAdlList();
        const unsigned NumBanks = sizeof(banknames)/sizeof(*banknames);
        ins_idx = (ins_idx + adl_ins_list.size() + offset) % adl_ins_list.size();

        UI.Color(15); std::fflush(stderr);
        std::printf("SELECTED G%c%d\t%s\n",
            cur_gm<128?'M':'P', cur_gm<128?cur_gm+1:cur_gm-128,
            "<-> select GM, ^v select ins, qwe play note");
        std::fflush(stdout);
        UI.Color(7); std::fflush(stderr);
        for(unsigned a=0; a<adl_ins_list.size(); ++a)
        {
            unsigned i = adl_ins_list[a];
            char ToneIndication[8] = "   ";
            if(adlins[i].tone)
            {
                if(adlins[i].tone < 20)
                    sprintf(ToneIndication, "+%-2d", adlins[i].tone);
                else if(adlins[i].tone < 128)
                    sprintf(ToneIndication, "=%-2d", adlins[i].tone);
                else
                    sprintf(ToneIndication, "-%-2d", adlins[i].tone-128);
            }
            std::printf("%s%s%s%u\t",
                ToneIndication,
                adlins[i].adlno1 != adlins[i].adlno2 ? "[2]" : "   ",
                (ins_idx == a) ? "->" : "\t",
                i
            );

            for(unsigned bankno=0; bankno<NumBanks; ++bankno)
                if(banks[bankno][cur_gm] == i)
                    std::printf(" %u", bankno);

            std::printf("\n");
        }
    }

    void HandleInputChar(char ch)
    {
        static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
        //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
        switch(ch)
        {
            case '/': case 'H': case 'A': NextAdl(-1); break;
            case '*': case 'P': case 'B': NextAdl(+1); break;
            case '-': case 'K': case 'D': NextGM(-1); break;
            case '+': case 'M': case 'C': NextGM(+1); break;
            case 3:
        #if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
            case 27:
        #endif
                QuitFlag=true; break;
            default:
                const char* p = strchr(notes, ch);
                if(p && *p) DoNote( (p - notes) - 12 );
    }   }

    double Tick(double eat_delay, double mindelay)
    {
        HandleInputChar( Input.PeekInput() );
        return 0.1; //eat_delay;
    }
};

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
    char* cmdline = GetCommandLine();
    int argc = ParseCommandLine(cmdline, NULL);
    char**argv = new char* [argc+1];
    ParseCommandLine(cmdline, argv);
#else
#undef main

int main(int argc, char** argv)
{
#endif
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often SDL_AudioCallBack()
    // is called.
    const double AudioBufferLength = 0.045;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.

    UI.Color(15); std::fflush(stderr);
    std::printf(
#ifdef __DJGPP__
        "ADLMIDI_A: MIDI player for OPL3 hardware\n"
#else
        "ADLMIDI: MIDI player for Linux and Windows with OPL3 emulation\n"
#endif
    );
    std::fflush(stdout);
    UI.Color(3); std::fflush(stderr);
    std::printf(
        "(C) 2011 Joel Yliluoma -- http://bisqwit.iki.fi/source/adlmidi.html\n");
    std::fflush(stdout);
    UI.Color(7); std::fflush(stderr);

    signal(SIGINT, TidyupAndExit);

#ifndef __DJGPP__

#ifndef __WIN32__
    // Set up SDL
    static SDL_AudioSpec spec, obtained;
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = spec.freq * AudioBufferLength;
    spec.callback = SDL_AudioCallback;
    if(SDL_OpenAudio(&spec, &obtained) < 0)
    {
        std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        //return 1;
    }
    if(spec.samples != obtained.samples)
        std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
            spec.samples,    spec.freq,    spec.channels,
            obtained.samples,obtained.freq,obtained.channels);
#endif

#endif /* not DJGPP */

    if(argc < 2)
    {
        UI.Color(7);  std::fflush(stderr);
        std::printf(
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "       adlmidi <midifilename> -1   To enter instrument tester\n"
            " -p Enables adlib percussion instrument mode\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -nl Quit without looping\n"
            " -w Write WAV file rather than playing\n"
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
            "     The fourth parameter can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
            );
        return 0;
    }

    while(argc > 2)
    {
        if(!std::strcmp("-p", argv[2]))
            AdlPercussionMode = true;
        else if(!std::strcmp("-v", argv[2]))
            HighVibratoMode = true;
        else if(!std::strcmp("-t", argv[2]))
            HighTremoloMode = true;
        else if(!std::strcmp("-nl", argv[2]))
            QuitWithoutLooping = true;
        else if(!std::strcmp("-w", argv[2]))
            WritePCMfile = true;
        else if(!std::strcmp("-s", argv[2]))
            ScaleModulators = true;
        else break;

        for(int p=2; p<argc; ++p) argv[p] = argv[p+1];
        --argc;
    }

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
            return 0;
        }
        std::printf("FM instrument bank %u selected.\n", AdlBank);
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
    std::printf("This bank has %u/%u four-op melodic instruments and %u/%u percussive ones.\n",
        n_fourop[0], n_total[0],
        n_fourop[1], n_total[1]);

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
            DoingInstrumentTesting ? 2
          : (n_fourop[0] >= n_total[0]*7/8) ? NumCards * 6
          : (n_fourop[0] < n_total[0]*1/8) ? 0
          : (NumCards==1 ? 1 : NumCards*4);

    std::printf(
        "Simulating %u OPL3 cards for a total of %u operators.\n"
        "Setting up the operators as %u four-op channels, %u dual-op channels",
        NumCards, NumCards*36,
        NumFourOps, (AdlPercussionMode ? 15 : 18) * NumCards - NumFourOps*2);
    if(AdlPercussionMode)
        std::printf(", %u percussion channels", NumCards * 5);
    std::printf("\n");
    std::fflush(stdout);

    MIDIplay player;
    player.ChooseDevice("");
    if(!player.LoadMIDI(argv[1]))
        return 2;

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

#ifdef __WIN32
    WindowsAudio::Open(PCM_RATE, 2, 16);
#else
    SDL_PauseAudio(0);
#endif

#endif /* djgpp */

    Tester InstrumentTester(player.opl);

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
                player.opl.cards[0].Generate(0, SendStereoAudio, n_samples);
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
                    player.opl.cards[card].Generate(
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
            while(AudioBuffer.size() > obtained.samples + (obtained.freq*2) * OurHeadRoomLength)
            {
                if(!WritePCMfile)
                    SDL_Delay(1); // std::min(10.0, 1e3 * eat_delay) );
                else
                {
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
