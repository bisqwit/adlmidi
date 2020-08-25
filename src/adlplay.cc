#include <cstdlib>
#include <cstring>
#include <set>

#ifdef __DJGPP__
# include <dos.h>
#endif

#ifndef __DJGPP__
#include "dbopl.h"
#endif

#include "adldata.hh"
#include "../include/adlplay.h"
#include "../include/adlcpp.h"
#include "../include/adlui.h"
#include "../include/adlinput.h"
#include "adlpriv.hh"


extern ADL_UserInterface UI;
extern ADL_Input Input;


static const unsigned short Operators[23*2] =
    {// Channels 0-2
     0x000,0x003,0x001,0x004,0x002,0x005, // operators  0, 3,  1, 4,  2, 5
     // Channels 3-5
     0x008,0x00B,0x009,0x00C,0x00A,0x00D, // operators  6, 9,  7,10,  8,11
     // Channels 6-8
     0x010,0x013,0x011,0x014,0x012,0x015, // operators 12,15, 13,16, 14,17
     // Same for second card
     0x100,0x103,0x101,0x104,0x102,0x105, // operators 18,21, 19,22, 20,23
     0x108,0x10B,0x109,0x10C,0x10A,0x10D, // operators 24,27, 25,28, 26,29
     0x110,0x113,0x111,0x114,0x112,0x115, // operators 30,33, 31,34, 32,35
     // Channel 18
     0x010,0x013,   // operators 12,15
     // Channel 19
     0x014,0xFFF,   // operator 16
     // Channel 19
     0x012,0xFFF,   // operator 14
     // Channel 19
     0x015,0xFFF,   // operator 17
     // Channel 19
     0x011,0xFFF }; // operator 13

static const unsigned short Channels[23] =
    {0x000,0x001,0x002, 0x003,0x004,0x005, 0x006,0x007,0x008, // 0..8
     0x100,0x101,0x102, 0x103,0x104,0x105, 0x106,0x107,0x108, // 9..17 (secondary set)
     0x006,0x007,0x008, 0xFFF,0xFFF }; // <- hw percussions, 0xFFF = no support for pitch/pan

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
"\0\0\0\3\3\0\0\7\0\5\7\5\0\5\7\5"//GP32
//8 950 1 2 3 4 5 6 7 8 960 1 2 3
"\5\6\5\0\6\0\5\6\0\6\0\6\5\5\5\5"//GP48
//4 5 6 7 8 970 1 2 3 4 5 6 7 8 9
"\5\0\0\0\0\0\7\0\0\0\0\0\0\0\0\0"//GP64
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";


static std::vector<adlinsdata> dynamic_metainstruments; // Replaces adlins[] when CMF file
static std::vector<adldata>    dynamic_instruments;     // Replaces adl[]    when CMF file
static const unsigned DynamicInstrumentTag = 0x8000u, DynamicMetaInstrumentTag = 0x4000000u;
static const adlinsdata& GetAdlMetaIns(unsigned n)
{
    return (n & DynamicMetaInstrumentTag)
        ? dynamic_metainstruments[n & ~DynamicMetaInstrumentTag]
        : adlins[n];
}
static unsigned GetAdlMetaNumber(unsigned midiins)
{
    return (AdlBank == ~0u)
        ? (midiins | DynamicMetaInstrumentTag)
        : banks[AdlBank][midiins];
}
static const adldata& GetAdlIns(unsigned short insno)
{
    return (insno & DynamicInstrumentTag)
        ? dynamic_instruments[insno & ~DynamicInstrumentTag]
        : adl[insno];
}



ADLMIDI_EXPORT OPL3::~OPL3()
{
#ifndef __DJGPP__
    for(unsigned card=0; card<cards.size(); ++card)
    {
        delete cards[card];
        cards[card] = nullptr;
    }
    cards.clear();
#endif
}

ADLMIDI_EXPORT void OPL3::Poke(unsigned card, unsigned index, unsigned value)
{
#ifdef __DJGPP__
    unsigned o = index >> 8;
    unsigned port = OPLBase + o * 2;
    outportb(port, index);
    for(unsigned c=0; c<6; ++c) inportb(port);
    outportb(port+1, value);
    for(unsigned c=0; c<35; ++c) inportb(port);
#else
    cards[card]->WriteReg(index, value);
#endif
}

ADLMIDI_EXPORT void OPL3::NoteOff(unsigned c)
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

