#ifndef ADLPLAY_H
#define ADLPLAY_H

#include <vector>
#include <map>

#include "fraction.hpp"

namespace DBOPL
{
struct Handler;
}

struct OPL3
{
    unsigned NumChannels;

#ifndef __DJGPP__
    std::vector<DBOPL::Handler*> cards;
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

    void Poke(unsigned card, unsigned index, unsigned value);
    void NoteOff(unsigned c);
    void NoteOn(unsigned c, double hertz);
    void Touch_Real(unsigned c, unsigned volume);
    void Touch(unsigned c, unsigned volume);
    void Patch(unsigned c, unsigned i);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void Reset();
};



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

            TrackInfo();
        };
        std::vector<TrackInfo> track;

        Position();
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
            short tone;
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

        MIDIchannel();
    };
    std::vector<MIDIchannel> Ch;
    bool cmf_percussion_mode = false;

    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For collisions
        struct Location
        {
            unsigned short MidCh;
            unsigned char  note;
            bool operator==(const Location&b) const;
            bool operator< (const Location&b) const;
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
        AdlChannel();

        void AddAge(long ms);
    };
    std::vector<AdlChannel> ch;

    std::vector< std::vector<unsigned char> > TrackData;
public:
    fraction<long> InvDeltaTicks, Tempo;
    bool loopStart, loopEnd;
    OPL3 opl;
public:
    static unsigned long ReadBEint(const void* buffer, unsigned nbytes);
    static unsigned long ReadLEint(const void* buffer, unsigned nbytes);
    unsigned long ReadVarLen(unsigned tk);

    bool LoadMIDI(const std::string& filename);

    /* Periodic tick handler.
     *   Input: s           = seconds since last call
     *   Input: granularity = don't expect intervals smaller than this, in seconds
     *   Output: desired number of seconds until next call
     */
    double Tick(double s, double granularity);

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
         int select_adlchn = -1);

    void ProcessEvents();
    void HandleEvent(size_t tk);

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    long CalculateAdlChannelGoodness
        (unsigned c, unsigned ins, unsigned /*MidCh*/) const;

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(int c, int ins);

    void KillOrEvacuate(
        unsigned from_channel,
        AdlChannel::users_t::iterator j,
        MIDIchannel::activenoteiterator i);

    void KillSustainingNotes(int MidCh = -1, int this_adlchn = -1);

    void SetRPN(unsigned MidCh, unsigned value, bool MSB);

    void UpdatePortamento(unsigned MidCh);

    void NoteUpdate_All(unsigned MidCh, unsigned props_mask);

    void NoteOff(unsigned MidCh, int note);

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

    void UpdateArpeggio(double /*amount*/);

public:
    unsigned ChooseDevice(const std::string& name);

    void Generate(int card,
                  void(*AddSamples_m32)(unsigned long,int32_t*),
                  void(*AddSamples_s32)(unsigned long,int32_t*),
                  unsigned long samples);
};



class Tester
{
    unsigned cur_gm;
    unsigned ins_idx;
    std::vector<unsigned> adl_ins_list;
    OPL3& opl;
public:
    Tester(OPL3& o);
    ~Tester();

    // Find list of adlib instruments that supposedly implement this GM
    void FindAdlList();
    void DoNote(int note);
    void NextGM(int offset);
    void NextAdl(int offset);
    void HandleInputChar(char ch);
    double Tick(double /*eat_delay*/, double /*mindelay*/);
};


#endif // ADLPLAY_H
