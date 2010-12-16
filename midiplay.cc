#include <vector>
#include <string>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <stdarg.h>

#include <SDL.h>
#include <deque>
#include <pthread.h>

#include "dbopl.h"

static const unsigned MaxSamplesAtTime = 512; // dbopl limitation

static const unsigned char adl[182][13] =
{
// The data bytes are:
//  [0,1] AM/VIB/EG/KSR/Multiple bits for carrier and modulator respectively
//  [2,3] KSL/Attenuation settings    for carrier and modulator respectively
//  [4,5] Attack and decay rates      for carrier and modulator respectively
//  [6,7] Sustain and release rates   for carrier and modulator respectively
//  [8,9] Wave select settings        for carrier and modulator respectively
//  [10]  Feedback/connection bits    for the channel (also stereo/pan bits)
//  [11]  For percussive instruments (GP35..GP87), the tone to play
//  [12]  Drawing symbol
    {   1,  1,143,  6,242,242,244,247,0,0, 56,  0,'P'}, // GM1:AcouGrandPiano
    {   1,  1, 75,  0,242,242,244,247,0,0, 56,  0,'P'}, // GM2:BrightAcouGrand
    {   1,  1, 73,  0,242,242,244,246,0,0, 56,  0,'P'}, // GM3:ElecGrandPiano
    { 129, 65, 18,  0,242,242,247,247,0,0, 54,  0,'P'}, // GM4:Honky-tonkPiano
    {   1,  1, 87,  0,241,242,247,247,0,0, 48,  0,'P'}, // GM5:Rhodes Piano
    {   1,  1,147,  0,241,242,247,247,0,0, 48,  0,'P'}, // GM6:Chorused Piano
    {   1, 22,128, 14,161,242,242,245,0,0, 56,  0,'h'}, // GM7:Harpsichord
    {   1,  1,146,  0,194,194,248,248,0,0, 58,  0,'c'}, // GM8:Clavinet
    {  12,129, 92,  0,246,243,244,245,0,0, 48,  0,'c'}, // GM9:Celesta
    {   7, 17,151,128,243,242,242,241,0,0, 50,  0,'k'}, // GM10:Glockenspiel
    {  23,  1, 33,  0, 84,244,244,244,0,0, 50,  0,'m'}, // GM11:Music box
    { 152,129, 98,  0,243,242,246,246,0,0, 48,  0,'v'}, // GM12:Vibraphone
    {  24,  1, 35,  0,246,231,246,247,0,0, 48,  0,'m'}, // GM13:Marimba
    {  21,  1,145,  0,246,246,246,246,0,0, 52,  0,'x'}, // GM14:Xylophone
    {  69,129, 89,128,211,163,243,243,0,0, 60,  0,'b'}, // GM15:Tubular Bells
    {   3,129, 73,128,117,181,245,245,1,0, 52,  0,'d'}, // GM16:Dulcimer
    { 113, 49,146,  0,246,241, 20,  7,0,0, 50,  0,'o'}, // GM17:Hammond Organ
    { 114, 48, 20,  0,199,199, 88,  8,0,0, 50,  0,'o'}, // GM18:Percussive Organ
    { 112,177, 68,  0,170,138, 24,  8,0,0, 52,  0,'o'}, // GM19:Rock Organ
    {  35,177,147,  0,151, 85, 35, 20,1,0, 52,  0,'o'}, // GM20:Church Organ
    {  97,177, 19,128,151, 85,  4,  4,1,0, 48,  0,'o'}, // GM21:Reed Organ
    {  36,177, 72,  0,152, 70, 42, 26,1,0, 60,  0,'a'}, // GM22:Accordion
    {  97, 33, 19,  0,145, 97,  6,  7,1,0, 58,  0,'h'}, // GM23:Harmonica
    {  33,161, 19,137,113, 97,  6,  7,0,0, 54,  0,'o'}, // GM24:Tango Accordion
    {   2, 65,156,128,243,243,148,200,1,0, 60,  0,'G'}, // GM25:Acoustic Guitar1
    {   3, 17, 84,  0,243,241,154,231,1,0, 60,  0,'G'}, // GM26:Acoustic Guitar2
    {  35, 33, 95,  0,241,242, 58,248,0,0, 48,  0,'G'}, // GM27:Electric Guitar1
    {   3, 33,135,128,246,243, 34,248,1,0, 54,  0,'G'}, // GM28:Electric Guitar2
    {   3, 33, 71,  0,249,246, 84, 58,0,0, 48,  0,'G'}, // GM29:Electric Guitar3
    {  35, 33, 74,  5,145,132, 65, 25,1,0, 56,  0,'G'}, // GM30:Overdrive Guitar
    {  35, 33, 74,  0,149,148, 25, 25,1,0, 56,  0,'G'}, // GM31:Distorton Guitar
    {   9,132,161,128, 32,209, 79,248,0,0, 56,  0,'G'}, // GM32:Guitar Harmonics
    {  33,162, 30,  0,148,195,  6,166,0,0, 50,  0,'B'}, // GM33:Acoustic Bass
    {  49, 49, 18,  0,241,241, 40, 24,0,0, 58,  0,'B'}, // GM34:Electric Bass 1
    {  49, 49,141,  0,241,241,232,120,0,0, 58,  0,'B'}, // GM35:Electric Bass 2
    {  49, 50, 91,  0, 81,113, 40, 72,0,0, 60,  0,'B'}, // GM36:Fretless Bass
    {   1, 33,139, 64,161,242,154,223,0,0, 56,  0,'B'}, // GM37:Slap Bass 1
    {  33, 33,139,  8,162,161, 22,223,0,0, 56,  0,'B'}, // GM38:Slap Bass 2
    {  49, 49,139,  0,244,241,232,120,0,0, 58,  0,'B'}, // GM39:Synth Bass 1
    {  49, 49, 18,  0,241,241, 40, 24,0,0, 58,  0,'B'}, // GM40:Synth Bass 2
    {  49, 33, 21,  0,221, 86, 19, 38,1,0, 56,  0,'V'}, // GM41:Violin
    {  49, 33, 22,  0,221,102, 19,  6,1,0, 56,  0,'V'}, // GM42:Viola
    { 113, 49, 73,  0,209, 97, 28, 12,1,0, 56,  0,'V'}, // GM43:Cello
    {  33, 35, 77,128,113,114, 18,  6,1,0, 50,  0,'V'}, // GM44:Contrabass
    { 241,225, 64,  0,241,111, 33, 22,1,0, 50,  0,'V'}, // GM45:Tremulo Strings
    {   2,  1, 26,128,245,133,117, 53,1,0, 48,  0,'H'}, // GM46:Pizzicato String
    {   2,  1, 29,128,245,243,117,244,1,0, 48,  0,'H'}, // GM47:Orchestral Harp
    {  16, 17, 65,  0,245,242,  5,195,1,0, 50,  0,'T'}, // GM48:Timpany
    {  33,162,155,  1,177,114, 37,  8,1,0, 62,  0,'S'}, // GM49:String Ensemble1
    { 161, 33,152,  0,127, 63,  3,  7,1,1, 48,  0,'S'}, // GM50:String Ensemble2
    { 161, 97,147,  0,193, 79, 18,  5,0,0, 58,  0,'S'}, // GM51:Synth Strings 1
    {  33, 97, 24,  0,193, 79, 34,  5,0,0, 60,  0,'S'}, // GM52:SynthStrings 2
    {  49,114, 91,131,244,138, 21,  5,0,0, 48,  0,'O'}, // GM53:Choir Aahs
    { 161, 97,144,  0,116,113, 57,103,0,0, 48,  0,'O'}, // GM54:Voice Oohs
    { 113,114, 87,  0, 84,122,  5,  5,0,0, 60,  0,'O'}, // GM55:Synth Voice
    { 144, 65,  0,  0, 84,165, 99, 69,0,0, 56,  0,'c'}, // GM56:Orchestra Hit
    {  33, 33,146,  1,133,143, 23,  9,0,0, 60,  0,'T'}, // GM57:Trumpet
    {  33, 33,148,  5,117,143, 23,  9,0,0, 60,  0,'T'}, // GM58:Trombone
    {  33, 97,148,  0,118,130, 21, 55,0,0, 60,  0,'T'}, // GM59:Tuba
    {  49, 33, 67,  0,158, 98, 23, 44,1,1, 50,  0,'T'}, // GM60:Muted Trumpet
    {  33, 33,155,  0, 97,127,106, 10,0,0, 50,  0,'T'}, // GM61:French Horn
    {  97, 34,138,  6,117,116, 31, 15,0,0, 56,  0,'T'}, // GM62:Brass Section
    { 161, 33,134,131,114,113, 85, 24,1,0, 48,  0,'T'}, // GM63:Synth Brass 1
    {  33, 33, 77,  0, 84,166, 60, 28,0,0, 56,  0,'T'}, // GM64:Synth Brass 2
    {  49, 97,143,  0,147,114,  2, 11,1,0, 56,  0,'X'}, // GM65:Soprano Sax
    {  49, 97,142,  0,147,114,  3,  9,1,0, 56,  0,'X'}, // GM66:Alto Sax
    {  49, 97,145,  0,147,130,  3,  9,1,0, 58,  0,'X'}, // GM67:Tenor Sax
    {  49, 97,142,  0,147,114, 15, 15,1,0, 58,  0,'X'}, // GM68:Baritone Sax
    {  33, 33, 75,  0,170,143, 22, 10,1,0, 56,  0,'T'}, // GM69:Oboe
    {  49, 33,144,  0,126,139, 23, 12,1,1, 54,  0,'T'}, // GM70:English Horn
    {  49, 50,129,  0,117, 97, 25, 25,1,0, 48,  0,'T'}, // GM71:Bassoon
    {  50, 33,144,  0,155,114, 33, 23,0,0, 52,  0,'F'}, // GM72:Clarinet
    { 225,225, 31,  0,133,101, 95, 26,0,0, 48,  0,'F'}, // GM73:Piccolo
    { 225,225, 70,  0,136,101, 95, 26,0,0, 48,  0,'F'}, // GM74:Flute
    { 161, 33,156,  0,117,117, 31, 10,0,0, 50,  0,'F'}, // GM75:Recorder
    {  49, 33,139,  0,132,101, 88, 26,0,0, 48,  0,'F'}, // GM76:Pan Flute
    { 225,161, 76,  0,102,101, 86, 38,0,0, 48,  0,'F'}, // GM77:Bottle Blow
    {  98,161,203,  0,118, 85, 70, 54,0,0, 48,  0,'F'}, // GM78:Shakuhachi
    {  98,161,153,  0, 87, 86,  7,  7,0,0, 59,  0,'F'}, // GM79:Whistle
    {  98,161,147,  0,119,118,  7,  7,0,0, 59,  0,'F'}, // GM80:Ocarina
    {  34, 33, 89,  0,255,255,  3, 15,2,0, 48,  0,'L'}, // GM81:Lead 1 squareea
    {  33, 33, 14,  0,255,255, 15, 15,1,1, 48,  0,'L'}, // GM82:Lead 2 sawtooth
    {  34, 33, 70,128,134,100, 85, 24,0,0, 48,  0,'L'}, // GM83:Lead 3 calliope
    {  33,161, 69,  0,102,150, 18, 10,0,0, 48,  0,'L'}, // GM84:Lead 4 chiff
    {  33, 34,139,  0,146,145, 42, 42,1,0, 48,  0,'L'}, // GM85:Lead 5 charang
    { 162, 97,158, 64,223,111,  5,  7,0,0, 50,  0,'L'}, // GM86:Lead 6 voice
    {  32, 96, 26,  0,239,143,  1,  6,0,2, 48,  0,'L'}, // GM87:Lead 7 fifths
    {  33, 33,143,128,241,244, 41,  9,0,0, 58,  0,'L'}, // GM88:Lead 8 brass
    { 119,161,165,  0, 83,160,148,  5,0,0, 50,  0,'p'}, // GM89:Pad 1 new age
    {  97,177, 31,128,168, 37, 17,  3,0,0, 58,  0,'p'}, // GM90:Pad 2 warm
    {  97, 97, 23,  0,145, 85, 52, 22,0,0, 60,  0,'p'}, // GM91:Pad 3 polysynth
    { 113,114, 93,  0, 84,106,  1,  3,0,0, 48,  0,'p'}, // GM92:Pad 4 choir
    {  33,162,151,  0, 33, 66, 67, 53,0,0, 56,  0,'p'}, // GM93:Pad 5 bowedpad
    { 161, 33, 28,  0,161, 49,119, 71,1,1, 48,  0,'p'}, // GM94:Pad 6 metallic
    {  33, 97,137,  3, 17, 66, 51, 37,0,0, 58,  0,'p'}, // GM95:Pad 7 halo
    { 161, 33, 21,  0, 17,207, 71,  7,1,0, 48,  0,'p'}, // GM96:Pad 8 sweep
    {  58, 81,206,  0,248,134,246,  2,0,0, 50,  0,'X'}, // GM97:FX 1 rain
    {  33, 33, 21,  0, 33, 65, 35, 19,1,0, 48,  0,'X'}, // GM98:FX 2 soundtrack
    {   6,  1, 91,  0,116,165,149,114,0,0, 48,  0,'X'}, // GM99:FX 3 crystal
    {  34, 97,146,131,177,242,129, 38,0,0, 60,  0,'X'}, // GM100:FX 4 atmosphere
    {  65, 66, 77,  0,241,242, 81,245,1,0, 48,  0,'X'}, // GM101:FX 5 brightness
    {  97,163,148,128, 17, 17, 81, 19,1,0, 54,  0,'X'}, // GM102:FX 6 goblins
    {  97,161,140,128, 17, 29, 49,  3,0,0, 54,  0,'X'}, // GM103:FX 7 echoes
    { 164, 97, 76,  0,243,129,115, 35,1,0, 52,  0,'X'}, // GM104:FX 8 sci-fi
    {   2,  7,133,  3,210,242, 83,246,0,1, 48,  0,'G'}, // GM105:Sitar
    {  17, 19, 12,128,163,162, 17,229,1,0, 48,  0,'G'}, // GM106:Banjo
    {  17, 17,  6,  0,246,242, 65,230,1,2, 52,  0,'G'}, // GM107:Shamisen
    { 147,145,145,  0,212,235, 50, 17,0,1, 56,  0,'G'}, // GM108:Koto
    {   4,  1, 79,  0,250,194, 86,  5,0,0, 60,  0,'G'}, // GM109:Kalimba
    {  33, 34, 73,  0,124,111, 32, 12,0,1, 54,  0,'T'}, // GM110:Bagpipe
    {  49, 33,133,  0,221, 86, 51, 22,1,0, 58,  0,'S'}, // GM111:Fiddle
    {  32, 33,  4,129,218,143,  5, 11,2,0, 54,  0,'S'}, // GM112:Shanai
    {   5,  3,106,128,241,195,229,229,0,0, 54,  0,'b'}, // GM113:Tinkle Bell
    {   7,  2, 21,  0,236,248, 38, 22,0,0, 58,  0,'b'}, // GM114:Agogo Bells
    {   5,  1,157,  0,103,223, 53,  5,0,0, 56,  0,'b'}, // GM115:Steel Drums
    {  24, 18,150,  0,250,248, 40,229,0,0, 58,  0,'b'}, // GM116:Woodblock
    {  16,  0,134,  3,168,250,  7,  3,0,0, 54,  0,'T'}, // GM117:Taiko Drum
    {  17, 16, 65,  3,248,243, 71,  3,2,0, 52,  0,'T'}, // GM118:Melodic Tom
    {   1, 16,142,  0,241,243,  6,  2,2,0, 62,  0,'T'}, // GM119:Synth Drum
    {  14,192,  0,  0, 31, 31,  0,255,0,3, 62,  0,'c'}, // GM120:Reverse Cymbal
    {   6,  3,128,136,248, 86, 36,132,0,2, 62,  0,'G'}, // GM121:Guitar FretNoise
    {  14,208,  0,  5,248, 52,  0,  4,0,3, 62,  0,'X'}, // GM122:Breath Noise
    {  14,192,  0,  0,246, 31,  0,  2,0,3, 62,  0,'X'}, // GM123:Seashore
    { 213,218,149, 64, 55, 86,163, 55,0,0, 48,  0,'X'}, // GM124:Bird Tweet
    {  53, 20, 92,  8,178,244, 97, 21,2,0, 58,  0,'X'}, // GM125:Telephone
    {  14,208,  0,  0,246, 79,  0,245,0,3, 62,  0,'X'}, // GM126:Helicopter
    {  38,228,  0,  0,255, 18,  1, 22,0,1, 62,  0,'X'}, // GM127:Applause/Noise
    {   0,  0,  0,  0,243,246,240,201,0,2, 62,  0,'X'}, // GM128:Gunshot
    {  16, 17, 68,  0,248,243,119,  6,2,0, 56, 35,'D'}, // GP35:Ac Bass Drum
    {  16, 17, 68,  0,248,243,119,  6,2,0, 56, 35,'D'}, // GP36:Bass Drum 1
    {   2, 17,  7,  0,249,248,255,255,0,0, 56, 52,'D'}, // GP37:Side Stick
    {   0,  0,  0,  0,252,250,  5, 23,2,0, 62, 48,'s'}, // GP38:Acoustic Snare
    {   0,  1,  2,  0,255,255,  7,  8,0,0, 48, 58,'h'}, // GP39:Hand Clap
    {   0,  0,  0,  0,252,250,  5, 23,2,0, 62, 60,'s'}, // GP40:Electric Snare
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 47,'T'}, // GP41:Low Floor Tom
    {  12, 18,  0,  0,246,251,  8, 71,0,2, 58, 43,'h'}, // GP42:Closed High Hat
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 49,'T'}, // GP43:High Floor Tom
    {  12, 18,  0,  5,246,123,  8, 71,0,2, 58, 43,'h'}, // GP44:Pedal High Hat
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 51,'T'}, // GP45:Low Tom
    {  12, 18,  0,  0,246,203,  2, 67,0,2, 58, 43,'h'}, // GP46:Open High Hat
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 54,'T'}, // GP47:Low-Mid Tom
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 57,'T'}, // GP48:High-Mid Tom
    {  14,208,  0,  0,246,159,  0,  2,0,3, 62, 72,'C'}, // GP49:Crash Cymbal 1
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 60,'T'}, // GP50:High Tom
    {  14,  7,  8, 74,248,244, 66,228,0,3, 62, 76,'C'}, // GP51:Ride Cymbal 1
    {  14,208,  0, 10,245,159, 48,  2,0,0, 62, 84,'C'}, // GP52:Chinese Cymbal
    {  14,  7, 10, 93,228,245,228,229,3,1, 54, 36,'b'}, // GP53:Ride Bell
    {   2,  5,  3, 10,180,151,  4,247,0,0, 62, 65,'T'}, // GP54:Tambourine
    {  78,158,  0,  0,246,159,  0,  2,0,3, 62, 84,'C'}, // GP55:Splash Cymbal
    {  17, 16, 69,  8,248,243, 55,  5,2,0, 56, 83,'B'}, // GP56:Cow Bell
    {  14,208,  0,  0,246,159,  0,  2,0,3, 62, 84,'C'}, // GP57:Crash Cymbal 2
    { 128, 16,  0, 13,255,255,  3, 20,3,0, 60, 24,'D'}, // GP58:Vibraslap
    {  14,  7,  8, 74,248,244, 66,228,0,3, 62, 77,'C'}, // GP59:Ride Cymbal 2
    {   6,  2, 11,  0,245,245, 12,  8,0,0, 54, 60,'T'}, // GP60:High Bongo
    {   1,  2,  0,  0,250,200,191,151,0,0, 55, 65,'T'}, // GP61:Low Bongo
    {   1,  1, 81,  0,250,250,135,183,0,0, 54, 59,'D'}, // GP62:Mute High Conga
    {   1,  2, 84,  0,250,248,141,184,0,0, 54, 51,'D'}, // GP63:Open High Conga
    {   1,  2, 89,  0,250,248,136,182,0,0, 54, 45,'D'}, // GP64:Low Conga
    {   1,  0,  0,  0,249,250, 10,  6,3,0, 62, 71,'T'}, // GP65:High Timbale
    {   0,  0,128,  0,249,246,137,108,3,0, 62, 60,'T'}, // GP66:Low Timbale
    {   3, 12,128,  8,248,246,136,182,3,0, 63, 58,'D'}, // GP67:High Agogo
    {   3, 12,133,  0,248,246,136,182,3,0, 63, 53,'D'}, // GP68:Low Agogo
    {  14,  0, 64,  8,118,119, 79, 24,0,2, 62, 64,'D'}, // GP69:Cabasa
    {  14,  3, 64,  0,200,155, 73,105,0,2, 62, 71,'D'}, // GP70:Maracas
    { 215,199,220,  0,173,141,  5,  5,3,0, 62, 61,'D'}, // GP71:Short Whistle
    { 215,199,220,  0,168,136,  4,  4,3,0, 62, 61,'D'}, // GP72:Long Whistle
    { 128, 17,  0,  0,246,103,  6, 23,3,3, 62, 44,'D'}, // GP73:Short Guiro
    { 128, 17,  0,  9,245, 70,  5, 22,2,3, 62, 40,'D'}, // GP74:Long Guiro
    {   6, 21, 63,  0,  0,247,244,245,0,0, 49, 69,'D'}, // GP75:Claves
    {   6, 18, 63,  0,  0,247,244,245,3,0, 48, 68,'D'}, // GP76:High Wood Block
    {   6, 18, 63,  0,  0,247,244,245,0,0, 49, 63,'D'}, // GP77:Low Wood Block
    {   1,  2, 88,  0,103,117,231,  7,0,0, 48, 74,'D'}, // GP78:Mute Cuica
    {  65, 66, 69,  8,248,117, 72,  5,0,0, 48, 60,'D'}, // GP79:Open Cuica
    {  10, 30, 64, 78,224,255,240,  5,3,0, 56, 80,'D'}, // GP80:Mute Triangle
    {  10, 30,124, 82,224,255,240,  2,3,0, 56, 64,'D'}, // GP81:Open Triangle
    {  14,  0, 64,  8,122,123, 74, 27,0,2, 62, 72,'D'}, // GP82
    {  14,  7, 10, 64,228, 85,228, 57,3,1, 54, 73,'D'}, // GP83
    {   5,  4,  5, 64,249,214, 50,165,3,0, 62, 70,'D'}, // GP84
    {   2, 21, 63,  0,  0,247,243,245,3,0, 56, 68,'D'}, // GP85
    {   1,  2, 79,  0,250,248,141,181,0,0, 55, 48,'D'}, // GP86
    {   0,  0,  0,  0,246,246, 12,  6,0,0, 52, 53,'D'}  // GP87 (low-low-mid tom?)
};

