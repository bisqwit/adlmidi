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
        double        bend, bendsense;
        unsigned char lastlrpn,lastmrpn; bool nrpn;
        struct NoteInfo
        {
            signed char adlchn; // adlib channel
            unsigned char  vol; // pressure
            unsigned short ins; // instrument selected on noteon
        };
        typedef std::map<unsigned char,NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        activenotemap_t activenotes;

        MIDIchannel()
            : bank(0), portamento(0), patch(0),
              volume(100),expression(100),
              panning(0), vibrato(0), sustain(0),
              bend(0.0), bendsense(2 / 8192.0),
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
        long   age;
        AdlChannel(): midichn(0),note(0), state(off),age(0) { }
    } ch[18];

    std::vector< std::vector<unsigned char> > TrackData;
public:
    double InvDeltaTicks, Tempo;
    double VibPos;
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
        std::fread(HeaderBuf, 1, 4+4+2+2+2, fp);
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
        VibPos        = 0.0;
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

        bool had_vibrato = false;
        for(unsigned a=0; a<16; ++a)
            if(Ch[a].vibrato && !Ch[a].activenotes.empty()) 
            {
                had_vibrato = true;
                NoteUpdate_All(a, Upd_Pitch);
            }
        if(had_vibrato)
        {
            const double twopi = 2*3.141592653;
            VibPos = std::fmod(VibPos + s * (twopi * 5), twopi);
        }
        else
            VibPos = 0.0;

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
        int c = i->second.adlchn, tone = i->first, ins = i->second.ins;
        if(MidCh == 9) tone = adl[ins][11]; // Percussion always uses constant tone
        // (MIDI channel 9 always plays percussion and ignores the patch number)

        ch[c].age = 0;
        if(props_mask & Upd_Off) // note off
        {
            if(Ch[MidCh].sustain == 0)
            {
                opl.NoteOff(c);
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
            if(Ch[MidCh].vibrato)
                bend += (Ch[MidCh].vibrato * (0.5/127)) * std::sin(VibPos);
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
        for(unsigned a=0; a<18; ++a) ch[a].age += t*1000;

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
                if(MidCh == 9)
                    i = 128 + note - 35;
                for(unsigned a = 0; a < 18; ++a)
                {
                    long s = ch[a].age;   // Age in seconds = better score
                    switch(ch[a].state)
                    {
                        case AdlChannel::off:
                            s += 3000000; // Empty channel = privileged
                            break;
                        case AdlChannel::sustained:
                            s += 100000;  // Sustained = free but deferred
                            break;
                        default: break;
                    }
                    if(i == opl.ins[a]) s += 20;  // Same instrument = good
                    if(a == MidCh) s += 20;
                    if(i<128 && opl.ins[a]>127)
                        s=s*2+9000;   // Percussion is inferior to melody
                    else if(opl.ins[a]<128 && i>127)
                        s=(s-9000)/2; // Percussion is inferior to melody
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
                        Ch[MidCh].panning = (value<64-32)?32:((value>=64+32)?16:0);
                        NoteUpdate_All(MidCh, Upd_Pan);
                        break;
                    case 121: // Reset all controllers
                        Ch[MidCh].bend       = 0;
                        Ch[MidCh].volume     = 100;
                        Ch[MidCh].expression = 100;
                        Ch[MidCh].sustain    = 0;
                        Ch[MidCh].vibrato    = 0;
                        Ch[MidCh].panning    = 0;
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
            case 0x20000: Ch[MidCh].bendsense = value/8192.0; break;
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
