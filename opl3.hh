#include "dbopl.h"

static void AddMonoAudio(unsigned long count, int* samples);
static void AddStereoAudio(unsigned long count, int* samples);

struct OPL3
{
    static const unsigned long PCM_RATE = 48000;

    unsigned char ins[18], pit[18];
    DBOPL::Handler dbopl_handler;

    struct OPLparams // Set up OPL parameters (In: c = channel (0..17))
    {
        unsigned port; // I/O port
        unsigned chan; // Per-OPL channel
        unsigned oper; // Operator offset
        inline OPLparams(unsigned c)
            : port(0x388 + 2*(c/9)), chan(c%9),
              oper(chan%3 + 8 * (chan/3) ) { }
    };
    void Poke(unsigned port, unsigned index, unsigned value)
    {
        /*dbopl_handler.WriteAddr(port+0, index);
          dbopl_handler.WriteAddr(port+1, value);*/
        if(port&2)
            dbopl_handler.WriteReg(index + 0x100, value);
        else
            dbopl_handler.WriteReg(index + 0x000, value);
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
        OPLparams p(c);
        Poke(p.port, 0xB0 + p.chan, pit[c] & 0xDF);
    }
    void NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
    {
        OPLparams p(c);
        unsigned x = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x += 0x400; } // Calculate octave
        x += (int)(hertz + 0.5);
        Poke(p.port, 0xA0 + p.chan, x & 0xFF);
        Poke(p.port, 0xB0 + p.chan, pit[c] = x >> 8);
    }
    void Touch_Real(unsigned c, unsigned volume)
    {
        OPLparams p(c);
        unsigned i = ins[c];
        {unsigned x = adl[i][2];
        Poke(p.port, 0x40 + p.oper, (x | 63) - volume + ((x & 63) * volume) / 63);}
        {unsigned x = adl[i][3];
        Poke(p.port, 0x43 + p.oper, (x | 63) - volume + ((x & 63) * volume) / 63);}
    }
    void Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63) / 8), A)
        Touch_Real(c, volume<=9112 ? 0 : std::log(volume)*11.541561 + (0.5 - 104.72843));
    }
    void Patch(unsigned c, unsigned i)
    {
        OPLparams p(c);
        ins[c] = i;
        for(unsigned x=0; x<=1; ++x)
          { Poke(p.port, 0x20 + p.oper + x*3, adl[i][0+x]);
            Poke(p.port, 0x60 + p.oper + x*3, adl[i][4+x]);
            Poke(p.port, 0x80 + p.oper + x*3, adl[i][6+x]);
            Poke(p.port, 0xE0 + p.oper + x*3, adl[i][8+x]); }
    }
    void Pan(unsigned c, unsigned value)
    {
        OPLparams p(c);
        Poke(p.port, 0xC0 + p.chan, adl[ins[c]][10] - value);
    }
    void Silence() // Silence all OPL channels.
    {
        for(unsigned c=0; c<18; ++c) { NoteOff(c); Touch_Real(c, 0); }
    }
    void Reset()
    {
        dbopl_handler.Init(PCM_RATE);
        for(unsigned a=0; a<18; ++a) ins[a] = pit[a] = 0;
        // Detect OPL3 (send pulses to timer, opl3 enable register)
        for(unsigned y=3; y<=4; ++y) Poke(OPLparams(0).port, 4, y*32);
        for(unsigned y=0; y<=2; ++y) Poke(OPLparams(9).port, 5, y&1);
        // Reset OPL3 (turn off all notes, set vital settings)
        Poke(OPLparams(0).port,    1, 32); // Enable wave
        Poke(OPLparams(0).port, 0xBD,  0); // Set melodic mode
        Poke(OPLparams(9).port,    5,  1); // Enable OPL3
        Poke(OPLparams(9).port,    4,  0); // Select mode 0 (also try 63 for fun!)
        Silence();
    }
};
