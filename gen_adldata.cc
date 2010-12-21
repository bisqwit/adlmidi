#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <map>
#include <set>

std::map<unsigned,
    std::map<unsigned, unsigned>
        > Correlate;
unsigned maxvalues[30] = { 0 };

static const char *const MidiInsName[] = {
"AcouGrandPiano",
"BrightAcouGrand",
"ElecGrandPiano",
"Honky-tonkPiano",
"Rhodes Piano",
"Chorused Piano",
"Harpsichord",
"Clavinet",
"Celesta",
"Glockenspiel",
"Music box",
"Vibraphone",
"Marimba",
"Xylophone",
"Tubular Bells",
"Dulcimer",
"Hammond Organ",
"Percussive Organ",
"Rock Organ",
"Church Organ",
"Reed Organ",
"Accordion",
"Harmonica",
"Tango Accordion",
"Acoustic Guitar1",
"Acoustic Guitar2",
"Electric Guitar1",
"Electric Guitar2",
"Electric Guitar3",
"Overdrive Guitar",
"Distorton Guitar",
"Guitar Harmonics",
"Acoustic Bass",
"Electric Bass 1",
"Electric Bass 2",
"Fretless Bass",
"Slap Bass 1",
"Slap Bass 2",
"Synth Bass 1",
"Synth Bass 2",
"Violin",
"Viola",
"Cello",
"Contrabass",
"Tremulo Strings",
"Pizzicato String",
"Orchestral Harp",
"Timpany",
"String Ensemble1",
"String Ensemble2",
"Synth Strings 1",
"SynthStrings 2",
"Choir Aahs",
"Voice Oohs",
"Synth Voice",
"Orchestra Hit",
"Trumpet",
"Trombone",
"Tuba",
"Muted Trumpet",
"French Horn",
"Brass Section",
"Synth Brass 1",
"Synth Brass 2",
"Soprano Sax",
"Alto Sax",
"Tenor Sax",
"Baritone Sax",
"Oboe",
"English Horn",
"Bassoon",
"Clarinet",
"Piccolo",
"Flute",
"Recorder",
"Pan Flute",
"Bottle Blow",
"Shakuhachi",
"Whistle",
"Ocarina",
"Lead 1 squareea",
"Lead 2 sawtooth",
"Lead 3 calliope",
"Lead 4 chiff",
"Lead 5 charang",
"Lead 6 voice",
"Lead 7 fifths",
"Lead 8 brass",
"Pad 1 new age",
"Pad 2 warm",
"Pad 3 polysynth",
"Pad 4 choir",
"Pad 5 bowedpad",
"Pad 6 metallic",
"Pad 7 halo",
"Pad 8 sweep",
"FX 1 rain",
"FX 2 soundtrack",
"FX 3 crystal",
"FX 4 atmosphere",
"FX 5 brightness",
"FX 6 goblins",
"FX 7 echoes",
"FX 8 sci-fi",
"Sitar",
"Banjo",
"Shamisen",
"Koto",
"Kalimba",
"Bagpipe",
"Fiddle",
"Shanai",
"Tinkle Bell",
"Agogo Bells",
"Steel Drums",
"Woodblock",
"Taiko Drum",
"Melodic Tom",
"Synth Drum",
"Reverse Cymbal",
"Guitar FretNoise",
"Breath Noise",
"Seashore",
"Bird Tweet",
"Telephone",
"Helicopter",
"Applause/Noise",
"Gunshot",
// 27..34:  High Q; Slap; Scratch Push; Scratch Pull; Sticks;
//          Square Click; Metronome Click; Metronome Bell
"Ac Bass Drum",
"Bass Drum 1",
"Side Stick",
"Acoustic Snare",
"Hand Clap",
"Electric Snare",
"Low Floor Tom",
"Closed High Hat",
"High Floor Tom",
"Pedal High Hat",
"Low Tom",
"Open High Hat",
"Low-Mid Tom",
"High-Mid Tom",
"Crash Cymbal 1",
"High Tom",
"Ride Cymbal 1",
"Chinese Cymbal",
"Ride Bell",
"Tambourine",
"Splash Cymbal",
"Cow Bell",
"Crash Cymbal 2",
"Vibraslap",
"Ride Cymbal 2",
"High Bongo",
"Low Bongo",
"Mute High Conga",
"Open High Conga",
"Low Conga",
"High Timbale",
"Low Timbale",
"High Agogo",
"Low Agogo",
"Cabasa",
"Maracas",
"Short Whistle",
"Long Whistle",
"Short Guiro",
"Long Guiro",
"Claves",
"High Wood Block",
"Low Wood Block",
"Mute Cuica",
"Open Cuica",
"Mute Triangle",
"Open Triangle",
"Shaker","Jingle Bell","Bell Tree","Castanets","Mute Surdu","Open Surdu",""};
static const unsigned char adl[181][14] =
{
// The data bytes are:
//  [0,1] AM/VIB/EG/KSR/Multiple bits for carrier and modulator respectively
//  [2,3] Attack and decay rates      for carrier and modulator respectively
//  [4,5] Sustain and release rates   for carrier and modulator respectively
//  [6,7] Wave select settings        for carrier and modulator respectively
//  [8,9] KSL/Attenuation settings    for carrier and modulator respectively
//  [10]  Feedback/connection bits    for the channel
//  [11]  For percussive instruments (GP35..GP87), the tone to play
//  [12]  Drawing symbol
    {   1,  1,242,242,244,247,0,0,143,  6, 8,  0,'P',0x04}, // GM1:AcouGrandPiano
    {   1,  1,242,242,244,247,0,0, 75,  0, 8,  0,'P',0x04}, // GM2:BrightAcouGrand
    {   1,  1,242,242,244,246,0,0, 73,  0, 8,  0,'P',0x04}, // GM3:ElecGrandPiano
    { 129, 65,242,242,247,247,0,0, 18,  0, 6,  0,'P',0x04}, // GM4:Honky-tonkPiano
    {   1,  1,241,242,247,247,0,0, 87,  0, 0,  0,'P',0x04}, // GM5:Rhodes Piano
    {   1,  1,241,242,247,247,0,0,147,  0, 0,  0,'P',0x04}, // GM6:Chorused Piano
    {   1, 22,161,242,242,245,0,0,128, 14, 8,  0,'h',0x04}, // GM7:Harpsichord
    {   1,  1,194,194,248,248,0,0,146,  0,10,  0,'c',0x04}, // GM8:Clavinet
    {  12,129,246,243,244,245,0,0, 92,  0, 0,  0,'c',0x04}, // GM9:Celesta
    {   7, 17,243,242,242,241,0,0,151,128, 2,  0,'k',0x04}, // GM10:Glockenspiel
    {  23,  1, 84,244,244,244,0,0, 33,  0, 2,  0,'m',0x04}, // GM11:Music box
    { 152,129,243,242,246,246,0,0, 98,  0, 0,  0,'v',0x04}, // GM12:Vibraphone
    {  24,  1,246,231,246,247,0,0, 35,  0, 0,  0,'m',0x04}, // GM13:Marimba
    {  21,  1,246,246,246,246,0,0,145,  0, 4,  0,'x',0x04}, // GM14:Xylophone
    {  69,129,211,163,243,243,0,0, 89,128,12,  0,'b',0x04}, // GM15:Tubular Bells
    {   3,129,117,181,245,245,1,0, 73,128, 4,  0,'d',0x04}, // GM16:Dulcimer
    { 113, 49,246,241, 20,  7,0,0,146,  0, 2,  0,'o',0x04}, // GM17:Hammond Organ
    { 114, 48,199,199, 88,  8,0,0, 20,  0, 2,  0,'o',0x04}, // GM18:Percussive Organ
    { 112,177,170,138, 24,  8,0,0, 68,  0, 4,  0,'o',0x62}, // GM19:Rock Organ
    {  35,177,151, 85, 35, 20,1,0,147,  0, 4,  0,'o',0x04}, // GM20:Church Organ
    {  97,177,151, 85,  4,  4,1,0, 19,128, 0,  0,'o',0x62}, // GM21:Reed Organ
    {  36,177,152, 70, 42, 26,1,0, 72,  0,12,  0,'a',0x0C}, // GM22:Accordion
    {  97, 33,145, 97,  6,  7,1,0, 19,  0,10,  0,'h',0x04}, // GM23:Harmonica
    {  33,161,113, 97,  6,  7,0,0, 19,137, 6,  0,'o',0x04}, // GM24:Tango Accordion
    {   2, 65,243,243,148,200,1,0,156,128,12,  0,'G',0x04}, // GM25:Acoustic Guitar1
    {   3, 17,243,241,154,231,1,0, 84,  0,12,  0,'G',0x04}, // GM26:Acoustic Guitar2
    {  35, 33,241,242, 58,248,0,0, 95,  0, 0,  0,'G',0x04}, // GM27:Electric Guitar1
    {   3, 33,246,243, 34,248,1,0,135,128, 6,  0,'G',0x04}, // GM28:Electric Guitar2
    {   3, 33,249,246, 84, 58,0,0, 71,  0, 0,  0,'G',0x00}, // GM29:Electric Guitar3
    {  35, 33,145,132, 65, 25,1,0, 74,  5, 8,  0,'G',0x04}, // GM30:Overdrive Guitar
    {  35, 33,149,148, 25, 25,1,0, 74,  0, 8,  0,'G',0x04}, // GM31:Distorton Guitar
    {   9,132, 32,209, 79,248,0,0,161,128, 8,  0,'G',0xEE}, // GM32:Guitar Harmonics
    {  33,162,148,195,  6,166,0,0, 30,  0, 2,  0,'B',0xEE}, // GM33:Acoustic Bass
    {  49, 49,241,241, 40, 24,0,0, 18,  0,10,  0,'B',0x0C}, // GM34:Electric Bass 1
    {  49, 49,241,241,232,120,0,0,141,  0,10,  0,'B',0x04}, // GM35:Electric Bass 2
    {  49, 50, 81,113, 40, 72,0,0, 91,  0,12,  0,'B',0x04}, // GM36:Fretless Bass
    {   1, 33,161,242,154,223,0,0,139, 64, 8,  0,'B',0x04}, // GM37:Slap Bass 1
    {  33, 33,162,161, 22,223,0,0,139,  8, 8,  0,'B',0x04}, // GM38:Slap Bass 2
    {  49, 49,244,241,232,120,0,0,139,  0,10,  0,'B',0x04}, // GM39:Synth Bass 1
    {  49, 49,241,241, 40, 24,0,0, 18,  0,10,  0,'B',0x04}, // GM40:Synth Bass 2
    {  49, 33,221, 86, 19, 38,1,0, 21,  0, 8,  0,'V',0x0C}, // GM41:Violin
    {  49, 33,221,102, 19,  6,1,0, 22,  0, 8,  0,'V',0x04}, // GM42:Viola
    { 113, 49,209, 97, 28, 12,1,0, 73,  0, 8,  0,'V',0x04}, // GM43:Cello
    {  33, 35,113,114, 18,  6,1,0, 77,128, 2,  0,'V',0x04}, // GM44:Contrabass
    { 241,225,241,111, 33, 22,1,0, 64,  0, 2,  0,'V',0x04}, // GM45:Tremulo Strings
    {   2,  1,245,133,117, 53,1,0, 26,128, 0,  0,'H',0x3D}, // GM46:Pizzicato String
    {   2,  1,245,243,117,244,1,0, 29,128, 0,  0,'H',0x0C}, // GM47:Orchestral Harp
    {  16, 17,245,242,  5,195,1,0, 65,  0, 2,  0,'M',0x04}, // GM48:Timpany
    {  33,162,177,114, 37,  8,1,0,155,  1,14,  0,'S',0x04}, // GM49:String Ensemble1
    { 161, 33,127, 63,  3,  7,1,1,152,  0, 0,  0,'S',0x00}, // GM50:String Ensemble2
    { 161, 97,193, 79, 18,  5,0,0,147,  0,10,  0,'S',0x04}, // GM51:Synth Strings 1
    {  33, 97,193, 79, 34,  5,0,0, 24,  0,12,  0,'S',0x04}, // GM52:SynthStrings 2
    {  49,114,244,138, 21,  5,0,0, 91,131, 0,  0,'O',0x04}, // GM53:Choir Aahs
    { 161, 97,116,113, 57,103,0,0,144,  0, 0,  0,'O',0x04}, // GM54:Voice Oohs
    { 113,114, 84,122,  5,  5,0,0, 87,  0,12,  0,'O',0x62}, // GM55:Synth Voice
    { 144, 65, 84,165, 99, 69,0,0,  0,  0, 8,  0,'c',0x04}, // GM56:Orchestra Hit
    {  33, 33,133,143, 23,  9,0,0,146,  1,12,  0,'T',0x04}, // GM57:Trumpet
    {  33, 33,117,143, 23,  9,0,0,148,  5,12,  0,'T',0x04}, // GM58:Trombone
    {  33, 97,118,130, 21, 55,0,0,148,  0,12,  0,'T',0x00}, // GM59:Tuba
    {  49, 33,158, 98, 23, 44,1,1, 67,  0, 2,  0,'T',0x62}, // GM60:Muted Trumpet
    {  33, 33, 97,127,106, 10,0,0,155,  0, 2,  0,'T',0x62}, // GM61:French Horn
    {  97, 34,117,116, 31, 15,0,0,138,  6, 8,  0,'T',0x04}, // GM62:Brass Section
    { 161, 33,114,113, 85, 24,1,0,134,131, 0,  0,'T',0x04}, // GM63:Synth Brass 1
    {  33, 33, 84,166, 60, 28,0,0, 77,  0, 8,  0,'T',0x62}, // GM64:Synth Brass 2
    {  49, 97,147,114,  2, 11,1,0,143,  0, 8,  0,'X',0x04}, // GM65:Soprano Sax
    {  49, 97,147,114,  3,  9,1,0,142,  0, 8,  0,'X',0x00}, // GM66:Alto Sax
    {  49, 97,147,130,  3,  9,1,0,145,  0,10,  0,'X',0x00}, // GM67:Tenor Sax
    {  49, 97,147,114, 15, 15,1,0,142,  0,10,  0,'X',0x62}, // GM68:Baritone Sax
    {  33, 33,170,143, 22, 10,1,0, 75,  0, 8,  0,'T',0x04}, // GM69:Oboe
    {  49, 33,126,139, 23, 12,1,1,144,  0, 6,  0,'T',0x04}, // GM70:English Horn
    {  49, 50,117, 97, 25, 25,1,0,129,  0, 0,  0,'T',0x04}, // GM71:Bassoon
    {  50, 33,155,114, 33, 23,0,0,144,  0, 4,  0,'F',0x62}, // GM72:Clarinet
    { 225,225,133,101, 95, 26,0,0, 31,  0, 0,  0,'F',0x04}, // GM73:Piccolo
    { 225,225,136,101, 95, 26,0,0, 70,  0, 0,  0,'F',0x3D}, // GM74:Flute
    { 161, 33,117,117, 31, 10,0,0,156,  0, 2,  0,'F',0x62}, // GM75:Recorder
    {  49, 33,132,101, 88, 26,0,0,139,  0, 0,  0,'F',0x04}, // GM76:Pan Flute
    { 225,161,102,101, 86, 38,0,0, 76,  0, 0,  0,'F',0x04}, // GM77:Bottle Blow
    {  98,161,118, 85, 70, 54,0,0,203,  0, 0,  0,'F',0x04}, // GM78:Shakuhachi
    {  98,161, 87, 86,  7,  7,0,0,153,  0,11,  0,'F',0x04}, // GM79:Whistle
    {  98,161,119,118,  7,  7,0,0,147,  0,11,  0,'F',0x04}, // GM80:Ocarina
    {  34, 33,255,255,  3, 15,2,0, 89,  0, 0,  0,'L',0x04}, // GM81:Lead 1 squareea
    {  33, 33,255,255, 15, 15,1,1, 14,  0, 0,  0,'L',0xF4}, // GM82:Lead 2 sawtooth
    {  34, 33,134,100, 85, 24,0,0, 70,128, 0,  0,'L',0x04}, // GM83:Lead 3 calliope
    {  33,161,102,150, 18, 10,0,0, 69,  0, 0,  0,'L',0x04}, // GM84:Lead 4 chiff
    {  33, 34,146,145, 42, 42,1,0,139,  0, 0,  0,'L',0x04}, // GM85:Lead 5 charang
    { 162, 97,223,111,  5,  7,0,0,158, 64, 2,  0,'L',0x04}, // GM86:Lead 6 voice
    {  32, 96,239,143,  1,  6,0,2, 26,  0, 0,  0,'L',0x04}, // GM87:Lead 7 fifths
    {  33, 33,241,244, 41,  9,0,0,143,128,10,  0,'L',0x04}, // GM88:Lead 8 brass
    { 119,161, 83,160,148,  5,0,0,165,  0, 2,  0,'p',0x04}, // GM89:Pad 1 new age
    {  97,177,168, 37, 17,  3,0,0, 31,128,10,  0,'p',0x04}, // GM90:Pad 2 warm
    {  97, 97,145, 85, 52, 22,0,0, 23,  0,12,  0,'p',0x04}, // GM91:Pad 3 polysynth
    { 113,114, 84,106,  1,  3,0,0, 93,  0, 0,  0,'p',0x04}, // GM92:Pad 4 choir
    {  33,162, 33, 66, 67, 53,0,0,151,  0, 8,  0,'p',0x04}, // GM93:Pad 5 bowedpad
    { 161, 33,161, 49,119, 71,1,1, 28,  0, 0,  0,'p',0x04}, // GM94:Pad 6 metallic
    {  33, 97, 17, 66, 51, 37,0,0,137,  3,10,  0,'p',0x04}, // GM95:Pad 7 halo
    { 161, 33, 17,207, 71,  7,1,0, 21,  0, 0,  0,'p',0x04}, // GM96:Pad 8 sweep
    {  58, 81,248,134,246,  2,0,0,206,  0, 2,  0,'X',0x04}, // GM97:FX 1 rain
    {  33, 33, 33, 65, 35, 19,1,0, 21,  0, 0,  0,'X',0x04}, // GM98:FX 2 soundtrack
    {   6,  1,116,165,149,114,0,0, 91,  0, 0,  0,'X',0xF4}, // GM99:FX 3 crystal
    {  34, 97,177,242,129, 38,0,0,146,131,12,  0,'X',0x04}, // GM100:FX 4 atmosphere
    {  65, 66,241,242, 81,245,1,0, 77,  0, 0,  0,'X',0xF4}, // GM101:FX 5 brightness
    {  97,163, 17, 17, 81, 19,1,0,148,128, 6,  0,'X',0x04}, // GM102:FX 6 goblins
    {  97,161, 17, 29, 49,  3,0,0,140,128, 6,  0,'X',0x3D}, // GM103:FX 7 echoes
    { 164, 97,243,129,115, 35,1,0, 76,  0, 4,  0,'X',0x04}, // GM104:FX 8 sci-fi
    {   2,  7,210,242, 83,246,0,1,133,  3, 0,  0,'G',0x04}, // GM105:Sitar
    {  17, 19,163,162, 17,229,1,0, 12,128, 0,  0,'G',0x04}, // GM106:Banjo
    {  17, 17,246,242, 65,230,1,2,  6,  0, 4,  0,'G',0x04}, // GM107:Shamisen
    { 147,145,212,235, 50, 17,0,1,145,  0, 8,  0,'G',0x04}, // GM108:Koto
    {   4,  1,250,194, 86,  5,0,0, 79,  0,12,  0,'G',0x04}, // GM109:Kalimba
    {  33, 34,124,111, 32, 12,0,1, 73,  0, 6,  0,'T',0xF4}, // GM110:Bagpipe
    {  49, 33,221, 86, 51, 22,1,0,133,  0,10,  0,'S',0xF4}, // GM111:Fiddle
    {  32, 33,218,143,  5, 11,2,0,  4,129, 6,  0,'S',0x04}, // GM112:Shanai
    {   5,  3,241,195,229,229,0,0,106,128, 6,  0,'b',0x04}, // GM113:Tinkle Bell
    {   7,  2,236,248, 38, 22,0,0, 21,  0,10,  0,'b',0x04}, // GM114:Agogo Bells
    {   5,  1,103,223, 53,  5,0,0,157,  0, 8,  0,'b',0x04}, // GM115:Steel Drums
    {  24, 18,250,248, 40,229,0,0,150,  0,10,  0,'b',0x04}, // GM116:Woodblock
    {  16,  0,168,250,  7,  3,0,0,134,  3, 6,  0,'M',0x04}, // GM117:Taiko Drum
    {  17, 16,248,243, 71,  3,2,0, 65,  3, 4,  0,'M',0x04}, // GM118:Melodic Tom
    {   1, 16,241,243,  6,  2,2,0,142,  0,14,  0,'M',0x62}, // GM119:Synth Drum
    {  14,192, 31, 31,  0,255,0,3,  0,  0,14,  0,'c',0xF4}, // GM120:Reverse Cymbal
    {   6,  3,248, 86, 36,132,0,2,128,136,14,  0,'G',0x04}, // GM121:Guitar FretNoise
    {  14,208,248, 52,  0,  4,0,3,  0,  5,14,  0,'X',0x04}, // GM122:Breath Noise
    {  14,192,246, 31,  0,  2,0,3,  0,  0,14,  0,'X',0x62}, // GM123:Seashore
    { 213,218, 55, 86,163, 55,0,0,149, 64, 0,  0,'X',0x0C}, // GM124:Bird Tweet
    {  53, 20,178,244, 97, 21,2,0, 92,  8,10,  0,'X',0x04}, // GM125:Telephone
    {  14,208,246, 79,  0,245,0,3,  0,  0,14,  0,'X',0x62}, // GM126:Helicopter
    {  38,228,255, 18,  1, 22,0,1,  0,  0,14,  0,'X',0x04}, // GM127:Applause/Noise
    {   0,  0,243,246,240,201,0,2,  0,  0,14,  0,'X',0x04}, // GM128:Gunshot
    {  16, 17,248,243,119,  6,2,0, 68,  0, 8, 35,'D',0x04}, // GP35:Ac Bass Drum
    {  16, 17,248,243,119,  6,2,0, 68,  0, 8, 35,'D',0x04}, // GP36:Bass Drum 1
    {   2, 17,249,248,255,255,0,0,  7,  0, 8, 52,'D',0x04}, // GP37:Side Stick
    {   0,  0,252,250,  5, 23,2,0,  0,  0,14, 48,'s',0x04}, // GP38:Acoustic Snare
    {   0,  1,255,255,  7,  8,0,0,  2,  0, 0, 58,'h',0x04}, // GP39:Hand Clap
    {   0,  0,252,250,  5, 23,2,0,  0,  0,14, 60,'s',0x04}, // GP40:Electric Snare
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 47,'M',0x04}, // GP41:Low Floor Tom
    {  12, 18,246,251,  8, 71,0,2,  0,  0,10, 43,'h',0x04}, // GP42:Closed High Hat
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 49,'M',0x04}, // GP43:High Floor Tom
    {  12, 18,246,123,  8, 71,0,2,  0,  5,10, 43,'h',0x04}, // GP44:Pedal High Hat
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 51,'M',0x04}, // GP45:Low Tom
    {  12, 18,246,203,  2, 67,0,2,  0,  0,10, 43,'h',0x04}, // GP46:Open High Hat
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 54,'M',0x04}, // GP47:Low-Mid Tom
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 57,'M',0x04}, // GP48:High-Mid Tom
    {  14,208,246,159,  0,  2,0,3,  0,  0,14, 72,'C',0x62}, // GP49:Crash Cymbal 1
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 60,'M',0x04}, // GP50:High Tom
    {  14,  7,248,244, 66,228,0,3,  8, 74,14, 76,'C',0x04}, // GP51:Ride Cymbal 1
    {  14,208,245,159, 48,  2,0,0,  0, 10,14, 84,'C',0x04}, // GP52:Chinese Cymbal
    {  14,  7,228,245,228,229,3,1, 10, 93, 6, 36,'b',0x04}, // GP53:Ride Bell
    {   2,  5,180,151,  4,247,0,0,  3, 10,14, 65,'M',0x04}, // GP54:Tambourine
    {  78,158,246,159,  0,  2,0,3,  0,  0,14, 84,'C',0x04}, // GP55:Splash Cymbal
    {  17, 16,248,243, 55,  5,2,0, 69,  8, 8, 83,'B',0x04}, // GP56:Cow Bell
    {  14,208,246,159,  0,  2,0,3,  0,  0,14, 84,'C',0x04}, // GP57:Crash Cymbal 2
    { 128, 16,255,255,  3, 20,3,0,  0, 13,12, 24,'D',0x04}, // GP58:Vibraslap
    {  14,  7,248,244, 66,228,0,3,  8, 74,14, 77,'C',0x04}, // GP59:Ride Cymbal 2
    {   6,  2,245,245, 12,  8,0,0, 11,  0, 6, 60,'M',0x04}, // GP60:High Bongo
    {   1,  2,250,200,191,151,0,0,  0,  0, 7, 65,'M',0x04}, // GP61:Low Bongo
    {   1,  1,250,250,135,183,0,0, 81,  0, 6, 59,'D',0x04}, // GP62:Mute High Conga
    {   1,  2,250,248,141,184,0,0, 84,  0, 6, 51,'D',0x04}, // GP63:Open High Conga
    {   1,  2,250,248,136,182,0,0, 89,  0, 6, 45,'D',0x04}, // GP64:Low Conga
    {   1,  0,249,250, 10,  6,3,0,  0,  0,14, 71,'M',0x62}, // GP65:High Timbale
    {   0,  0,249,246,137,108,3,0,128,  0,14, 60,'M',0x04}, // GP66:Low Timbale
    {   3, 12,248,246,136,182,3,0,128,  8,15, 58,'D',0x04}, // GP67:High Agogo
    {   3, 12,248,246,136,182,3,0,133,  0,15, 53,'D',0x04}, // GP68:Low Agogo
    {  14,  0,118,119, 79, 24,0,2, 64,  8,14, 64,'D',0x04}, // GP69:Cabasa
    {  14,  3,200,155, 73,105,0,2, 64,  0,14, 71,'D',0x04}, // GP70:Maracas
    { 215,199,173,141,  5,  5,3,0,220,  0,14, 61,'D',0x04}, // GP71:Short Whistle
    { 215,199,168,136,  4,  4,3,0,220,  0,14, 61,'D',0x04}, // GP72:Long Whistle
    { 128, 17,246,103,  6, 23,3,3,  0,  0,14, 44,'D',0x04}, // GP73:Short Guiro
    { 128, 17,245, 70,  5, 22,2,3,  0,  9,14, 40,'D',0x04}, // GP74:Long Guiro
    {   6, 21,  0,247,244,245,0,0, 63,  0, 1, 69,'D',0x04}, // GP75:Claves
    {   6, 18,  0,247,244,245,3,0, 63,  0, 0, 68,'D',0x04}, // GP76:High Wood Block
    {   6, 18,  0,247,244,245,0,0, 63,  0, 1, 63,'D',0x04}, // GP77:Low Wood Block
    {   1,  2,103,117,231,  7,0,0, 88,  0, 0, 74,'D',0x04}, // GP78:Mute Cuica
    {  65, 66,248,117, 72,  5,0,0, 69,  8, 0, 60,'D',0x04}, // GP79:Open Cuica
    {  10, 30,224,255,240,  5,3,0, 64, 78, 8, 80,'D',0x04}, // GP80:Mute Triangle
    {  10, 30,224,255,240,  2,3,0,124, 82, 8, 64,'D',0x04}, // GP81:Open Triangle
    {  14,  0,122,123, 74, 27,0,2, 64,  8,14, 72,'D',0x04}, // GP82
    {  14,  7,228, 85,228, 57,3,1, 10, 64, 6, 73,'D',0x04}, // GP83
    {   5,  4,249,214, 50,165,3,0,  5, 64,14, 70,'D',0x04}, // GP84
    {   2, 21,  0,247,243,245,3,0, 63,  0, 8, 68,'D',0x04}, // GP85
    {   1,  2,250,248,141,181,0,0, 79,  0, 7, 48,'D',0x04}, // GP86
    {   0,  0,246,246, 12,  6,0,0,  0,  0, 4, 53,'D',0x04}  // GP87 (low-low-mid tom?)
};