static void AddMonoAudio(unsigned long count, int* samples);
static void AddStereoAudio(unsigned long count, int* samples);

static class UI
{
    int x, y, color, txtline;
    char slots[80][18], background[80][18+1];
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
    void PrintLn(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        char Line[512];
        int nchars = std::vsnprintf(Line, sizeof(Line), fmt, ap);
        va_end(ap);

        HideCursor();
        GotoXY(0,txtline);
        Color(8);
        for(x=0; x<nchars && x < 80; ++x)
            std::fputc( background[x][txtline] = Line[x], stderr);
        for(int tmp=0; tmp<3 && x<80 && background[x][txtline]!='.'; ++x,++tmp)
        {
            Color(1);
            std::fputc( background[x][txtline] = '.', stderr);
        }
        std::fflush(stderr);

        txtline=1 + (txtline)%18;
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
    {
        HideCursor();
        int notex = (note+55)%79;
        int notey = adlchn;
        char illustrate_char = background[notex][notey+1];
        if(pressure)
        {
            illustrate_char = adl[ins][12];
            if(bend < 0) illustrate_char = '<';
            if(bend > 0) illustrate_char = '>';
        }
        if(slots[notex][notey] != illustrate_char)
        {
            slots[notex][notey] = illustrate_char;
            GotoXY(notex, notey+1);
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
            if(newx == 0) std::fputc('\r', stderr);
            else if(newx < x) std::fprintf(stderr, "\33[%dD", x-newx);
            else if(newx > x) std::fprintf(stderr, "\33[%dC", newx-x);
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
            static const char shuffle[] = {9,10,11,12,13,14,15};
            return ins_colors[ins] = shuffle[ins_color_counter++ % 7];
        }
    }
} UI;

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
        unsigned char patch, bank;
        double        bend;
        int           volume;
        int           expression;
        int           panning;
        int           vibrato;
        struct NoteInfo
        {
            int adlchn; // adlib channel
            int vol;    // pressure
            int ins;    // instrument selected on noteon
        };
        std::map<int/*note number*/, NoteInfo> activenotes;
        int lastlrpn,lastmrpn; bool nrpn;

