#include "dbopl.h"

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
        // The formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
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
        for(unsigned a=0; a<18; ++a) { ins[a] = 198; pit[a] = 0; }
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