ADLMIDI_EXPORT void OPL3::NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
{
    unsigned card = c/23, cc = c%23;
    unsigned x = 0x2000;
    if(hertz < 0 || hertz > 131071) // Avoid infinite loop
        return;
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

ADLMIDI_EXPORT void OPL3::Touch_Real(unsigned c, unsigned volume)
{
    if(volume > 63) volume = 63;
    unsigned card = c/23, cc = c%23;
    unsigned i = ins[c];
    unsigned o1 = Operators[cc*2+0];
    unsigned o2 = Operators[cc*2+1];

    const adldata& adli = GetAdlIns(i);
    unsigned x = adli.modulator_40, y = adli.carrier_40;

    unsigned mode = 1; // 2-op AM
    if(four_op_category[c] == 0 || four_op_category[c] == 3)
    {
        mode = adli.feedconn & 1; // 2-op FM or 2-op AM
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
        mode += (GetAdlIns(i0).feedconn & 1) + (GetAdlIns(i1).feedconn & 1) * 2;
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

    if(CartoonersVolumes)
    {
        Poke(card, 0x40+o1, x);
        if(o2 != 0xFFF)
            Poke(card, 0x40+o2, y - volume/2);
    }
    else
    {
        bool do_modulator = do_ops[ mode ][ 0 ] || ScaleModulators;
        bool do_carrier   = do_ops[ mode ][ 1 ] || ScaleModulators;

        Poke(card, 0x40+o1, do_modulator ? (x|63) - volume + volume*(x&63)/63 : x);
        if(o2 != 0xFFF)
            Poke(card, 0x40+o2, do_carrier   ? (y|63) - volume + volume*(y&63)/63 : y);
        //Poke(card, 0x40+o1, do_modulator ? (x|63) - (63-(x&63))*volume/63 : x);
        //if(o2 != 0xFFF)
        //Poke(card, 0x40+o2, do_carrier   ? (y|63) - (63-(y&63))*volume/63 : y);
    }

    // Correct formula (ST3, AdPlug):
    //   63 - ((63-instrvol)/63.0)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

ADLMIDI_EXPORT void OPL3::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
    // Produce a value in 0-63 range
    if(LogarithmicVolumes)
    {
        Touch_Real(c, volume*63/(127*127*127));
    }
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        Touch_Real(c, volume>8725  ? std::log(volume)*11.541561 + (0.5 - 104.22845) : 0);

        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
}

ADLMIDI_EXPORT void OPL3::Patch(unsigned c, unsigned i)
{
    unsigned card = c/23, cc = c%23;
    static const unsigned char data[4] = {0x20,0x60,0x80,0xE0};
    ins[c] = i;
    unsigned o1 = Operators[cc*2+0];
    unsigned o2 = Operators[cc*2+1];

    const adldata& adli = GetAdlIns(i);
    unsigned x = adli.modulator_E862, y = adli.carrier_E862;
    for(unsigned a=0; a<4; ++a, x>>=8, y>>=8)
    {
        Poke(card, data[a]+o1, x&0xFF);
        if(o2 != 0xFFF)
            Poke(card, data[a]+o2, y&0xFF);
    }
}

ADLMIDI_EXPORT void OPL3::Pan(unsigned c, unsigned value)
{
    unsigned card = c/23, cc = c%23;
    if(Channels[cc] != 0xFFF)
        Poke(card, 0xC0 + Channels[cc], GetAdlIns(ins[c]).feedconn | value);
}

ADLMIDI_EXPORT void OPL3::Silence() // Silence all OPL channels.
{
    for(unsigned c=0; c<NumChannels; ++c) { NoteOff(c); Touch_Real(c,0); }
}

ADLMIDI_EXPORT void OPL3::Reset()
{
#ifndef __DJGPP__
    for(unsigned card=0; card<cards.size(); ++card)
    {
        delete cards[card];
        cards[card] = nullptr;
    }
    cards.resize(NumCards);
    for(unsigned card=0; card<cards.size(); ++card)
    {
        cards[card] = new DBOPL::Handler;
    }
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
        cards[card]->Init(PCM_RATE);
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
    if (WritingToTTY)
    {
        UI.PrintLn("Channels used as:");
        std::string s;
        for(size_t a=0; a<four_op_category.size(); ++a)
        {
            s += ' ';
            s += std::to_string(four_op_category[a]);
            if(a%23 == 22) { UI.PrintLn("%s", s.c_str()); s.clear(); }
        }
        if(!s.empty()) { UI.PrintLn("%s", s.c_str()); }
    }
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




ADLMIDI_EXPORT MIDIplay::AdlChannel::AdlChannel(): users(), koff_time_until_neglible(0) { }

ADLMIDI_EXPORT void MIDIplay::AdlChannel::AddAge(long ms)
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

ADLMIDI_EXPORT MIDIplay::MIDIchannel::MIDIchannel()
    : portamento(0),
      bank_lsb(0), bank_msb(0), patch(0),
      volume(100),expression(100),
      panning(0x30), vibrato(0), sustain(0),
      bend(0.0), bendsense(2 / 8192.0),
      vibpos(0), vibspeed(2*3.141592653*5.0),
      vibdepth(0.5/127), vibdelay(0),
      lastlrpn(0),lastmrpn(0),nrpn(false),
      activenotes() { }

ADLMIDI_EXPORT MIDIplay::Position::TrackInfo::TrackInfo(): ptr(0), delay(0), status(0) { }

ADLMIDI_EXPORT MIDIplay::Position::Position(): began(false), wait(0.0), track() { }

ADLMIDI_EXPORT bool MIDIplay::AdlChannel::Location::operator==(const MIDIplay::AdlChannel::Location &b) const
{ return MidCh==b.MidCh && note==b.note; }

ADLMIDI_EXPORT bool MIDIplay::AdlChannel::Location::operator<(const MIDIplay::AdlChannel::Location &b) const
{ return MidCh<b.MidCh || (MidCh==b.MidCh&& note<b.note); }

ADLMIDI_EXPORT unsigned long MIDIplay::ReadBEint(const void *buffer, unsigned nbytes)
{
    unsigned long result=0;
    const unsigned char* data = (const unsigned char*) buffer;
    for(unsigned n=0; n<nbytes; ++n)
        result = (result << 8) + data[n];
    return result;
}

ADLMIDI_EXPORT unsigned long MIDIplay::ReadLEint(const void *buffer, unsigned nbytes)
{
    unsigned long result=0;
    const unsigned char* data = (const unsigned char*) buffer;
    for(unsigned n=0; n<nbytes; ++n)
        result = result + (data[n] << (n*8));
    return result;
}

ADLMIDI_EXPORT unsigned long MIDIplay::ReadVarLen(unsigned tk)
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

ADLMIDI_EXPORT bool MIDIplay::LoadMIDI(const std::string &filename)
{
    std::FILE* fp = std::fopen(filename.c_str(), "rb");
    if(!fp) { std::fprintf(stderr, "\n"); std::perror(filename.c_str()); return false; }
    const unsigned HeaderSize = 4+4+2+2+2; // 14
    char HeaderBuf[HeaderSize]="";
riffskip:;
    std::fread(HeaderBuf, 1, HeaderSize, fp);
    if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
    { std::fseek(fp, 6, SEEK_CUR); goto riffskip; }
    size_t DeltaTicks=192, TrackCount=1;

    bool is_GMF = false; // GMD/MUS files (ScummVM)
    bool is_MUS = false; // MUS/DMX files (Doom)
    bool is_IMF = false; // IMF
    bool is_CMF = false; // Creative Music format (CMF/CTMF)
    bool is_RSXX = false; // RSXX, such as Cartooners
    std::vector<unsigned char> MUS_instrumentList;

    if(std::memcmp(HeaderBuf, "GMF\1", 4) == 0)
    {
        // GMD/MUS files (ScummVM)
        std::fseek(fp, 7-(HeaderSize), SEEK_CUR);
        is_GMF = true;
    }
    else if(std::memcmp(HeaderBuf, "MUS\1x1A", 4) == 0)
    {
        // MUS/DMX files (Doom)
        unsigned start = ReadLEint(HeaderBuf+8, 2);
        is_MUS = true;
        std::fseek(fp, int(start-HeaderSize), SEEK_CUR);
    }
    else if(std::memcmp(HeaderBuf, "CTMF", 4) == 0)
    {
        // Creative Music Format (CMF).
        // When playing CTMF files, use the following commandline:
        // adlmidi song8.ctmf -p -v 1 1 0
        // i.e. enable percussion mode, deeper vibrato, and use only 1 card.

        is_CMF = true;
        //unsigned version   = ReadLEint(HeaderBuf+4, 2);
        unsigned ins_start = ReadLEint(HeaderBuf+6, 2);
        unsigned mus_start = ReadLEint(HeaderBuf+8, 2);
        //unsigned deltas    = ReadLEint(HeaderBuf+10, 2);
        unsigned ticks     = ReadLEint(HeaderBuf+12, 2);
        // Read title, author, remarks start offsets in file
        std::fread(HeaderBuf, 1, 6, fp);
        //unsigned long notes_starts[3] = {ReadLEint(HeaderBuf+0,2),ReadLEint(HeaderBuf+0,4),ReadLEint(HeaderBuf+0,6)};
        std::fseek(fp, 16, SEEK_CUR); // Skip the channels-in-use table
        std::fread(HeaderBuf, 1, 4, fp);
        unsigned ins_count =  ReadLEint(HeaderBuf+0, 2);//, basictempo = ReadLEint(HeaderBuf+2, 2);
        std::fseek(fp, ins_start, SEEK_SET);
        //std::printf("%u instruments\n", ins_count);
        for(unsigned i=0; i<ins_count; ++i)
        {
            unsigned char InsData[16];
            std::fread(InsData, 1, 16, fp);
            /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                    i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                       InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
            struct adldata    adl;
            struct adlinsdata adlins;
            adl.modulator_E862 =
                    (InsData[8] << 24)
                    + (InsData[6] << 16)
                    + (InsData[4] << 8)
                    + (InsData[0] << 0);
            adl.carrier_E862 =
                    (InsData[9] << 24)
                    + (InsData[7] << 16)
                    + (InsData[5] << 8)
                    + (InsData[1] << 0);
            adl.modulator_40 = InsData[2];
            adl.carrier_40   = InsData[3];
            adl.feedconn     = InsData[10];
            adl.finetune = 0;

            adlins.adlno1 = dynamic_instruments.size() | DynamicInstrumentTag;
            adlins.adlno2 = adlins.adlno1;
            adlins.ms_sound_kon  = 1000;
            adlins.ms_sound_koff = 500;
            adlins.tone  = 0;
            adlins.flags = 0;
            dynamic_metainstruments.push_back(adlins);
            dynamic_instruments.push_back(adl);
        }
        std::fseek(fp, mus_start, SEEK_SET);
        TrackCount = 1;
        DeltaTicks = ticks;
        AdlBank    = ~0u; // Ignore AdlBank number, use dynamic banks instead
        //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
        LogarithmicVolumes = true;
    }
    else
    {
        // Try parsing as EA RSXX file
        char rsxxbuf[8];
        {long p = std::ftell(fp);
            std::fseek(fp, HeaderBuf[0]-0x10, SEEK_SET);
            std::fread(rsxxbuf, 6, 1, fp);
            std::fseek(fp, p, SEEK_SET);}
        if(std::memcmp(rsxxbuf, "rsxx}u", 6) == 0)
        {
            is_RSXX = true;
            std::fprintf(stderr, "Detected RSXX format\n");
            std::fseek(fp, HeaderBuf[0], SEEK_SET);
            TrackCount = 1;
            DeltaTicks = 60;
            LogarithmicVolumes = true;
            CartoonersVolumes = true;
        }
        else
        {
            // Try parsing as an IMF file
            if(1)
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
                /*size_t  Fmt =*/ ReadBEint(HeaderBuf+8,  2);
                TrackCount = ReadBEint(HeaderBuf+10, 2);
                DeltaTicks = ReadBEint(HeaderBuf+12, 2);
            }
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
            if(is_GMF || is_CMF || is_RSXX) // Take the rest of the file
            {
                long pos = std::ftell(fp);
                std::fseek(fp, 0, SEEK_END);
                TrackLength = ftell(fp) - pos;
                std::fseek(fp, pos, SEEK_SET);
            }
            else if(is_MUS) // Read TrackLength from file position 4
            {
                long pos = std::ftell(fp);
                std::fseek(fp, 4, SEEK_SET);
                TrackLength = std::fgetc(fp); TrackLength += (std::fgetc(fp) << 8);
                std::fseek(fp, pos, SEEK_SET);
            }
            else // Read MTrk header
            {
                std::fread(HeaderBuf, 1, 8, fp);
                if(std::memcmp(HeaderBuf, "MTrk", 4) != 0) goto InvFmt;
                TrackLength = ReadBEint(HeaderBuf+4, 4);
            }
            // Read track data
            TrackData[tk].resize(TrackLength);
            std::fread(&TrackData[tk][0], 1, TrackLength, fp);
            if(is_GMF || is_MUS) // Note: CMF does include the track end tag.
            {
                TrackData[tk].insert(TrackData[tk].end(), EndTag+0, EndTag+4);
            }
            // Read next event time
            if(!is_RSXX)
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

ADLMIDI_EXPORT double MIDIplay::Tick(double s, double granularity)
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

ADLMIDI_EXPORT void MIDIplay::NoteUpdate(unsigned MidCh, MIDIplay::MIDIchannel::activenoteiterator i, unsigned props_mask, int select_adlchn)
{
    MIDIchannel::NoteInfo& info = i->second;
    const int tone    = info.tone;
    const int vol     = info.vol;
    const int midiins = info.midiins;
    const int insmeta = info.insmeta;
    const adlinsdata& ains = GetAdlMetaIns(insmeta);

    AdlChannel::Location my_loc;
    my_loc.MidCh = MidCh;
    my_loc.note  = i->first;

    for(auto jnext = info.phys.begin(); jnext != info.phys.end(); )
    {
        auto j = jnext++;
        int c   = j->first;
        int ins = j->second;
        if(select_adlchn >= 0 && c != select_adlchn) continue;

        if(props_mask & Upd_Patch)
        {
            opl.Patch(c, ins);
            AdlChannel::LocationData& d = ch[c].users[my_loc];
            d.sustained = false; // inserts if necessary
            d.vibdelay  = 0;
            d.kon_time_until_neglible = ains.ms_sound_kon;
            d.ins       = ins;
        }
    }
    for(auto jnext = info.phys.begin(); jnext != info.phys.end(); )
    {
        auto j = jnext++;
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
                            ains.ms_sound_koff;
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
                double bend = Ch[MidCh].bend + GetAdlIns(ins).finetune;
                double phase = 0.0;

                if((ains.flags & adlinsdata::Flag_Pseudo4op) && ins == ains.adlno2)
                {
                    phase = 0.125; // Detune the note slightly (this is what Doom does)
                }

                //phase -= 12; // hack

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

ADLMIDI_EXPORT void MIDIplay::ProcessEvents()
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

        /* If the -nl commandline option was given, quit now */
        if(QuitWithoutLooping)
        {
            QuitFlag = true;
        }
    }
}

ADLMIDI_EXPORT void MIDIplay::HandleEvent(size_t tk)
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
        if(evtype == 0x51) { Tempo = InvDeltaTicks * fraction<long>( (long) ReadBEint(data.data(), data.size())); return; }
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
        //if(MidCh != 9) note -= 12; // HACK for OpenGL video for changing octaves
        if(CartoonersVolumes && vol != 0)
        {
            // Check if this is just a note after-touch
            auto i = Ch[MidCh].activenotes.find(note);
            if(i != Ch[MidCh].activenotes.end())
            {
                i->second.vol = vol;
                NoteUpdate(MidCh, i, Upd_Volume);
                break;
            }
        }
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

        const unsigned meta    = GetAdlMetaNumber(midiins);
        const adlinsdata& ains = GetAdlMetaIns(meta);

        int tone = note;
        if(ains.tone)
        {
            // 0..19:    add x
            // 20..127:  set x
            // 128..255: subtract x-128
            if(ains.tone < 20)
                tone += ains.tone;
            else if(ains.tone < 128)
                tone = ains.tone;
            else
                tone -= ains.tone-128;
        }
        int i[2] = { ains.adlno1, ains.adlno2 };
        bool pseudo_4op = ains.flags & adlinsdata::Flag_Pseudo4op;

        if(AdlPercussionMode && PercussionMap[midiins & 0xFF]) i[1] = i[0];

        static std::set<unsigned char> missing_warnings;
        if(!missing_warnings.count(midiins) && (ains.flags & adlinsdata::Flag_NoSound))
        {
            UI.PrintLn("[%i]Playing missing instrument %i", MidCh, midiins);
            missing_warnings.insert(midiins);
        }

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
                    {
                        if(cmf_percussion_mode)
                            expected_mode = MidCh < 11 ? 0 : (3+MidCh-11); // CMF
                        else
                            expected_mode = PercussionMap[midiins & 0xFF];
                    }
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
        auto ir = Ch[MidCh].activenotes.insert(std::make_pair(note, MIDIchannel::NoteInfo()));
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
        auto i = Ch[MidCh].activenotes.find(note);
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
            Ch[MidCh].volume     = CartoonersVolumes ? 127 : 100;
            Ch[MidCh].expression = CartoonersVolumes ? 127 : 100;
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

        case 103: cmf_percussion_mode = value; break; // CMF (ctrl 0x67) rhythm mode
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
        // Set this pressure to all active notes on the channel
        for(auto& ch: Ch[MidCh].activenotes) { ch.second.vol = vol; }
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

ADLMIDI_EXPORT long MIDIplay::CalculateAdlChannelGoodness(unsigned c, unsigned ins, unsigned) const
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

ADLMIDI_EXPORT void MIDIplay::PrepareAdlChannelForNewNote(int c, int ins)
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

ADLMIDI_EXPORT
void MIDIplay::KillOrEvacuate(unsigned from_channel,
                              AdlChannel::users_t::iterator j,
                              MIDIplay::MIDIchannel::activenoteiterator i)
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

ADLMIDI_EXPORT void MIDIplay::KillSustainingNotes(int MidCh, int this_adlchn)
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

ADLMIDI_EXPORT void MIDIplay::SetRPN(unsigned MidCh, unsigned value, bool MSB)
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

ADLMIDI_EXPORT void MIDIplay::UpdatePortamento(unsigned MidCh)
{
    // mt = 2^(portamento/2048) * (1.0 / 5000.0)
    /*
        double mt = std::exp(0.00033845077 * Ch[MidCh].portamento);
        NoteUpdate_All(MidCh, Upd_Pitch);
        */
    UI.PrintLn("Portamento %u: %u (unimplemented)", MidCh, Ch[MidCh].portamento);
}

ADLMIDI_EXPORT void MIDIplay::NoteUpdate_All(unsigned MidCh, unsigned props_mask)
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

ADLMIDI_EXPORT void MIDIplay::NoteOff(unsigned MidCh, int note)
{
    MIDIchannel::activenoteiterator
            i = Ch[MidCh].activenotes.find(note);
    if(i != Ch[MidCh].activenotes.end())
    {
        NoteUpdate(MidCh, i, Upd_Off);
    }
}

ADLMIDI_EXPORT void MIDIplay::UpdateArpeggio(double) // amount = amount of time passed
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

ADLMIDI_EXPORT void MIDIplay::Generate(int card,
                        void (*AddSamples_m32)(unsigned long, int32_t *),
                        void (*AddSamples_s32)(unsigned long, int32_t *),
                        unsigned long samples)
{
    opl.cards[card]->Generate(AddSamples_m32, AddSamples_s32, samples);
}

ADLMIDI_EXPORT unsigned MIDIplay::ChooseDevice(const std::string &name)
{
    std::map<std::string, unsigned>::iterator i = devices.find(name);
    if(i != devices.end()) return i->second;
    size_t n = devices.size() * 16;
    devices.insert( std::make_pair(name, n) );
    Ch.resize(n+16);
    return n;
}





ADLMIDI_EXPORT Tester::Tester(OPL3 &o) : opl(o)
{
    cur_gm   = 0;
    ins_idx  = 0;
}

ADLMIDI_EXPORT Tester::~Tester()
{
}

ADLMIDI_EXPORT void Tester::FindAdlList()
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

ADLMIDI_EXPORT void Tester::DoNote(int note)
{
    if(adl_ins_list.empty()) FindAdlList();
    const unsigned meta = adl_ins_list[ins_idx];
    const adlinsdata& ains = GetAdlMetaIns(meta);

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

ADLMIDI_EXPORT void Tester::NextGM(int offset)
{
    cur_gm = (cur_gm + 256 + offset) & 0xFF;
    FindAdlList();
}

ADLMIDI_EXPORT void Tester::NextAdl(int offset)
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
        const unsigned i = adl_ins_list[a];
        const adlinsdata& ains = GetAdlMetaIns(i);

        char ToneIndication[8] = "   ";
        if(ains.tone)
        {
            if(ains.tone < 20)
                sprintf(ToneIndication, "+%-2d", ains.tone);
            else if(ains.tone < 128)
                sprintf(ToneIndication, "=%-2d", ains.tone);
            else
                sprintf(ToneIndication, "-%-2d", ains.tone-128);
        }
        std::printf("%s%s%s%u\t",
                    ToneIndication,
                    ains.adlno1 != ains.adlno2 ? "[2]" : "   ",
                    (ins_idx == a) ? "->" : "\t",
                    i
                    );

        for(unsigned bankno=0; bankno<NumBanks; ++bankno)
            if(banks[bankno][cur_gm] == i)
                std::printf(" %u", bankno);

        std::printf("\n");
    }
}

ADLMIDI_EXPORT void Tester::HandleInputChar(char ch)
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

ADLMIDI_EXPORT double Tester::Tick(double, double)
{
    HandleInputChar( Input.PeekInput() );
    //return eat_delay;
    return 0.1;
}