struct insdata
{
    unsigned char data[11];
    signed char   finetune;
    bool operator==(const insdata& b) const
    {
        return std::memcmp(data, b.data, 11) == 0 && finetune == b.finetune;
    }
    bool operator< (const insdata& b) const
    {
        int c = std::memcmp(data, b.data, 11);
        if(c != 0) return c < 0;
        if(finetune != b.finetune) return finetune < b.finetune;
        return 0;
    }
    bool operator!=(const insdata& b) const { return !operator==(b); }
};
struct ins
{
    size_t insno1, insno2;
    unsigned char notenum;

    bool operator==(const ins& b) const
    {
        return notenum == b.notenum
        && insno1 == b.insno1
        && insno2 == b.insno2;
    }
    bool operator< (const ins& b) const
    {
        if(insno1 != b.insno1) return insno1 < b.insno1;
        if(insno2 != b.insno2) return insno2 < b.insno2;
        if(notenum != b.notenum) return notenum < b.notenum;
        return 0;
    }
    bool operator!=(const ins& b) const { return !operator==(b); }
};

static std::map<insdata, std::pair<size_t, std::set<std::string> > > insdatatab;

static std::map<ins, std::pair<size_t, std::set<std::string> > > instab;
static std::map<unsigned, std::map<unsigned, size_t> > progs;