        MIDIchannel()
            : patch(0), bank(0), bend(0), volume(127),expression(127), panning(0),
              vibrato(0), activenotes(),
              lastlrpn(0),lastmrpn(0),nrpn(false) { }
    } Ch[16];
    // Additional information about AdLib channels
    struct AdlChannel
    {
        // For channel allocation:
        bool   on;
        double age;
        // For collisions
        int    midichn;
        int    note;

        AdlChannel(): on(false),age(0), midichn(0),note(0) { }
    } ch[18];

    std::vector< std::vector<unsigned char> > TrackData;
public:
    double InvDeltaTicks, Tempo, BendSense;
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
        BendSense     = 2.0 / 8192;
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
         std::map<int, MIDIchannel::NoteInfo>::iterator i,
         unsigned props_mask)
    {
        // Determine the instrument and the note value (tone)
        int c = i->second.adlchn, tone = i->first, ins = i->second.ins;
        if(MidCh == 9) tone = adl[ins][11]; // Percussion always uses constant tone
        // (MIDI channel 9 always plays percussion and ignores the patch number)

        if(props_mask & Upd_Off) // note off
        {
            ch[c].on  = false;
            ch[c].age = 0.0;
            opl.NoteOff(c);
            Ch[MidCh].activenotes.erase(i);
            UI.IllustrateNote(c, tone, ins, 0, 0.0);
        }
        else
        {
            ch[c].on  = true;
            ch[c].age = 0.0;
            UI.IllustrateNote(c, tone, ins, i->second.vol, Ch[MidCh].bend);
        }
        if(props_mask & Upd_Patch ) opl.Patch(c, ins);
        if(props_mask & Upd_Pan   ) opl.Pan(c,   Ch[MidCh].panning);
        if(props_mask & Upd_Volume) opl.Touch(c, i->second.vol * Ch[MidCh].volume * Ch[MidCh].expression);
        if(props_mask & Upd_Pitch )
        {
            double bend = Ch[MidCh].bend;
            if(Ch[MidCh].vibrato)
                bend += (Ch[MidCh].vibrato * (0.5/127)) * std::sin(VibPos);
            opl.NoteOn(c, 172.00093 * std::exp(0.057762265 * (tone + bend)));
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
        //fprintf(stderr, "Shortest: %ld --> %g [tempo=%g]\n", shortest, shortest*Tempo, Tempo);
        double t = shortest * Tempo;
        if(CurrentPosition.began) CurrentPosition.wait += t;
        for(unsigned a=0; a<18; ++a) ch[a].age += t;

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
                double bs = -9;
                unsigned c = 0, i = Ch[MidCh].patch;
                if(MidCh==9)
                    i = 128 + note - 35;
                for(unsigned a=0; a<18; ++a)
                {
                    double s = ch[a].age;         // Age in seconds
                    if(!ch[a].on) s += 3000.0;    // Empty channel = privileged
                    if(i == opl.ins[a]) s += 0.02; // Same instrument = good
                    if(a == MidCh) s += 0.02;
                    if(i<128 && opl.ins[a]>127) s=s*2+9; // Percussion is inferior to melody
                    if(s > bs) { bs=s; c = a; } // Best candidate wins
                }
                if(ch[c].on) NoteOff(ch[c].midichn, ch[c].note); // Collision: Kill old note
                // Allocate active note for MIDI channel
                std::pair<std::map<int,MIDIchannel::NoteInfo>::iterator,bool>
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
                std::map<int,MIDIchannel::NoteInfo>::iterator
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
                        Ch[MidCh].bank = (Ch[MidCh].bank & 0xFF) | (value<<8); break;
                    case 32: // Set bank lsb
                        Ch[MidCh].bank = (Ch[MidCh].bank & 0xFF00) | (value); break;
                    case 7: // Change volume
                        Ch[MidCh].volume = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 11: // Change expression (another volume factor)
                        Ch[MidCh].expression = value;
                        NoteUpdate_All(MidCh, Upd_Volume);
                        break;
                    case 10: // Change panning
                        Ch[MidCh].panning = (value<48)?32:((value>79)?16:0);
                        NoteUpdate_All(MidCh, Upd_Pan);
                        break;
                    case 121: // Reset all controllers
                        Ch[MidCh].bend    = 0;
                        Ch[MidCh].volume  = 127;
                        Ch[MidCh].vibrato = 0;
                        Ch[MidCh].panning = 0;
                        NoteUpdate_All(MidCh, Upd_Pan+Upd_Volume+Upd_Pitch);
                        break;
                    case 123: // All notes off
                        NoteUpdate_All(MidCh, Upd_Off);
                        break;
                    case 98: Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=true; break;
                    case 99: Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=true; break;
                    case 100:Ch[MidCh].lastlrpn=value; Ch[MidCh].nrpn=false; break;
                    case 101:Ch[MidCh].lastmrpn=value; Ch[MidCh].nrpn=false; break;
                    case 6: SetRPN(MidCh, value, true); break;
                    case 38: SetRPN(MidCh, value, false); break;
                    // Other ctrls worth considering:
                    //  64 = sustain pedal (how does it even work?)
                    default:
                        UI.PrintLn("Ctrl %d <- %d", ctrlno, value);
                }
                break;
            }
            case 0xC: // Patch change
                Ch[MidCh].patch = TrackData[tk][CurrentPosition.track[tk].ptr++];
                if(Ch[MidCh].bank)
                {
                    UI.PrintLn("Bank %u ignored", Ch[MidCh].bank);
                }
                break;
            case 0xD: // Channel after-touch
            {
                // TODO: Verify, is this correct action?
                int  vol = TrackData[tk][CurrentPosition.track[tk].ptr++];
                for(std::map<int,MIDIchannel::NoteInfo>::iterator
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
                Ch[MidCh].bend = (a + b*128 - 8192) * BendSense;
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
            case 0x20000: BendSense = value/8192.0; break;
            default: UI.PrintLn("%s %04X <- %d (%cSB)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB]);
        }
    }

    void NoteOff(unsigned MidCh, int note)
    {
        std::map<int,MIDIchannel::NoteInfo>::iterator
            i = Ch[MidCh].activenotes.find(note);
        if(i != Ch[MidCh].activenotes.end())
            NoteUpdate(MidCh, i, Upd_Off);
    }
    void NoteUpdate_All(unsigned MidCh, unsigned props_mask)
    {
        for(std::map<int,MIDIchannel::NoteInfo>::iterator
            i = Ch[MidCh].activenotes.begin();
            i != Ch[MidCh].activenotes.end();
            )
        {
            std::map<int,MIDIchannel::NoteInfo>::iterator j(i++);
            NoteUpdate(MidCh, j, props_mask);
        }
    }
};