static void SetBank(unsigned bank, unsigned patch, size_t insno)
{
    progs[bank][patch] = insno+1;
}

static size_t InsertIns(
    const insdata& id,
    const insdata& id2,
    ins& in,
    const std::string& name,
    const std::string& name2 = "")
{
  if(true)
  {
    std::map<insdata, std::pair<size_t,std::set<std::string> > >::iterator
        i = insdatatab.lower_bound(id);

    size_t insno = ~0;
    if(i == insdatatab.end() || i->first != id)
    {
        std::pair<insdata, std::pair<size_t,std::set<std::string> > > res;
        res.first = id;
        res.second.first = insdatatab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        insdatatab.insert(i, res);
        insno = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        insno = i->second.first;
    }

    in.insno1 = insno;
  }
  if(id != id2)
  {
    std::map<insdata, std::pair<size_t,std::set<std::string> > >::iterator
        i = insdatatab.lower_bound(id2);

    size_t insno2 = ~0;
    if(i == insdatatab.end() || i->first != id2)
    {
        std::pair<insdata, std::pair<size_t,std::set<std::string> > > res;
        res.first = id2;
        res.second.first = insdatatab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        insdatatab.insert(i, res);
        insno2 = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        insno2 = i->second.first;
    }
    in.insno2 = insno2;
  }
  else
    in.insno2 = in.insno1;


  {
    std::map<ins, std::pair<size_t,std::set<std::string> > >::iterator
        i = instab.lower_bound(in);

    size_t resno = ~0;
    if(i == instab.end() || i->first != in)
    {
        std::pair<ins, std::pair<size_t,std::set<std::string> > > res;
        res.first = in;
        res.second.first = instab.size();
        if(!name.empty()) res.second.second.insert(name);
        if(!name2.empty()) res.second.second.insert(name2);
        instab.insert(i, res);
        resno = res.second.first;
    }
    else
    {
        if(!name.empty()) i->second.second.insert(name);
        if(!name2.empty()) i->second.second.insert(name2);
        resno = i->second.first;
    }
    return resno;
  }
}


static void LoadStandard()
{
    for(unsigned a=0; a<181; ++a)
    {
        char name2[512]; sprintf(name2, "%s%u",
            a<128 ? "GM" : "GP",
            a<128 ? a : (a-128+35));

        const unsigned midiindex = (a&128) ? (a+35) : a;
        insdata tmp1;
        memcpy(tmp1.data, adl[a], 11);
        tmp1.finetune = 0;
        ins tmp2;
        tmp2.notenum  = adl[a][11];
        size_t resno = InsertIns(tmp1,tmp1, tmp2,
            MidiInsName[a][0]?std::string(1,'\377')+MidiInsName[a]:"", name2);
        SetBank(0, midiindex, resno);
    }
}

static void LoadBNK(const char* fn, unsigned bank, const char* prefix, bool is_fat)
{
    FILE* fp = fopen(fn, "rb");
    fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    rewind(fp);
    fread(&data[0], 1, data.size(), fp),
    fclose(fp);

    //printf("%s:\n", fn);
    unsigned short sig1 = *(short*)&data[8];
    unsigned short sig2 = *(short*)&data[10];
    unsigned       sig3 = *(unsigned*)&data[12];
    unsigned       sig4 = *(unsigned*)&data[16];
    //printf("%u %u %u %u\n", sig1,sig2,sig3,sig4);

    for(unsigned n=0; n<sig1; ++n)
    {
        const size_t offset1 = sig3 + n*12;

        unsigned char a = data[offset1+0];
        unsigned char b = data[offset1+1];
        unsigned char c = data[offset1+2];
        std::string name;
        for(unsigned p=0; p<9; ++p)
        {
            if(data[offset1+3+p] == '\0') break;
            name += char(data[offset1+3+p]);
        }

        //printf("%3d %3d %3d %8s: ", a,b,c, name.c_str());

        const size_t offset2 = sig4 + (a + b*256) * 30;
        const unsigned char* op1 = &data[offset2+2];
        const unsigned char* op2 = &data[offset2+15];

        bool percussive = is_fat
            ?   name[1] == 'P' /* data[offset2] == 1 */
            :   (c != 0);

        int gmno = is_fat
            ?   ((n & 127) + percussive*128)
            :   (n + percussive*128);

        if(is_fat && percussive)
        {
            if(name[2] == 'O'
            || name[2] == 'S')
            {
                gmno = 128 + std::atoi(name.substr(3).c_str());
            }
        }

        char name2[512];
        if(is_fat)
            sprintf(name2, "%s%c%u", prefix, percussive?'P':'M', gmno&127);
        else
            sprintf(name2, "%s%u", prefix, n);

        insdata tmp;
        tmp.data[0] = (op1[ 9] << 7) // TREMOLO FLAG
                    + (op1[10] << 6) // VIBRATO FLAG
                    + (op1[ 5] << 5) // SUSTAIN FLAG
                    + (op1[11] << 4) // SCALING FLAG
                     + op1[ 1];      // FREQMUL
        tmp.data[1] = (op2[ 9] << 7)
                    + (op2[10] << 6)
                    + (op2[ 5] << 5)
                    + (op2[11] << 4)
                     + op2[ 1];
        tmp.data[2] = op1[3]*0x10 + op1[6]; // ATTACK, DECAY
        tmp.data[3] = op2[3]*0x10 + op2[6];
        tmp.data[4] = op1[4]*0x10 + op1[7]; // SUSTAIN, RELEASE
        tmp.data[5] = op2[4]*0x10 + op2[7];
        tmp.data[6] = data[offset2+28];
        tmp.data[7] = data[offset2+29];
        tmp.data[8] = op1[0]*0x40 + op1[8]; // KSL , LEVEL
        tmp.data[9] = op2[0]*0x40 + op2[8]; // KSL , LEVEL
        tmp.data[10] = op1[2]*2 + op1[12]; // FEEDBACK, ADDITIVEFLAG
        tmp.finetune = 0;
        // Note: op2[2] and op2[12] are unused and contain garbage.
        ins tmp2;
        tmp2.notenum = is_fat ? data[offset2+1] : c;

        if(is_fat) tmp.data[10] ^= 1;

        size_t resno = InsertIns(tmp,tmp, tmp2, std::string(1,'\377')+name, name2);

        if(!is_fat)
        {
            SetBank(bank, gmno, resno);
        }
        else
        {
            if(name[2] == 'O' || name[1] == 'M') SetBank(bank+0, gmno, resno);
            if(name[2] == 'S' || name[1] == 'M') SetBank(bank+1, gmno, resno);
        }

        /*for(unsigned p=0; p<30; ++p)
        {
            unsigned char value = data[offset2+p];
            if(value > maxvalues[p]) maxvalues[p] = value;

            if(0 && midi_index < 182)
            {
                for(unsigned valuebit=0; valuebit<8; ++valuebit)
                {
                    unsigned v = (value >> valuebit) & 1;
                    for(unsigned adlbyte=0; adlbyte<11; ++adlbyte)
                    {
                        const unsigned adlbits =
                            (adlbyte==6 || adlbyte==7) ? 2
                          : (adlbyte==10) ? 4
                          : 8;
                        for(unsigned adlbit=0; adlbit<adlbits; ++adlbit)
                        {
                            if((adlbyte == 9 || adlbyte == 8) && (adlbit==5||adlbit==4))
                                continue;
                            unsigned a = (adl[midi_index][adlbyte] >> adlbit) & 1;
                            if(v == a)
                            {
                                Correlate[p*8+valuebit]
                                    [adlbyte*8+adlbit] += 1;
                            }
                        }
                    }
                }
            }

            #define dot(maxval) if(value<=maxval) printf("."); else printf("?[%u]%X",p,value);

          {
            
            //if(p==6 || p==7 || p==19||p==20) value=15-value;
            
            if(p==4 || p==10 || p==17 || p==23)// || p==25)
                printf(" %2X", value);
            else
                printf(" %X", value);
         }
        nl:;
            //if(p == 12) printf("\n%*s", 22, "");
            //if(p == 25) printf("\n%*s", 22, "");
        }
        printf("\n");*/
    }
}