/* REVERB CODE BEGIN */
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
                .7,   // room_scale   (0..1)
                .6,   // reverberance (0..1)
                .5,   // hf_damping   (0..1)
                .000, // pre_delay_s  (0.. 0.5)
                1.0,  // stereo_depth (0..1)
                MaxSamplesAtTime);
    }
} reverb_data;
/* REVERB CODE END */

static std::deque<short> AudioBuffer;
static pthread_mutex_t AudioBuffer_lock = PTHREAD_MUTEX_INITIALIZER;

static void SDL_AudioCallback(void*, Uint8* stream, int len)
{
    SDL_LockAudio();
    short* target = (short*) stream;
    pthread_mutex_lock(&AudioBuffer_lock);
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len/2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a=0; a<ate; ++a)
        target[a] = AudioBuffer[a];
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    //fprintf(stderr, " - remain %u\n", (unsigned) AudioBuffer.size()/2);
    pthread_mutex_unlock(&AudioBuffer_lock);
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
    // Convert input to float format
    std::vector<float> dry[2];
    for(unsigned w=0; w<2; ++w)
    {
        dry[w].resize(count);
        for(unsigned long p = 0; p < count; ++p)
            dry[w][p] = (samples[p*2+w] - average_flt[w]) * double(0.4/32768.0);
        reverb_data.chan[w].input_fifo.insert(
        reverb_data.chan[w].input_fifo.end(),
            dry[w].begin(), dry[w].end());
    }
    // Reverbify it
    for(unsigned w=0; w<2; ++w)
        reverb_data.chan[w].Process(count);

    // Convert to signed 16-bit int format and put to playback queue
    pthread_mutex_lock(&AudioBuffer_lock);
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
    pthread_mutex_unlock(&AudioBuffer_lock);
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

    if(argc != 2)
    {
        std::fprintf(stderr, "Usage: midiplay <midifilename>\n");
        return 0;
    }

    MIDIplay player;

    if(!player.LoadMIDI(argv[1]))
        return 2;

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