struct Doom_OPL2instrument {
  unsigned char trem_vibr_1;    /* OP 1: tremolo/vibrato/sustain/KSR/multi */
  unsigned char att_dec_1;      /* OP 1: attack rate/decay rate */
  unsigned char sust_rel_1;     /* OP 1: sustain level/release rate */
  unsigned char wave_1;         /* OP 1: waveform select */
  unsigned char scale_1;        /* OP 1: key scale level */
  unsigned char level_1;        /* OP 1: output level */
  unsigned char feedback;       /* feedback/AM-FM (both operators) */
  unsigned char trem_vibr_2;    /* OP 2: tremolo/vibrato/sustain/KSR/multi */
  unsigned char att_dec_2;      /* OP 2: attack rate/decay rate */
  unsigned char sust_rel_2;     /* OP 2: sustain level/release rate */
  unsigned char wave_2;         /* OP 2: waveform select */
  unsigned char scale_2;        /* OP 2: key scale level */
  unsigned char level_2;        /* OP 2: output level */
  unsigned char unused;
  short         basenote;       /* base note offset */
} __attribute__ ((packed));

struct Doom_opl_instr {
        unsigned short        flags;
#define FL_FIXED_PITCH  0x0001          // note has fixed pitch (drum note)
#define FL_UNKNOWN      0x0002          // ??? (used in instrument #65 only)
#define FL_DOUBLE_VOICE 0x0004          // use two voices instead of one

        unsigned char         finetune;
        unsigned char         note;
        struct Doom_OPL2instrument patchdata[2];
} __attribute__ ((packed));


static void LoadDoom(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = fopen(fn, "rb");
    fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    rewind(fp);
    fread(&data[0], 1, data.size(), fp),
    fclose(fp);

    for(unsigned a=0; a<175; ++a)
    {
        const size_t offset1 = 0x18A4 + a*32;
        const size_t offset2 = 8      + a*36;

        std::string name;
        for(unsigned p=0; p<32; ++p)
            if(data[offset1]!='\0')
                name += char(data[offset1+p]);

        //printf("%3d %3d %3d %8s: ", a,b,c, name.c_str());
        int gmno = a<128 ? a : ((a|128)+35);

        char name2[512]; sprintf(name2, "%s%c%u", prefix, (gmno<128?'M':'P'), gmno&127);

        Doom_opl_instr& ins = *(Doom_opl_instr*) &data[offset2];

        insdata tmp[2];
        for(unsigned index=0; index<2; ++index)
        {
            const Doom_OPL2instrument& src = ins.patchdata[index];
            tmp[index].data[0] = src.trem_vibr_1;
            tmp[index].data[1] = src.trem_vibr_2;
            tmp[index].data[2] = src.att_dec_1;
            tmp[index].data[3] = src.att_dec_2;
            tmp[index].data[4] = src.sust_rel_1;
            tmp[index].data[5] = src.sust_rel_2;
            tmp[index].data[6] = src.wave_1;
            tmp[index].data[7] = src.wave_2;
            tmp[index].data[8] = src.scale_1 | src.level_1;
            tmp[index].data[9] = src.scale_2 | src.level_2;
            tmp[index].data[10] = src.feedback;
            tmp[index].finetune = src.basenote + 12;
        }
        struct ins tmp2;
        tmp2.notenum  = ins.note;

        if(!(ins.flags&4))
        {
            size_t resno = InsertIns(tmp[0],tmp[0],tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }
        else // Double instrument
        {
            size_t resno = InsertIns(tmp[0],tmp[1],tmp2, std::string(1,'\377')+name, name2);
            SetBank(bank, gmno, resno);
        }

        /*const Doom_OPL2instrument& A = ins.patchdata[0];
        const Doom_OPL2instrument& B = ins.patchdata[1];
        printf(
            "flags:%04X fine:%02X note:%02X\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d}\n"
            "{t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X f:%02X\n"
            " t:%02X a:%02X s:%02X w:%02X s:%02X l:%02X ?:%02X base:%d} ",
            ins.flags, ins.finetune, ins.note,
            A.trem_vibr_1, A.att_dec_1, A.sust_rel_1,
            A.wave_1, A.scale_1, A.level_1, A.feedback,
            A.trem_vibr_2, A.att_dec_2, A.sust_rel_2,
            A.wave_2, A.scale_2, A.level_2, A.unused, A.basenote,
            B.trem_vibr_1, B.att_dec_1, B.sust_rel_1,
            B.wave_1, B.scale_1, B.level_1, B.feedback,
            B.trem_vibr_2, B.att_dec_2, B.sust_rel_2,
            B.wave_2, B.scale_2, B.level_2, B.unused, B.basenote);
        printf(" %s VS %s\n", name.c_str(), MidiInsName[a]);
        printf("------------------------------------------------------------\n");*/
    }
}
static void LoadMiles(const char* fn, unsigned bank, const char* prefix)
{
    FILE* fp = fopen(fn, "rb");
    fseek(fp, 0, SEEK_END);
    std::vector<unsigned char> data(ftell(fp));
    rewind(fp);
    fread(&data[0], 1, data.size(), fp),
    fclose(fp);

    for(unsigned a=0; a<200; ++a)
    {
        unsigned gmnumber  = data[a*6+0];
        unsigned gmnumber2 = data[a*6+1];
        unsigned offset    = *(unsigned*)&data[a*6+2];
        
        if(gmnumber == 0xFF) break;
        int gmno = gmnumber2==0x7F ? gmnumber+0x80 : gmnumber;
        int midi_index = gmno < 128 ? gmno
                       : gmno < 128+35 ? -1
                       : gmno < 128+88 ? gmno-35
                       : -1;
        unsigned length = data[offset] + data[offset+1]*256;
        signed char notenum = data[offset+2];

        /*printf("%02X %02X %08X ", gmnumber,gmnumber2, offset);
        for(unsigned b=0; b<length; ++b)
        {
            if(b > 3 && (b-3)%11 == 0) printf("\n                        ");
            printf("%02X ", data[offset+b]);
        }
        printf("\n");*/

        char name2[512]; sprintf(name2, "%s%c%u", prefix,
            (gmno<128?'M':'P'), gmno&127);

        insdata tmp[200];

        const unsigned inscount = (length-3)/11;
        for(unsigned i=0; i<inscount; ++i)
        {
            unsigned o = offset + 3 + i*11;
            tmp[i].finetune = (gmno < 128 && i == 0) ? notenum : 0;
            tmp[i].data[0] = data[o+0];  // 20
            tmp[i].data[8] = data[o+1];  // 40 (vol)
            tmp[i].data[2] = data[o+2];  // 60
            tmp[i].data[4] = data[o+3];  // 80
            tmp[i].data[6] = data[o+4];  // E0
            tmp[i].data[1] = data[o+6];  // 23
            tmp[i].data[9] = data[o+7]; // 43 (vol)
            tmp[i].data[3] = data[o+8]; // 63
            tmp[i].data[5] = data[o+9]; // 83
            tmp[i].data[7] = data[o+10]; // E3

            tmp[i].data[10] = data[offset+3+5] & 0x0F; // C0
            if(i == 1)
                tmp[0].data[10] = data[offset+3+5] >> 4;
        }
        if(inscount == 1) tmp[1] = tmp[0];
        if(inscount <= 2)
        {
            struct ins tmp2;
            tmp2.notenum  = gmno < 128 ? 0 : data[offset+3];
            size_t resno = InsertIns(tmp[0], tmp[1], tmp2,
                std::string(1,'\377')+MidiInsName[midi_index], name2);
            SetBank(bank, gmno, resno);
        }
    }
}

#include "dbopl.h"

std::vector<int> sampleBuf;
static void AddMonoAudio(unsigned long count, int* samples)
{
    sampleBuf.insert(sampleBuf.end(), samples, samples+count);
}

struct DurationInfo
{
    long ms_sound_kon;
    long ms_sound_koff;
};
static DurationInfo MeasureDurations(const ins& in)
{
    insdata id[2];
    bool found[2] = {false,false};
    for(std::map<insdata, std::pair<size_t, std::set<std::string> > >
        ::const_iterator
        j = insdatatab.begin();
        j != insdatatab.end();
        ++j)
    {
        if(j->second.first == in.insno1) { id[0] = j->first; found[0]=true; if(found[1]) break; }
        if(j->second.first == in.insno2) { id[1] = j->first; found[1]=true; if(found[0]) break; }
    }
    const unsigned rate = 22050;
    const unsigned interval             = 150;
    const unsigned samples_per_interval = rate / interval;
    const unsigned notenum = in.notenum ? in.notenum : 44;

    DBOPL::Handler opl;
    static const short initdata[(2+3+2+2)*2] =
    { 0x004,96, 0x004,128,        // Pulse timer
      0x105, 0, 0x105,1, 0x105,0, // Pulse OPL3 enable, leave disabled
      0x001,32, 0x0BD,0           // Enable wave & melodic
    };
    opl.Init(rate);
    for(unsigned a=0; a<18; a+=2) opl.WriteReg(initdata[a], initdata[a+1]);

    const unsigned n_notes = in.insno1 == in.insno2 ? 1 : 2;
    unsigned x[2];
    for(unsigned n=0; n<n_notes; ++n)
    {
        static const unsigned char patchdata[11] =
            {0x20,0x23,0x60,0x63,0x80,0x83,0xE0,0xE3,0xC0,0x40,0x43};
        for(unsigned a=0; a<11; ++a) opl.WriteReg(patchdata[a]+n, id[n].data[a]);
        double hertz = 172.00093 * std::exp(0.057762265 * (notenum + id[n].finetune));
        if(hertz > 131071)
        {
            fprintf(stderr, "Why does note %d + finetune %d produce hertz %g?\n",
                notenum, id[n].finetune, hertz);
            hertz = 131071;
        }
        x[n] = 0x2000;
        while(hertz >= 1023.5) { hertz /= 2.0; x[n] += 0x400; } // Calculate octave
        x[n] += (int)(hertz + 0.5);

        // Keyon the note
        opl.WriteReg(0xA0+n, x[n]&0xFF);
        opl.WriteReg(0xB0+n, x[n]>>8);
    }

    // For up to 40 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_on;
    double highest_sofar = 0;
    for(unsigned period=0; period<40*interval; ++period)
    {
        sampleBuf.clear();
        unsigned n = samples_per_interval;
        while(n > 512)
            {opl.Generate(AddMonoAudio, 0, 512); n-=512;}
        if(n)opl.Generate(AddMonoAudio, 0, n);
        unsigned long count = sampleBuf.size();

        double mean = 0.0;
        for(unsigned long c=0; c<count; ++c)
            mean += sampleBuf[c];
        mean /= count;
        double std_deviation = 0;
        for(unsigned long c=0; c<count; ++c)
        {
            double diff = (sampleBuf[c]-mean);
            std_deviation += diff*diff;
        }
        std_deviation = std::sqrt(std_deviation / count);
        amplitudecurve_on.push_back(std_deviation);
        if(std_deviation > highest_sofar)
            highest_sofar = std_deviation;

        if(period > 6*interval && std_deviation < highest_sofar*0.2)
            break;
    }

    // Keyoff the note
    for(unsigned n=0; n<n_notes; ++n)
        opl.WriteReg(0xB0+n, (x[n]>>8) & 0xDF);

    // Now, for up to 60 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_off;
    for(unsigned period=0; period<60*interval; ++period)
    {
        sampleBuf.clear();
        unsigned n = samples_per_interval;
        while(n > 512)
            {opl.Generate(AddMonoAudio, 0, 512); n-=512;}
        if(n)opl.Generate(AddMonoAudio, 0, n);
        unsigned long count = sampleBuf.size();

        double mean = 0.0;
        for(unsigned long c=0; c<count; ++c)
            mean += sampleBuf[c];
        mean /= count;
        double std_deviation = 0;
        for(unsigned long c=0; c<count; ++c)
        {
            double diff = (sampleBuf[c]-mean);
            std_deviation += diff*diff;
        }
        std_deviation = std::sqrt(std_deviation / count);
        amplitudecurve_off.push_back(std_deviation);

        if(std_deviation < highest_sofar*0.2) break;
    }

    /* Analyze the results */
    double begin_amplitude        = amplitudecurve_on[0];
    double peak_amplitude_value   = begin_amplitude;
    size_t peak_amplitude_time    = 0;
    size_t quarter_amplitude_time = amplitudecurve_on.size();
    size_t keyoff_out_time        = 0;

    for(size_t a=1; a<amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] > peak_amplitude_value)
        {
            peak_amplitude_value = amplitudecurve_on[a];
            peak_amplitude_time  = a;
        }
    }
    for(size_t a=peak_amplitude_time; a<amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] <= peak_amplitude_value * 0.2)
        {
            quarter_amplitude_time = a;
            break;
        }
    }
    for(size_t a=0; a<amplitudecurve_off.size(); ++a)
    {
        if(amplitudecurve_off[a] <= peak_amplitude_value * 0.2)
        {
            keyoff_out_time = a;
            break;
        }
    }

    if(keyoff_out_time == 0 && amplitudecurve_on.back() < peak_amplitude_value*0.2)
        keyoff_out_time = quarter_amplitude_time;

    if(peak_amplitude_time == 0)
    {
        printf(
            "    // Amplitude begins at %6.1f,\n"
            "    // fades to 20%% at %.1fs, keyoff fades to 20%% in %.1fs.\n",
            begin_amplitude,
            quarter_amplitude_time / double(interval),
            keyoff_out_time / double(interval));
    }
    else
    {
        printf(
            "    // Amplitude begins at %6.1f, peaks %6.1f at %.1fs,\n"
            "    // fades to 20%% at %.1fs, keyoff fades to 20%% in %.1fs.\n",
            begin_amplitude,
            peak_amplitude_value,
            peak_amplitude_time / double(interval),
            quarter_amplitude_time / double(interval),
            keyoff_out_time / double(interval));
    }

    DurationInfo result;
    result.ms_sound_kon  = quarter_amplitude_time * 1000.0 / interval;
    result.ms_sound_koff = keyoff_out_time        * 1000.0 / interval;
    return result;
}

int main()
{
    LoadStandard();
    LoadBNK("descent/melodic.bnk", 1, "HMIGM", false);
    LoadBNK("descent/drum.bnk",    1, "HMIGP", false);
    LoadBNK("descent/intmelo.bnk", 2, "intM", false);
    LoadBNK("descent/intdrum.bnk", 2, "intP", false);
    LoadBNK("descent/hammelo.bnk", 3, "hamM", false);
    LoadBNK("descent/hamdrum.bnk", 3, "hamP", false);
    LoadBNK("descent/rickmelo.bnk",4, "rickM", false);
    LoadBNK("descent/rickdrum.bnk",4, "rickP", false);
    LoadDoom("doom2/genmidi.op2", 5, "dM");
    LoadDoom("doom2/genmidi.htc", 6, "hxM");
    LoadMiles("miles/warcraft.ad", 7, "sG");
    LoadMiles("miles/simfarm.opl", 8, "qG");
    LoadMiles("miles/simfarm.ad", 9, "mG");
    LoadMiles("miles/sample.ad", 10, "MG");
    LoadMiles("miles/sample.opl", 11, "oG");
    // LoadMiles("miles/sc3.opl",  10, "2miles"); // Actually this is our "standard"!

    //LoadBNK("fat/fatv10.bnk",   9, "sfat", true); // Two banks. Also, not worth it.

    static const char* banknames[] =
    {"Miles SC3",      // Nov  9  1993
     "HMI GM",         // 
     "HMI int",
     "HMI ham",
     "HMI rick",
     "Doom",           //
     "Hexen",          // 
     "Miles Warcraft", // 1995
     "Miles QUAD-OP",  // 
     "Miles Simfarm",  // 1993
     "Miles 2.14 OSS",
     "Miles UW"        //
    };

#if 0
    for(unsigned a=0; a<36*8; ++a)
    {
        if( (1 << (a%8)) > maxvalues[a/8]) continue;
        
        const std::map<unsigned,unsigned>& data = Correlate[a];
        if(data.empty()) continue;
        std::vector< std::pair<unsigned,unsigned> > correlations;
        for(std::map<unsigned,unsigned>::const_iterator
            i = data.begin();
            i != data.end();
            ++i)
        {
            correlations.push_back( std::make_pair( i->second,i->first ) );
        }
        std::sort(correlations.begin(), correlations.end());
        printf("Byte %2u bit %u=mask %02X:\n", a/8, a%8, 1<<(a%8));
        for(size_t c=0; c<correlations.size() && c < 10; ++c)
        {
            unsigned count = correlations[correlations.size()-1-c ].first;
            unsigned index = correlations[correlations.size()-1-c ].second;
            printf("\tAdldata index %u, bit %u=mask %02X (%u matches)\n",
                index/8, index%8, 1 << (index%8), count);
        }
    }
#endif

    printf(
        /*
        "static const struct\n"
        "{\n"
        "    unsigned carrier_E862, modulator_E862;  // See below\n"
        "    unsigned char carrier_40, modulator_40; // KSL/attenuation settings\n"
        "    unsigned char feedconn; // Feedback/connection bits for the channel\n"
        "    signed char finetune;   // Finetune\n"
        "} adl[] =\n"*/
        "const adldata adl[%u] =\n"
        "{ //    ,---------+-------- Wave select settings\n"
        "  //    | ,-------÷-+------ Sustain/release rates\n"
        "  //    | | ,-----÷-÷-+---- Attack/decay rates\n"
        "  //    | | | ,---÷-÷-÷-+-- AM/VIB/EG/KSR/Multiple bits\n"
        "  //    | | | |   | | | |\n"
        "  //    | | | |   | | | |     ,----+-- KSL/attenuation settings\n"
        "  //    | | | |   | | | |     |    |    ,----- Feedback/connection bits\n"
        "  //    | | | |   | | | |     |    |    |\n", (unsigned)insdatatab.size());
    for(size_t b=insdatatab.size(), c=0; c<b; ++c)
        for(std::map<insdata,std::pair<size_t,std::set<std::string> > >
            ::const_iterator
            i = insdatatab.begin();
            i != insdatatab.end();
            ++i)
        {
            if(i->second.first != c) continue;
            printf("    { ");

            unsigned carrier_E862 =
                (i->first.data[6] << 24)
              + (i->first.data[4] << 16)
              + (i->first.data[2] << 8)
              + (i->first.data[0] << 0);
            unsigned modulator_E862 =
                (i->first.data[7] << 24)
              + (i->first.data[5] << 16)
              + (i->first.data[3] << 8)
              + (i->first.data[1] << 0);
            printf("0x%07X,0x%07X, 0x%02X,0x%02X, 0x%X,%+d",
                carrier_E862,
                modulator_E862,
                i->first.data[8],
                i->first.data[9],
                i->first.data[10],
                i->first.finetune);

            std::string names;
            for(std::set<std::string>::const_iterator
                j = i->second.second.begin();
                j != i->second.second.end();
                ++j)
            {
                if(!names.empty()) names += "; ";
                if((*j)[0] == '\377')
                    names += j->substr(1);
                else
                    names += *j;
            }
            printf(" }, // %u: %s\n", (unsigned)c, names.c_str());
        }
    printf("};\n");
    /*printf("static const struct\n"
           "{\n"
           "    unsigned short adlno1, adlno2;\n"
           "    unsigned char tone;\n"
           "    long ms_sound_kon;  // Number of milliseconds it produces sound;\n"
           "    long ms_sound_koff;\n"
           "} adlins[] =\n");*/
    printf("const struct adlinsdata adlins[%u] =\n", (unsigned)instab.size());
    printf("{\n");
    for(size_t b=instab.size(), c=0; c<b; ++c)
        for(std::map<ins,std::pair<size_t,std::set<std::string> > >
            ::const_iterator
            i = instab.begin();
            i != instab.end();
            ++i)
        {
            if(i->second.first != c) continue;

            DurationInfo info = MeasureDurations(i->first);

            printf("    { ");
            printf("%3d,%3d,%3d, %6ld,%6ld",
                (unsigned) i->first.insno1,
                (unsigned) i->first.insno2,
                i->first.notenum,
                info.ms_sound_kon,
                info.ms_sound_koff);
            std::string names;
            for(std::set<std::string>::const_iterator
                j = i->second.second.begin();
                j != i->second.second.end();
                ++j)
            {
                if(!names.empty()) names += "; ";
                if((*j)[0] == '\377')
                    names += j->substr(1);
                else
                    names += *j;
            }
            printf(" }, // %u: %s\n\n", (unsigned)c, names.c_str());
            fflush(stdout);
        }
    printf("};\n");
    //printf("static const unsigned short banks[][256] =\n");
    const unsigned bankcount = sizeof(banknames)/sizeof(*banknames);
    printf("const unsigned short banks[%u][256] =\n", bankcount);
    printf("{\n");
    for(unsigned bank=0; bank<bankcount; ++bank)
    {
        printf("    { // bank %u, %s\n", bank, banknames[bank]);
        for(unsigned p=0; p<256; ++p)
        {
            size_t v = progs[bank][p];
            if(v == 0)
                v = 198; // Blank.in
            else
                v -= 1;
            printf("%3d,", (unsigned)v);
            if(p%16 == 15) printf("\n");
        }
        printf("    },\n");
    }
    printf("};\n");
}
