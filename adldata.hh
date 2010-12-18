static const unsigned char adl[][12] =
// The data bytes are:
//  [0,1] AM/VIB/EG/KSR/Multiple bits for carrier and modulator respectively
//  [2,3] Attack and decay rates      for carrier and modulator respectively
//  [4,5] Sustain and release rates   for carrier and modulator respectively
//  [6,7] Wave select settings        for carrier and modulator respectively
//  [8,9] KSL/Attenuation settings    for carrier and modulator respectively
//  [10]  Feedback/connection bits    for the channel
//  [11]  For percussive instruments (GP35..GP87), the tone to play
//  [12]  Drawing symbol
{
    { 0x01,0x01,0xF2,0xF2,0xF4,0xF7,0,0,0x8F,0x06,0x08,'P' }, // 0: GM0:AcouGrandPiano
    { 0x01,0x01,0xF2,0xF2,0xF4,0xF7,0,0,0x4B,0x00,0x08,'P' }, // 1: GM1:BrightAcouGrand
    { 0x01,0x01,0xF2,0xF2,0xF4,0xF6,0,0,0x49,0x00,0x08,'P' }, // 2: GM2:ElecGrandPiano
    { 0x81,0x41,0xF2,0xF2,0xF7,0xF7,0,0,0x12,0x00,0x06,'P' }, // 3: GM3:Honky-tonkPiano
    { 0x01,0x01,0xF1,0xF2,0xF7,0xF7,0,0,0x57,0x00,0x00,'P' }, // 4: GM4:Rhodes Piano
    { 0x01,0x01,0xF1,0xF2,0xF7,0xF7,0,0,0x93,0x00,0x00,'P' }, // 5: GM5:Chorused Piano
    { 0x01,0x16,0xA1,0xF2,0xF2,0xF5,0,0,0x80,0x0E,0x08,'h' }, // 6: GM6:Harpsichord
    { 0x01,0x01,0xC2,0xC2,0xF8,0xF8,0,0,0x92,0x00,0x0A,'c' }, // 7: GM7:Clavinet
    { 0x0C,0x81,0xF6,0xF3,0xF4,0xF5,0,0,0x5C,0x00,0x00,'c' }, // 8: GM8:Celesta
    { 0x07,0x11,0xF3,0xF2,0xF2,0xF1,0,0,0x97,0x80,0x02,'k' }, // 9: GM9:Glockenspiel
    { 0x17,0x01,0x54,0xF4,0xF4,0xF4,0,0,0x21,0x00,0x02,'m' }, // 10: GM10:Music box
    { 0x98,0x81,0xF3,0xF2,0xF6,0xF6,0,0,0x62,0x00,0x00,'v' }, // 11: GM11:Vibraphone
    { 0x18,0x01,0xF6,0xE7,0xF6,0xF7,0,0,0x23,0x00,0x00,'m' }, // 12: GM12:Marimba
    { 0x15,0x01,0xF6,0xF6,0xF6,0xF6,0,0,0x91,0x00,0x04,'x' }, // 13: GM13:Xylophone
    { 0x45,0x81,0xD3,0xA3,0xF3,0xF3,0,0,0x59,0x80,0x0C,'b' }, // 14: GM14:Tubular Bells
    { 0x03,0x81,0x75,0xB5,0xF5,0xF5,1,0,0x49,0x80,0x04,'d' }, // 15: GM15:Dulcimer
    { 0x71,0x31,0xF6,0xF1,0x14,0x07,0,0,0x92,0x00,0x02,'o' }, // 16: GM16:Hammond Organ; HMIGM16; am016.in
    { 0x72,0x30,0xC7,0xC7,0x58,0x08,0,0,0x14,0x00,0x02,'o' }, // 17: GM17:Percussive Organ; HMIGM17; am017.in
    { 0x70,0xB1,0xAA,0x8A,0x18,0x08,0,0,0x44,0x00,0x04,'o' }, // 18: GM18:Rock Organ; HMIGM18; am018.in
    { 0x23,0xB1,0x97,0x55,0x23,0x14,1,0,0x93,0x00,0x04,'o' }, // 19: GM19:Church Organ; HMIGM19; am019.in
    { 0x61,0xB1,0x97,0x55,0x04,0x04,1,0,0x13,0x80,0x00,'o' }, // 20: GM20:Reed Organ; HMIGM20; am020.in
    { 0x24,0xB1,0x98,0x46,0x2A,0x1A,1,0,0x48,0x00,0x0C,'a' }, // 21: GM21:Accordion; HMIGM21; am021.in
    { 0x61,0x21,0x91,0x61,0x06,0x07,1,0,0x13,0x00,0x0A,'h' }, // 22: GM22:Harmonica; HMIGM22; am022.in
    { 0x21,0xA1,0x71,0x61,0x06,0x07,0,0,0x13,0x89,0x06,'o' }, // 23: GM23:Tango Accordion; HMIGM23; am023.in
    { 0x02,0x41,0xF3,0xF3,0x94,0xC8,1,0,0x9C,0x80,0x0C,'G' }, // 24: GM24:Acoustic Guitar1; HMIGM24; am024.in
    { 0x03,0x11,0xF3,0xF1,0x9A,0xE7,1,0,0x54,0x00,0x0C,'G' }, // 25: GM25:Acoustic Guitar2; HMIGM25; am025.in
    { 0x23,0x21,0xF1,0xF2,0x3A,0xF8,0,0,0x5F,0x00,0x00,'G' }, // 26: GM26:Electric Guitar1; HMIGM26; am026.in
    { 0x03,0x21,0xF6,0xF3,0x22,0xF8,1,0,0x87,0x80,0x06,'G' }, // 27: GM27:Electric Guitar2
    { 0x03,0x21,0xF9,0xF6,0x54,0x3A,0,0,0x47,0x00,0x00,'G' }, // 28: BPerc.in; BPercin; GM28:Electric Guitar3; HMIGM28; am028.in; hamM3; hamM60; intM3; muteguit; rickM3
    { 0x23,0x21,0x91,0x84,0x41,0x19,1,0,0x4A,0x05,0x08,'G' }, // 29: GM29:Overdrive Guitar
    { 0x23,0x21,0x95,0x94,0x19,0x19,1,0,0x4A,0x00,0x08,'G' }, // 30: GDist.in; GDistin; GM30:Distorton Guitar; HMIGM30; am030.in; hamM6; intM6; rickM6
    { 0x09,0x84,0x20,0xD1,0x4F,0xF8,0,0,0xA1,0x80,0x08,'G' }, // 31: GFeedbck; GM31:Guitar Harmonics; HMIGM31; am031.in; hamM5; intM5; rickM5
    { 0x21,0xA2,0x94,0xC3,0x06,0xA6,0,0,0x1E,0x00,0x02,'B' }, // 32: GM32:Acoustic Bass; HMIGM32; am032.in
    { 0x31,0x31,0xF1,0xF1,0x28,0x18,0,0,0x12,0x00,0x0A,'B' }, // 33: GM33:Electric Bass 1; GM39:Synth Bass 2; HMIGM33; HMIGM39; am033.in; am039.in; hamM68; synbass2
    { 0x31,0x31,0xF1,0xF1,0xE8,0x78,0,0,0x8D,0x00,0x0A,'B' }, // 34: GM34:Electric Bass 2; HMIGM34; Slapbass; am034.in; rickM81
    { 0x31,0x32,0x51,0x71,0x28,0x48,0,0,0x5B,0x00,0x0C,'B' }, // 35: GM35:Fretless Bass; HMIGM35; am035.in
    { 0x01,0x21,0xA1,0xF2,0x9A,0xDF,0,0,0x8B,0x40,0x08,'B' }, // 36: GM36:Slap Bass 1; HMIGM36; am036.in
    { 0x21,0x21,0xA2,0xA1,0x16,0xDF,0,0,0x8B,0x08,0x08,'B' }, // 37: GM37:Slap Bass 2
    { 0x31,0x31,0xF4,0xF1,0xE8,0x78,0,0,0x8B,0x00,0x0A,'B' }, // 38: BSynth3; BSynth3.; GM38:Synth Bass 1; HMIGM38; am038.in; hamM13; hamM67; intM13; rickM13; synbass1
    { 0x31,0x21,0xDD,0x56,0x13,0x26,1,0,0x15,0x00,0x08,'V' }, // 39: GM40:Violin; HMIGM40; am040.in
    { 0x31,0x21,0xDD,0x66,0x13,0x06,1,0,0x16,0x00,0x08,'V' }, // 40: GM41:Viola; HMIGM41; am041.in
    { 0x71,0x31,0xD1,0x61,0x1C,0x0C,1,0,0x49,0x00,0x08,'V' }, // 41: GM42:Cello; HMIGM42; am042.in
    { 0x21,0x23,0x71,0x72,0x12,0x06,1,0,0x4D,0x80,0x02,'V' }, // 42: GM43:Contrabass; HMIGM43; am043.in
    { 0xF1,0xE1,0xF1,0x6F,0x21,0x16,1,0,0x40,0x00,0x02,'V' }, // 43: GM44:Tremulo Strings; HMIGM44; am044.in
    { 0x02,0x01,0xF5,0x85,0x75,0x35,1,0,0x1A,0x80,0x00,'H' }, // 44: GM45:Pizzicato String; HMIGM45; am045.in
    { 0x02,0x01,0xF5,0xF3,0x75,0xF4,1,0,0x1D,0x80,0x00,'H' }, // 45: GM46:Orchestral Harp; HMIGM46; am046.in
    { 0x10,0x11,0xF5,0xF2,0x05,0xC3,1,0,0x41,0x00,0x02,'M' }, // 46: BSynth4; BSynth4.; GM47:Timpany; HMIGM47; am047.in; hamM14; intM14; rickM14
    { 0x21,0xA2,0xB1,0x72,0x25,0x08,1,0,0x9B,0x01,0x0E,'S' }, // 47: GM48:String Ensemble1; HMIGM48; am048.in
    { 0xA1,0x21,0x7F,0x3F,0x03,0x07,1,1,0x98,0x00,0x00,'S' }, // 48: GM49:String Ensemble2; HMIGM49; am049.in
    { 0xA1,0x61,0xC1,0x4F,0x12,0x05,0,0,0x93,0x00,0x0A,'S' }, // 49: GM50:Synth Strings 1; HMIGM50; PMellow; PMellow.; am050.in; hamM20; intM20; rickM20
    { 0x21,0x61,0xC1,0x4F,0x22,0x05,0,0,0x18,0x00,0x0C,'S' }, // 50: GM51:SynthStrings 2; HMIGM51; am051.in
    { 0x31,0x72,0xF4,0x8A,0x15,0x05,0,0,0x5B,0x83,0x00,'O' }, // 51: Choir.in; GM52:Choir Aahs; HMIGM52; am052.in; rickM85
    { 0xA1,0x61,0x74,0x71,0x39,0x67,0,0,0x90,0x00,0x00,'O' }, // 52: GM53:Voice Oohs; HMIGM53; Oohs.ins; am053.in; rickM86
    { 0x71,0x72,0x54,0x7A,0x05,0x05,0,0,0x57,0x00,0x0C,'O' }, // 53: GM54:Synth Voice; HMIGM54; am054.in
    { 0x90,0x41,0x54,0xA5,0x63,0x45,0,0,0x00,0x00,0x08,'c' }, // 54: GM55:Orchestra Hit; HMIGM55; am055.in
    { 0x21,0x21,0x85,0x8F,0x17,0x09,0,0,0x92,0x01,0x0C,'T' }, // 55: GM56:Trumpet; HMIGM56; am056.in
    { 0x21,0x21,0x75,0x8F,0x17,0x09,0,0,0x94,0x05,0x0C,'T' }, // 56: GM57:Trombone; HMIGM57; am057.in
    { 0x21,0x61,0x76,0x82,0x15,0x37,0,0,0x94,0x00,0x0C,'T' }, // 57: GM58:Tuba; HMIGM58; am058.in
    { 0x31,0x21,0x9E,0x62,0x17,0x2C,1,1,0x43,0x00,0x02,'T' }, // 58: GM59:Muted Trumpet; HMIGM59; am059.in
    { 0x21,0x21,0x61,0x7F,0x6A,0x0A,0,0,0x9B,0x00,0x02,'T' }, // 59: GM60:French Horn; HMIGM60; am060.in
    { 0x61,0x22,0x75,0x74,0x1F,0x0F,0,0,0x8A,0x06,0x08,'T' }, // 60: GM61:Brass Section; HMIGM61; am061.in
    { 0xA1,0x21,0x72,0x71,0x55,0x18,1,0,0x86,0x83,0x00,'T' }, // 61: GM62:Synth Brass 1
    { 0x21,0x21,0x54,0xA6,0x3C,0x1C,0,0,0x4D,0x00,0x08,'T' }, // 62: GM63:Synth Brass 2; HMIGM63; am063.in
    { 0x31,0x61,0x93,0x72,0x02,0x0B,1,0,0x8F,0x00,0x08,'X' }, // 63: GM64:Soprano Sax; HMIGM64; am064.in
    { 0x31,0x61,0x93,0x72,0x03,0x09,1,0,0x8E,0x00,0x08,'X' }, // 64: GM65:Alto Sax; HMIGM65; am065.in
    { 0x31,0x61,0x93,0x82,0x03,0x09,1,0,0x91,0x00,0x0A,'X' }, // 65: GM66:Tenor Sax; HMIGM66; am066.in
    { 0x31,0x61,0x93,0x72,0x0F,0x0F,1,0,0x8E,0x00,0x0A,'X' }, // 66: GM67:Baritone Sax; HMIGM67; am067.in
    { 0x21,0x21,0xAA,0x8F,0x16,0x0A,1,0,0x4B,0x00,0x08,'T' }, // 67: GM68:Oboe; HMIGM68; am068.in
    { 0x31,0x21,0x7E,0x8B,0x17,0x0C,1,1,0x90,0x00,0x06,'T' }, // 68: GM69:English Horn; HMIGM69; am069.in
    { 0x31,0x32,0x75,0x61,0x19,0x19,1,0,0x81,0x00,0x00,'T' }, // 69: GM70:Bassoon; HMIGM70; am070.in
    { 0x32,0x21,0x9B,0x72,0x21,0x17,0,0,0x90,0x00,0x04,'F' }, // 70: GM71:Clarinet; HMIGM71; am071.in
    { 0xE1,0xE1,0x85,0x65,0x5F,0x1A,0,0,0x1F,0x00,0x00,'F' }, // 71: GM72:Piccolo; HMIGM72; am072.in
    { 0xE1,0xE1,0x88,0x65,0x5F,0x1A,0,0,0x46,0x00,0x00,'F' }, // 72: GM73:Flute; HMIGM73; am073.in
    { 0xA1,0x21,0x75,0x75,0x1F,0x0A,0,0,0x9C,0x00,0x02,'F' }, // 73: GM74:Recorder; HMIGM74; am074.in
    { 0x31,0x21,0x84,0x65,0x58,0x1A,0,0,0x8B,0x00,0x00,'F' }, // 74: GM75:Pan Flute; HMIGM75; am075.in
    { 0xE1,0xA1,0x66,0x65,0x56,0x26,0,0,0x4C,0x00,0x00,'F' }, // 75: GM76:Bottle Blow; HMIGM76; am076.in
    { 0x62,0xA1,0x76,0x55,0x46,0x36,0,0,0xCB,0x00,0x00,'F' }, // 76: GM77:Shakuhachi; HMIGM77; am077.in
    { 0x62,0xA1,0x57,0x56,0x07,0x07,0,0,0x99,0x00,0x0B,'F' }, // 77: GM78:Whistle; HMIGM78; am078.in
    { 0x62,0xA1,0x77,0x76,0x07,0x07,0,0,0x93,0x00,0x0B,'F' }, // 78: GM79:Ocarina; HMIGM79; am079.in; hamM61; ocarina
    { 0x22,0x21,0xFF,0xFF,0x03,0x0F,2,0,0x59,0x00,0x00,'L' }, // 79: GM80:Lead 1 squareea; HMIGM80; LSquare; LSquare.; am080.in; hamM16; hamM65; intM16; rickM16; squarewv
    { 0x21,0x21,0xFF,0xFF,0x0F,0x0F,1,1,0x0E,0x00,0x00,'L' }, // 80: GM81:Lead 2 sawtooth; HMIGM81; am081.in
    { 0x22,0x21,0x86,0x64,0x55,0x18,0,0,0x46,0x80,0x00,'L' }, // 81: GM82:Lead 3 calliope; HMIGM82; am082.in
    { 0x21,0xA1,0x66,0x96,0x12,0x0A,0,0,0x45,0x00,0x00,'L' }, // 82: GM83:Lead 4 chiff; HMIGM83; am083.in
    { 0x21,0x22,0x92,0x91,0x2A,0x2A,1,0,0x8B,0x00,0x00,'L' }, // 83: GM84:Lead 5 charang; HMIGM84; am084.in
    { 0xA2,0x61,0xDF,0x6F,0x05,0x07,0,0,0x9E,0x40,0x02,'L' }, // 84: GM85:Lead 6 voice; HMIGM85; PFlutes; PFlutes.; Solovox.; am085.in; hamM17; intM17; rickM17; rickM87
    { 0x20,0x60,0xEF,0x8F,0x01,0x06,0,2,0x1A,0x00,0x00,'L' }, // 85: GM86:Lead 7 fifths; HMIGM86; Saw_wave; am086.in; rickM93
    { 0x21,0x21,0xF1,0xF4,0x29,0x09,0,0,0x8F,0x80,0x0A,'L' }, // 86: GM87:Lead 8 brass; HMIGM87; am087.in
    { 0x77,0xA1,0x53,0xA0,0x94,0x05,0,0,0xA5,0x00,0x02,'p' }, // 87: GM88:Pad 1 new age; HMIGM88; am088.in
    { 0x61,0xB1,0xA8,0x25,0x11,0x03,0,0,0x1F,0x80,0x0A,'p' }, // 88: GM89:Pad 2 warm; HMIGM89; am089.in
    { 0x61,0x61,0x91,0x55,0x34,0x16,0,0,0x17,0x00,0x0C,'p' }, // 89: GM90:Pad 3 polysynth; HMIGM90; LTriang; LTriang.; am090.in; hamM21; intM21; rickM21
    { 0x71,0x72,0x54,0x6A,0x01,0x03,0,0,0x5D,0x00,0x00,'p' }, // 90: GM91:Pad 4 choir; HMIGM91; Spacevo.; am091.in; rickM95
    { 0x21,0xA2,0x21,0x42,0x43,0x35,0,0,0x97,0x00,0x08,'p' }, // 91: GM92:Pad 5 bowedpad; HMIGM92; am092.in
    { 0xA1,0x21,0xA1,0x31,0x77,0x47,1,1,0x1C,0x00,0x00,'p' }, // 92: GM93:Pad 6 metallic; HMIGM93; PSlow.in; PSlowin; am093.in; hamM22; intM22; rickM22
    { 0x21,0x61,0x11,0x42,0x33,0x25,0,0,0x89,0x03,0x0A,'p' }, // 93: GM94:Pad 7 halo; HMIGM94; Halopad.; PSweep.i; PSweepi; am094.in; halopad; hamM23; hamM54; intM23; rickM23; rickM96
    { 0xA1,0x21,0x11,0xCF,0x47,0x07,1,0,0x15,0x00,0x00,'p' }, // 94: GM95:Pad 8 sweep; HMIGM95; Sweepad.; am095.in; hamM66; rickM97; sweepad
    { 0x3A,0x51,0xF8,0x86,0xF6,0x02,0,0,0xCE,0x00,0x02,'X' }, // 95: GM96:FX 1 rain; HMIGM96; am096.in
    { 0x21,0x21,0x21,0x41,0x23,0x13,1,0,0x15,0x00,0x00,'X' }, // 96: GM97:FX 2 soundtrack; HMIGM97; am097.in
    { 0x06,0x01,0x74,0xA5,0x95,0x72,0,0,0x5B,0x00,0x00,'X' }, // 97: GM98:FX 3 crystal; HMIGM98; am098.in
    { 0x22,0x61,0xB1,0xF2,0x81,0x26,0,0,0x92,0x83,0x0C,'X' }, // 98: GM99:FX 4 atmosphere; HMIGM99; am099.in
    { 0x41,0x42,0xF1,0xF2,0x51,0xF5,1,0,0x4D,0x00,0x00,'X' }, // 99: GM100:FX 5 brightness; HMIGM100; am100.in; am100in; hamM51
    { 0x61,0xA3,0x11,0x11,0x51,0x13,1,0,0x94,0x80,0x06,'X' }, // 100: GM101:FX 6 goblins; HMIGM101; am101.in
    { 0x61,0xA1,0x11,0x1D,0x31,0x03,0,0,0x8C,0x80,0x06,'X' }, // 101: Echodrp1; GM102:FX 7 echoes; HMIGM102; am102.in; rickM98
    { 0xA4,0x61,0xF3,0x81,0x73,0x23,1,0,0x4C,0x00,0x04,'X' }, // 102: GM103:FX 8 sci-fi; HMIGM103; am103.in
    { 0x02,0x07,0xD2,0xF2,0x53,0xF6,0,1,0x85,0x03,0x00,'G' }, // 103: GM104:Sitar; HMIGM104; am104.in
    { 0x11,0x13,0xA3,0xA2,0x11,0xE5,1,0,0x0C,0x80,0x00,'G' }, // 104: GM105:Banjo; HMIGM105; am105.in
    { 0x11,0x11,0xF6,0xF2,0x41,0xE6,1,2,0x06,0x00,0x04,'G' }, // 105: GM106:Shamisen; HMIGM106; LDist.in; LDistin; am106.in; hamM24; intM24; rickM24
    { 0x93,0x91,0xD4,0xEB,0x32,0x11,0,1,0x91,0x00,0x08,'G' }, // 106: GM107:Koto; HMIGM107; am107.in
    { 0x04,0x01,0xFA,0xC2,0x56,0x05,0,0,0x4F,0x00,0x0C,'G' }, // 107: GM108:Kalimba; HMIGM108; am108.in; hamM57; kalimba
    { 0x21,0x22,0x7C,0x6F,0x20,0x0C,0,1,0x49,0x00,0x06,'T' }, // 108: GM109:Bagpipe; HMIGM109; am109.in
    { 0x31,0x21,0xDD,0x56,0x33,0x16,1,0,0x85,0x00,0x0A,'S' }, // 109: GM110:Fiddle; HMIGM110; am110.in
    { 0x20,0x21,0xDA,0x8F,0x05,0x0B,2,0,0x04,0x81,0x06,'S' }, // 110: GM111:Shanai; HMIGM111; am111.in
    { 0x05,0x03,0xF1,0xC3,0xE5,0xE5,0,0,0x6A,0x80,0x06,'b' }, // 111: GM112:Tinkle Bell; HMIGM112; am112.in
    { 0x07,0x02,0xEC,0xF8,0x26,0x16,0,0,0x15,0x00,0x0A,'b' }, // 112: GM113:Agogo Bells; HMIGM113; agogoin; am113.in; hamM50
    { 0x05,0x01,0x67,0xDF,0x35,0x05,0,0,0x9D,0x00,0x08,'b' }, // 113: GM114:Steel Drums; HMIGM114; am114.in
    { 0x18,0x12,0xFA,0xF8,0x28,0xE5,0,0,0x96,0x00,0x0A,'b' }, // 114: GM115:Woodblock; HMIGM115; Woodblk.; am115.in; rickM100
    { 0x10,0x00,0xA8,0xFA,0x07,0x03,0,0,0x86,0x03,0x06,'M' }, // 115: GM116:Taiko Drum; HMIGM116; Taikoin; am116.in; hamM69; hamP90; taikoin
    { 0x11,0x10,0xF8,0xF3,0x47,0x03,2,0,0x41,0x03,0x04,'M' }, // 116: GM117:Melodic Tom; HMIGM117; am117.in; hamM58; melotom
    { 0x01,0x10,0xF1,0xF3,0x06,0x02,2,0,0x8E,0x00,0x0E,'M' }, // 117: GM118:Synth Drum; HMIGM118; am118.in
    { 0x0E,0xC0,0x1F,0x1F,0x00,0xFF,0,3,0x00,0x00,0x0E,'c' }, // 118: GM119:Reverse Cymbal; HMIGM119; am119.in
    { 0x06,0x03,0xF8,0x56,0x24,0x84,0,2,0x80,0x88,0x0E,'G' }, // 119: DNoise1; DNoise1.; Fretnos.; GM120:Guitar FretNoise; HMIGM120; am120.in; hamM36; intM36; rickM101; rickM36
    { 0x0E,0xD0,0xF8,0x34,0x00,0x04,0,3,0x00,0x05,0x0E,'X' }, // 120: GM121:Breath Noise; HMIGM121; am121.in
    { 0x0E,0xC0,0xF6,0x1F,0x00,0x02,0,3,0x00,0x00,0x0E,'X' }, // 121: GM122:Seashore; HMIGM122; am122.in
    { 0xD5,0xDA,0x37,0x56,0xA3,0x37,0,0,0x95,0x40,0x00,'X' }, // 122: GM123:Bird Tweet; HMIGM123; am123.in
    { 0x35,0x14,0xB2,0xF4,0x61,0x15,2,0,0x5C,0x08,0x0A,'X' }, // 123: GM124:Telephone; HMIGM124; am124.in
    { 0x0E,0xD0,0xF6,0x4F,0x00,0xF5,0,3,0x00,0x00,0x0E,'X' }, // 124: GM125:Helicopter; HMIGM125; am125.in
    { 0x26,0xE4,0xFF,0x12,0x01,0x16,0,1,0x00,0x00,0x0E,'X' }, // 125: GM126:Applause/Noise; HMIGM126; am126.in
    { 0x00,0x00,0xF3,0xF6,0xF0,0xC9,0,2,0x00,0x00,0x0E,'X' }, // 126: GM127:Gunshot; HMIGM127; am127.in
    { 0x10,0x11,0xF8,0xF3,0x77,0x06,2,0,0x44,0x00,0x08,'D' }, // 127: GP35:Ac Bass Drum; GP36:Bass Drum 1; IntP34; IntP35; apo035.i; apo035i; aps035i; hamP11; hamP34; hamP35; kick2.in; rickP14; rickP34; rickP35
    { 0x02,0x11,0xF9,0xF8,0xFF,0xFF,0,0,0x07,0x00,0x08,'D' }, // 128: GP37:Side Stick
    { 0x00,0x00,0xFC,0xFA,0x05,0x17,2,0,0x00,0x00,0x0E,'s' }, // 129: GP38:Acoustic Snare; GP40:Electric Snare
    { 0x00,0x01,0xFF,0xFF,0x07,0x08,0,0,0x02,0x00,0x00,'h' }, // 130: GP39:Hand Clap
    { 0x00,0x00,0xF6,0xF6,0x0C,0x06,0,0,0x00,0x00,0x04,'M' }, // 131: GP41:Low Floor Tom; GP43:High Floor Tom; GP45:Low Tom; GP47:Low-Mid Tom; GP48:High-Mid Tom; GP50:High Tom; GP87; aps041i; hamP1; hamP2; hamP3; hamP4; hamP5; hamP6; rickP105; surdu.in
    { 0x0C,0x12,0xF6,0xFB,0x08,0x47,0,2,0x00,0x00,0x0A,'h' }, // 132: GP42:Closed High Hat; IntP55; aps042.i; aps042i; hamP55; rickP55
    { 0x0C,0x12,0xF6,0x7B,0x08,0x47,0,2,0x00,0x05,0x0A,'h' }, // 133: GP44:Pedal High Hat
    { 0x0C,0x12,0xF6,0xCB,0x02,0x43,0,2,0x00,0x00,0x0A,'h' }, // 134: GP46:Open High Hat
    { 0x0E,0xD0,0xF6,0x9F,0x00,0x02,0,3,0x00,0x00,0x0E,'C' }, // 135: GP49:Crash Cymbal 1; GP57:Crash Cymbal 2; crash1i; hamP0
    { 0x0E,0x07,0xF8,0xF4,0x42,0xE4,0,3,0x08,0x4A,0x0E,'C' }, // 136: GP51:Ride Cymbal 1; GP59:Ride Cymbal 2
    { 0x0E,0xD0,0xF5,0x9F,0x30,0x02,0,0,0x00,0x0A,0x0E,'C' }, // 137: GP52:Chinese Cymbal; aps052i; hamP19
    { 0x0E,0x07,0xE4,0xF5,0xE4,0xE5,3,1,0x0A,0x5D,0x06,'b' }, // 138: GP53:Ride Bell
    { 0x02,0x05,0xB4,0x97,0x04,0xF7,0,0,0x03,0x0A,0x0E,'M' }, // 139: GP54:Tambourine
    { 0x4E,0x9E,0xF6,0x9F,0x00,0x02,0,3,0x00,0x00,0x0E,'C' }, // 140: GP55:Splash Cymbal
    { 0x11,0x10,0xF8,0xF3,0x37,0x05,2,0,0x45,0x08,0x08,'B' }, // 141: GP56:Cow Bell
    { 0x80,0x10,0xFF,0xFF,0x03,0x14,3,0,0x00,0x0D,0x0C,'D' }, // 142: GP58:Vibraslap
    { 0x06,0x02,0xF5,0xF5,0x0C,0x08,0,0,0x0B,0x00,0x06,'M' }, // 143: GP60:High Bongo
    { 0x01,0x02,0xFA,0xC8,0xBF,0x97,0,0,0x00,0x00,0x07,'M' }, // 144: GP61:Low Bongo
    { 0x01,0x01,0xFA,0xFA,0x87,0xB7,0,0,0x51,0x00,0x06,'D' }, // 145: GP62:Mute High Conga
    { 0x01,0x02,0xFA,0xF8,0x8D,0xB8,0,0,0x54,0x00,0x06,'D' }, // 146: GP63:Open High Conga
    { 0x01,0x02,0xFA,0xF8,0x88,0xB6,0,0,0x59,0x00,0x06,'D' }, // 147: GP64:Low Conga
    { 0x01,0x00,0xF9,0xFA,0x0A,0x06,3,0,0x00,0x00,0x0E,'M' }, // 148: GP65:High Timbale; hamP8; rickP98; rickP99; timbale; timbale.
    { 0x00,0x00,0xF9,0xF6,0x89,0x6C,3,0,0x80,0x00,0x0E,'M' }, // 149: GP66:Low Timbale
    { 0x03,0x0C,0xF8,0xF6,0x88,0xB6,3,0,0x80,0x08,0x0F,'D' }, // 150: GP67:High Agogo
    { 0x03,0x0C,0xF8,0xF6,0x88,0xB6,3,0,0x85,0x00,0x0F,'D' }, // 151: GP68:Low Agogo
    { 0x0E,0x00,0x76,0x77,0x4F,0x18,0,2,0x40,0x08,0x0E,'D' }, // 152: GP69:Cabasa
    { 0x0E,0x03,0xC8,0x9B,0x49,0x69,0,2,0x40,0x00,0x0E,'D' }, // 153: GP70:Maracas
    { 0xD7,0xC7,0xAD,0x8D,0x05,0x05,3,0,0xDC,0x00,0x0E,'D' }, // 154: GP71:Short Whistle
    { 0xD7,0xC7,0xA8,0x88,0x04,0x04,3,0,0xDC,0x00,0x0E,'D' }, // 155: GP72:Long Whistle
    { 0x80,0x11,0xF6,0x67,0x06,0x17,3,3,0x00,0x00,0x0E,'D' }, // 156: GP73:Short Guiro; guiros.i; rickP96
    { 0x80,0x11,0xF5,0x46,0x05,0x16,2,3,0x00,0x09,0x0E,'D' }, // 157: GP74:Long Guiro
    { 0x06,0x15,0x00,0xF7,0xF4,0xF5,0,0,0x3F,0x00,0x01,'D' }, // 158: GP75:Claves
    { 0x06,0x12,0x00,0xF7,0xF4,0xF5,3,0,0x3F,0x00,0x00,'D' }, // 159: GP76:High Wood Block
    { 0x06,0x12,0x00,0xF7,0xF4,0xF5,0,0,0x3F,0x00,0x01,'D' }, // 160: GP77:Low Wood Block
    { 0x01,0x02,0x67,0x75,0xE7,0x07,0,0,0x58,0x00,0x00,'D' }, // 161: GP78:Mute Cuica
    { 0x41,0x42,0xF8,0x75,0x48,0x05,0,0,0x45,0x08,0x00,'D' }, // 162: GP79:Open Cuica
    { 0x0A,0x1E,0xE0,0xFF,0xF0,0x05,3,0,0x40,0x4E,0x08,'D' }, // 163: GP80:Mute Triangle
    { 0x0A,0x1E,0xE0,0xFF,0xF0,0x02,3,0,0x7C,0x52,0x08,'D' }, // 164: GP81:Open Triangle
    { 0x0E,0x00,0x7A,0x7B,0x4A,0x1B,0,2,0x40,0x08,0x0E,'D' }, // 165: GP82; aps082i; hamP7
    { 0x0E,0x07,0xE4,0x55,0xE4,0x39,3,1,0x0A,0x40,0x06,'D' }, // 166: GP83
    { 0x05,0x04,0xF9,0xD6,0x32,0xA5,3,0,0x05,0x40,0x0E,'D' }, // 167: GP84
    { 0x02,0x15,0x00,0xF7,0xF3,0xF5,3,0,0x3F,0x00,0x08,'D' }, // 168: GP85
    { 0x01,0x02,0xFA,0xF8,0x8D,0xB5,0,0,0x4F,0x00,0x07,'D' }, // 169: GP86
    { 0x23,0x21,0x95,0x84,0x19,0x19,1,0,0x48,0x00,0x08,'P' }, // 170: HMIGM0; HMIGM29; am029.in
    { 0x31,0x21,0xF2,0xF2,0x54,0x56,0,0,0x4B,0x00,0x08,'P' }, // 171: HMIGM1; am001.in
    { 0x31,0x21,0xF2,0xF2,0x55,0x76,0,0,0x49,0x00,0x08,'P' }, // 172: HMIGM2; am002.in
    { 0xB1,0x61,0xF2,0xF3,0x3B,0x0B,0,0,0x0E,0x00,0x06,'P' }, // 173: HMIGM3; am003.in
    { 0x01,0x21,0xF1,0xF1,0x38,0x28,0,0,0x57,0x00,0x00,'P' }, // 174: HMIGM4; am004.in
    { 0x01,0x21,0xF1,0xF1,0x38,0x28,0,0,0x93,0x00,0x00,'P' }, // 175: HMIGM5; am005.in
    { 0x21,0x36,0xA2,0xF1,0x01,0xD5,0,0,0x80,0x0E,0x08,'h' }, // 176: HMIGM6; am006.in
    { 0x01,0x01,0xC2,0xC2,0xA8,0x58,0,0,0x92,0x00,0x0A,'c' }, // 177: HMIGM7; am007.in
    { 0x0C,0x81,0xF6,0xF3,0x54,0xB5,0,0,0x5C,0x00,0x00,'c' }, // 178: HMIGM8; am008.in
    { 0x07,0x11,0xF6,0xF5,0x32,0x11,0,0,0x97,0x80,0x02,'k' }, // 179: HMIGM9; am009.in
    { 0x17,0x01,0x56,0xF6,0x04,0x04,0,0,0x21,0x00,0x02,'m' }, // 180: HMIGM10; am010.in
    { 0x18,0x81,0xF3,0xF2,0xE6,0xF6,0,0,0x62,0x00,0x00,'v' }, // 181: HMIGM11; am011.in
    { 0x18,0x21,0xF7,0xE5,0x55,0xD8,0,0,0x23,0x00,0x00,'m' }, // 182: HMIGM12; am012.in
    { 0x15,0x01,0xF6,0xF6,0xA6,0xE6,0,0,0x91,0x00,0x04,'x' }, // 183: HMIGM13; am013.in
    { 0x45,0x81,0xD3,0xA3,0x82,0xE3,0,0,0x59,0x80,0x0C,'b' }, // 184: HMIGM14; am014.in
    { 0x03,0x81,0x74,0xB3,0x55,0x05,1,0,0x49,0x80,0x04,'d' }, // 185: HMIGM15; am015.in
    { 0x03,0x21,0xF6,0xF3,0x22,0xF3,1,0,0x87,0x80,0x06,'G' }, // 186: HMIGM27; am027.in
    { 0x01,0x21,0xA1,0xF2,0x9A,0xDF,0,0,0x89,0x40,0x08,'B' }, // 187: HMIGM37; am037.in
    { 0xA1,0x21,0x72,0x71,0x55,0x18,1,0,0x86,0x0D,0x00,'T' }, // 188: HMIGM62; am062.in
    { 0x10,0x10,0x00,0x00,0xF0,0xF0,0,0,0x3F,0x3F,0x00,'?' }, // 189: Blank.in; HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99
    { 0x2E,0x07,0xF0,0x74,0xF1,0x48,0,3,0x00,0x07,0x08,'?' }, // 190: HMIGP27; HMIGP28; HMIGP29; HMIGP30; HMIGP31; Wierd1.i
    { 0x29,0x0E,0x52,0x85,0xFE,0xD9,0,3,0x00,0x07,0x06,'?' }, // 191: HMIGP32; HMIGP33; HMIGP34; Wierd2.i
    { 0x27,0x0A,0x62,0x95,0xFE,0xD9,0,3,0x00,0x07,0x08,'D' }, // 192: HMIGP35; Wierd3.i
    { 0x00,0x01,0xF8,0xF6,0xFD,0xC7,0,0,0x0B,0x00,0x08,'D' }, // 193: HMIGP36; Kick.ins
    { 0x16,0x11,0xF1,0xF9,0xFB,0x69,0,0,0x08,0x02,0x00,'D' }, // 194: HMIGP37; HMIGP85; HMIGP86; RimShot.; rimShot.; rimshot.
    { 0x26,0x02,0xFF,0xF8,0x00,0xA7,0,0,0x00,0x02,0x0E,'s' }, // 195: HMIGP38; HMIGP40; Snare.in
    { 0x30,0x3A,0x9F,0xE8,0x0F,0xFA,0,0,0x00,0x00,0x0E,'h' }, // 196: Clap.ins; HMIGP39
    { 0x06,0x11,0xFA,0xF5,0x1F,0xF5,0,0,0x0A,0x00,0x0F,'M' }, // 197: HMIGP41; HMIGP43; HMIGP45; HMIGP47; HMIGP48; HMIGP50; Toms.ins
    { 0x2E,0x06,0xF5,0x99,0xF1,0xF9,0,3,0x05,0x02,0x00,'h' }, // 198: HMIGP42; HMIGP44; clshat97
    { 0x27,0x0A,0x92,0x75,0xF8,0xD8,0,3,0x00,0x03,0x08,'h' }, // 199: HMIGP46; Opnhat96
    { 0x2C,0x0E,0x9F,0xC5,0x00,0xA4,2,3,0x00,0x09,0x0E,'C' }, // 200: Crashcym; HMIGP49; HMIGP52; HMIGP55; HMIGP57
    { 0x29,0x04,0x94,0xF9,0x00,0x44,0,3,0x10,0x0C,0x0E,'C' }, // 201: HMIGP51; HMIGP53; HMIGP59; Ridecym.; ridecym.
    { 0x2E,0x06,0xF5,0x87,0xF1,0xF7,0,3,0x09,0x02,0x00,'M' }, // 202: HMIGP54; Tamb.ins
    { 0x35,0x03,0xF5,0xF7,0xF1,0x28,2,0,0x19,0x02,0x00,'B' }, // 203: Cowbell.; HMIGP56
    { 0x80,0x00,0xFF,0xF5,0x00,0xF7,1,1,0x00,0x00,0x0C,'D' }, // 204: HMIGP58; vibrasla
    { 0x25,0x03,0xFA,0x98,0xFA,0xF9,0,0,0xCD,0x00,0x00,'M' }, // 205: HMIGP60; HMIGP62; mutecong
    { 0x25,0x03,0xF8,0xA8,0xFA,0xF7,1,0,0x1B,0x00,0x00,'M' }, // 206: HMIGP61; conga.in
    { 0x25,0x03,0xF8,0x96,0xFA,0xF6,1,0,0x21,0x00,0x0E,'D' }, // 207: HMIGP63; HMIGP64; loconga.
    { 0x04,0x03,0xF5,0xF6,0xF5,0x36,2,2,0x16,0x03,0x0A,'M' }, // 208: HMIGP65; HMIGP66; timbale.
    { 0x15,0x17,0xF0,0xA6,0x91,0xE8,0,0,0x1E,0x04,0x0E,'D' }, // 209: HMIGP67; HMIGP68; agogo.in
    { 0x0E,0x0E,0xFF,0x78,0x01,0x77,0,0,0x06,0x04,0x0E,'D' }, // 210: HMIGP69; HMIGP70; HMIGP82; shaker.i
    { 0x20,0x0E,0x9F,0x95,0x07,0x2B,2,2,0x1C,0x00,0x00,'D' }, // 211: HMIGP71; hiwhist.
    { 0x20,0x0E,0x9F,0x94,0x07,0x3B,2,2,0x1E,0x00,0x00,'D' }, // 212: HMIGP72; lowhist.
    { 0x20,0x09,0xF0,0x88,0xF7,0x3B,0,3,0x00,0x00,0x0C,'D' }, // 213: HMIGP73; higuiro.
    { 0x20,0x0A,0xF4,0x56,0xF7,0x3B,0,3,0x03,0x00,0x00,'D' }, // 214: HMIGP74; loguiro.
    { 0x14,0x12,0xF7,0xF7,0x5B,0x89,0,0,0x4B,0x00,0x00,'D' }, // 215: HMIGP75; clavecb.
    { 0x27,0x12,0xFA,0xF6,0xF2,0x9A,0,0,0x22,0x00,0x00,'D' }, // 216: HMIGP76; HMIGP77; woodblok
    { 0x20,0x08,0x50,0x77,0xF7,0x3B,1,0,0x09,0x05,0x00,'D' }, // 217: HMIGP78; hicuica.
    { 0x26,0x03,0x7F,0x77,0x07,0x6B,1,0,0x29,0x05,0x00,'D' }, // 218: HMIGP79; locuica.
    { 0x26,0x27,0xF1,0xF7,0xF0,0xFC,0,0,0x44,0x40,0x06,'D' }, // 219: HMIGP80; mutringl
    { 0x26,0x27,0xF1,0xF5,0xF0,0xF5,0,0,0x44,0x40,0x06,'D' }, // 220: HMIGP81; HMIGP83; HMIGP84; triangle
    { 0x21,0x20,0xF8,0xC6,0xF3,0xAD,0,0,0x1C,0x00,0x0C,'D' }, // 221: HMIGP87; taiko.in
    { 0x01,0x01,0xFF,0xFF,0xFF,0xFF,0,0,0x3F,0x3F,0x00,'P' }, // 222: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; nosound; nosound.; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9
    { 0x03,0x04,0xEE,0xE8,0xFF,0xFF,4,0,0x80,0x00,0x0C,'P' }, // 223: DBlock.i; DBlocki; hamM1; intM1; rickM1
    { 0x03,0xA1,0xF6,0xF3,0x22,0xF8,1,0,0x87,0x80,0x06,'P' }, // 224: GClean.i; GCleani; hamM2; intM2; rickM2
    { 0x10,0x00,0xA8,0xFA,0x07,0x05,0,0,0x86,0x03,0x06,'P' }, // 225: DToms.in; DTomsin; hamM4; intM4; rickM4
    { 0x31,0x32,0xF1,0xF2,0x53,0x27,0,2,0x48,0x00,0x06,'c' }, // 226: GOverD.i; GOverDi; Guit_fz2; hamM7; intM7; rickM7; rickM84
    { 0x61,0xE6,0x91,0xC1,0x1A,0x1A,0,0,0x40,0x03,0x08,'c' }, // 227: GMetal.i; GMetali; hamM8; intM8; rickM8
    { 0x11,0x01,0xFB,0xF3,0x71,0xB9,0,0,0x00,0x00,0x00,'k' }, // 228: BPick.in; BPickin; hamM9; intM9; rickM9
    { 0x31,0x23,0x72,0xD5,0xB5,0x98,1,0,0x0B,0x00,0x08,'m' }, // 229: BSlap.in; BSlapin; hamM10; intM10; rickM10
    { 0x01,0x11,0xD5,0x85,0x24,0x22,0,0,0x0F,0x00,0x0A,'v' }, // 230: BSynth1; BSynth1.; hamM11; intM11; rickM11
    { 0x11,0x31,0xF9,0xF1,0x25,0x34,0,0,0x05,0x00,0x0A,'m' }, // 231: BSynth2; BSynth2.; hamM12; intM12; rickM12
    { 0xA1,0x61,0x76,0x82,0x15,0x37,0,0,0x94,0x00,0x0C,'d' }, // 232: PSoft.in; PSoftin; hamM15; intM15; rickM15
    { 0x61,0x22,0x22,0x4F,0x36,0x08,1,0,0x10,0x40,0x08,'o' }, // 233: PRonStr1; hamM18; intM18; rickM18
    { 0x60,0x22,0x33,0x4F,0x36,0x08,2,0,0x15,0x40,0x0C,'o' }, // 234: PRonStr2; hamM19; intM19; rickM19
    { 0x04,0x01,0xF8,0x82,0x07,0x74,0,0,0x0E,0x05,0x06,'G' }, // 235: LTrap.in; LTrapin; hamM25; intM25; rickM25
    { 0x31,0x31,0xF1,0xF1,0xE5,0x74,0,1,0x89,0x00,0x0C,'G' }, // 236: LSaw.ins; LSawins; hamM26; intM26; rickM26
    { 0x31,0x72,0xF1,0xF1,0xE3,0x73,0,0,0x8A,0x00,0x0A,'G' }, // 237: PolySyn; PolySyn.; hamM27; intM27; rickM27
    { 0x01,0x91,0xF1,0x50,0xFF,0xFF,0,0,0x0D,0x80,0x06,'G' }, // 238: Pobo.ins; Poboins; hamM28; intM28; rickM28
    { 0x61,0x61,0x31,0xAF,0x47,0x07,1,0,0x0F,0x00,0x0A,'G' }, // 239: PSweep2; PSweep2.; hamM29; intM29; rickM29
    { 0x03,0x04,0xB3,0xF2,0xD3,0x24,0,0,0x40,0x80,0x04,'G' }, // 240: LBright; LBright.; hamM30; intM30; rickM30
    { 0x31,0x62,0x75,0x54,0x03,0x44,1,0,0x1A,0x40,0x0E,'G' }, // 241: SynStrin; hamM31; intM31; rickM31
    { 0xA1,0x61,0xA1,0xC2,0x21,0x16,0,1,0x92,0x40,0x06,'B' }, // 242: SynStr2; SynStr2.; hamM32; intM32; rickM32
    { 0x40,0x40,0xF2,0xF4,0xF0,0xF4,0,0,0x00,0x00,0x04,'B' }, // 243: hamM33; intM33; low_blub; rickM33
    { 0xC0,0x7E,0xF1,0x10,0x03,0x01,0,0,0x4F,0x0C,0x02,'B' }, // 244: DInsect; DInsect.; hamM34; intM34; rickM34
    { 0xC0,0xC1,0x9B,0xF9,0x45,0x15,0,0,0x05,0x00,0x0E,'B' }, // 245: hamM35; hardshak; intM35; rickM35
    { 0x50,0x50,0x4F,0xFF,0x06,0x03,0,0,0x10,0x00,0x00,'B' }, // 246: WUMP.ins; WUMPins; hamM37; intM37; rickM37
    { 0x05,0x00,0xF0,0xF6,0xF0,0xB4,2,1,0x08,0x00,0x0C,'B' }, // 247: DSnare.i; DSnarei; hamM38; intM38; rickM38
    { 0x31,0x10,0xF9,0xF2,0xF2,0x42,0,0,0x40,0x00,0x04,'B' }, // 248: DTimp.in; DTimpin; hamM39; intM39; rickM39
    { 0x7E,0x6E,0xFF,0x2F,0x0F,0x0F,0,0,0x00,0x00,0x0E,'V' }, // 249: DRevCym; DRevCym.; IntP93; drevcym; hamM40; intM40; rickM40
    { 0x01,0x01,0x54,0x54,0xF9,0xFB,2,2,0x19,0x00,0x08,'V' }, // 250: Dorky.in; Dorkyin; hamM41; intM41; rickM41
    { 0x53,0x00,0x5F,0x7F,0x66,0x07,0,0,0x05,0x00,0x06,'V' }, // 251: DFlab.in; DFlabin; hamM42; intM42; rickM42
    { 0xC0,0x7E,0xF1,0x70,0x03,0x06,0,0,0x4F,0x03,0x02,'V' }, // 252: DInsect2; hamM43; intM43; rickM43
    { 0xF0,0xE2,0x1E,0x11,0x11,0x14,1,1,0x00,0xC0,0x08,'V' }, // 253: DChopper; hamM44; intM44; rickM44
    { 0x06,0x84,0xA0,0xC5,0xF0,0x75,0,0,0x00,0x00,0x0E,'H' }, // 254: DShot.in; DShotin; IntP50; hamM45; hamP50; intM45; rickM45; rickP50; shot.ins; shotins
    { 0x13,0x11,0xF2,0xF1,0xF5,0xF5,1,0,0xC6,0x05,0x00,'H' }, // 255: KickAss; KickAss.; hamM46; intM46; rickM46
    { 0x01,0x11,0xF1,0xF1,0x53,0x74,1,2,0x49,0x02,0x06,'M' }, // 256: RVisCool; hamM47; intM47; rickM47
    { 0xD0,0x9E,0xF4,0xA2,0xE4,0x06,0,0,0x80,0x00,0x08,'S' }, // 257: DSpring; DSpring.; hamM48; intM48; rickM48
    { 0x21,0x21,0x13,0x42,0x87,0x08,0,0,0xCD,0x80,0x08,'S' }, // 258: Chorar22; intM49
    { 0x10,0x11,0xF0,0xD0,0x05,0x04,0,0,0x25,0x80,0x0E,'?' }, // 259: IntP27; hamP27; rickP27; timpani; timpani.
    { 0x00,0x00,0xB4,0xB4,0x65,0x75,0,0,0x00,0x00,0x07,'?' }, // 260: IntP28; hamP28; rickP28; timpanib
    { 0x00,0x00,0xF8,0xF6,0x2A,0x45,0,1,0x03,0x00,0x04,'?' }, // 261: APS043.i; APS043i; IntP29; hamP29; rickP29
    { 0x30,0x11,0xF8,0xF5,0xFF,0x7F,0,0,0x44,0x00,0x08,'?' }, // 262: IntP30; hamP30; mgun3.in; mgun3in; rickP30
    { 0x00,0x10,0xF9,0xF1,0xF9,0x23,0,0,0x08,0x00,0x0A,'?' }, // 263: IntP31; hamP31; kick4r.i; kick4ri; rickP31
    { 0x00,0x80,0xF9,0xF1,0xF9,0x26,0,0,0x04,0x00,0x08,'?' }, // 264: IntP32; hamP32; rickP32; timb1r.i; timb1ri
    { 0x00,0x00,0xF8,0xF8,0xFD,0x59,1,0,0xC4,0x00,0x0A,'?' }, // 265: IntP33; hamP33; rickP33; timb2r.i; timb2ri
    { 0x00,0x00,0xFA,0xF6,0x6F,0x8F,0,0,0x0B,0x00,0x00,'D' }, // 266: IntP36; hamP36; hartbeat; rickP36
    { 0x00,0x91,0xF9,0xF1,0xF9,0x23,0,0,0x04,0x00,0x08,'D' }, // 267: IntP37; IntP38; IntP39; IntP40; hamP37; hamP38; hamP39; hamP40; rickP37; rickP38; rickP39; rickP40; tom1r.in; tom1rin
    { 0x02,0x02,0xC8,0xC8,0x97,0x97,0,0,0x00,0x00,0x01,'M' }, // 268: IntP41; IntP42; IntP43; IntP44; IntP71; hamP71; rickP41; rickP42; rickP43; rickP44; rickP71; tom2.ins; tom2ins; woodbloc
    { 0x01,0x02,0xFA,0xDA,0xBF,0xBF,0,0,0x00,0x00,0x08,'M' }, // 269: IntP45; hamP45; rickP45; tom.ins; tomins
    { 0x01,0x01,0xFB,0xF7,0xF0,0x96,2,0,0x10,0x00,0x0E,'h' }, // 270: IntP46; IntP47; conga.in; congain; hamP46; hamP47; rickP46; rickP47
    { 0x04,0x00,0xFF,0xFF,0x02,0x07,0,0,0x00,0x00,0x0E,'M' }, // 271: IntP48; hamP48; rickP48; snare01r
    { 0x06,0x00,0xF0,0xF6,0xF0,0xB7,0,0,0x00,0x00,0x0C,'C' }, // 272: IntP49; hamP49; rickP49; slap.ins; slapins
    { 0x06,0xC4,0xF0,0xC4,0xF0,0x34,0,0,0x00,0x03,0x0E,'C' }, // 273: IntP51; hamP51; rickP51; snrsust; snrsust.
    { 0x19,0x20,0xF0,0xB7,0xF0,0xF7,0,0,0x0E,0x0A,0x0E,'C' }, // 274: IntP52; rickP52; snare.in; snarein
    { 0x06,0x00,0xF0,0xF6,0xF0,0xB4,0,0,0x00,0x00,0x0E,'b' }, // 275: IntP53; hamP53; rickP53; synsnar; synsnar.
    { 0x06,0x00,0xF0,0xF8,0xF0,0xB6,0,0,0x00,0x00,0x0E,'M' }, // 276: IntP54; hamP54; rickP54; synsnr1; synsnr1.
    { 0x31,0x10,0xF9,0xF2,0xF2,0x08,0,0,0x40,0x00,0x04,'B' }, // 277: IntP56; hamP56; rickP56; rimshotb
    { 0x01,0x09,0xFA,0xDA,0xBF,0xBF,0,0,0x00,0x08,0x08,'C' }, // 278: IntP57; hamP57; rickP57; rimshot; rimshot.
    { 0x2E,0x0E,0xBA,0xB4,0x10,0xF4,2,2,0x0E,0x00,0x0E,'D' }, // 279: IntP58; IntP59; crash.in; crashin; hamP58; hamP59; rickP58; rickP59
    { 0x2E,0x0E,0xFA,0xF4,0x10,0xF4,2,2,0x0E,0x00,0x0E,'M' }, // 280: IntP60; cymbal.i; cymbali; rickP60
    { 0xA4,0x03,0xB2,0x97,0xA2,0xD4,2,1,0x02,0x80,0x0E,'M' }, // 281: IntP61; cymbali; cymbals; cymbals.; hamP60; hamP61; rickP61
    { 0x04,0xC3,0xFF,0xFF,0x00,0x06,2,2,0x00,0x00,0x08,'D' }, // 282: IntP62; hamP62; hammer5r; rickP62
    { 0x04,0xC3,0xFF,0xF6,0x00,0xF5,2,2,0x00,0x00,0x08,'D' }, // 283: IntP63; IntP64; hamP63; hamP64; hammer3; hammer3.; rickP63; rickP64
    { 0x00,0x1E,0xC0,0x95,0xE1,0x53,0,1,0x80,0x80,0x06,'M' }, // 284: IntP65; hamP65; rickP65; ride2.in; ride2in
    { 0x03,0xC4,0xFF,0xF6,0x00,0xF6,2,3,0x00,0x00,0x08,'M' }, // 285: IntP66; hamP66; hammer1; hammer1.; rickP66
    { 0x4E,0x01,0xFF,0xF7,0x02,0xF7,2,3,0x00,0x00,0x08,'D' }, // 286: IntP67; IntP68; rickP67; rickP68; tambour; tambour.
    { 0x4E,0x01,0xFF,0xF6,0x02,0xF6,2,3,0x00,0x00,0x08,'D' }, // 287: IntP69; IntP70; hamP69; hamP70; rickP69; rickP70; tambou2; tambou2.
    { 0x51,0x52,0x8A,0xA4,0x58,0x18,2,0,0x00,0x00,0x0C,'D' }, // 288: IntP72; hamP72; rickP72; woodblok
    { 0x13,0x08,0xFB,0xE8,0xFF,0xFF,0,0,0x40,0x00,0x08,'D' }, // 289: IntP73; IntP74; claves.i; clavesi; rickP73; rickP74
    { 0x03,0x08,0xEE,0xE8,0xFF,0xFF,0,0,0x40,0x00,0x0C,'D' }, // 290: IntP75; claves2; claves2.; hamP75; rickP75
    { 0x05,0x08,0xEE,0xE8,0xFF,0xFF,0,0,0x55,0x00,0x0E,'D' }, // 291: IntP76; claves3; claves3.; hamP76; rickP76
    { 0x06,0x15,0x00,0xF7,0xFF,0xFD,0,0,0x3F,0x0D,0x01,'D' }, // 292: IntP77; clave.in; clavein; hamP77; rickP77
    { 0x0E,0x0E,0xF8,0xF8,0xF6,0xF6,0,0,0x00,0x00,0x00,'D' }, // 293: IntP78; IntP79; agogob4; agogob4.; hamP78; hamP79; rickP78; rickP79
    { 0x07,0x12,0xF2,0xF2,0x60,0x72,0,0,0x4F,0x09,0x08,'D' }, // 294: IntP80; clarion; clarion.; hamP80; rickP80
    { 0x17,0x12,0xF2,0xF2,0x61,0x74,0,0,0x4F,0x08,0x08,'D' }, // 295: IntP81; hamP81; rickP81; trainbel
    { 0x18,0x25,0xFB,0xF4,0x22,0x12,0,0,0x88,0x80,0x08,'D' }, // 296: IntP82; gong.ins; gongins; hamP82; rickP82
    { 0x04,0xC1,0xFF,0xF4,0xF0,0xB5,0,0,0x00,0x00,0x0E,'D' }, // 297: IntP83; hamP83; kalimbai; rickP83
    { 0x11,0x31,0xC8,0xF5,0x2F,0xF5,0,0,0x2D,0x00,0x0C,'D' }, // 298: IntP84; IntP85; hamP84; hamP85; rickP84; rickP85; xylo1.in; xylo1in
    { 0x09,0x2C,0x67,0x69,0x3D,0xFC,0,3,0x00,0x00,0x0E,'D' }, // 299: IntP86; hamP86; match.in; matchin; rickP86
    { 0x44,0xB2,0xD1,0x42,0x53,0x56,0,0,0x80,0x15,0x0E,'D' }, // 300: IntP87; breathi; breathi.; hamP87; rickP87
    { 0x07,0x00,0xF0,0x5C,0xF0,0xDC,0,0,0x00,0x00,0x0E,'?' }, // 301: IntP88; rickP88; scratch; scratch.
    { 0x7E,0x6E,0xFF,0x3F,0x0F,0x0F,0,0,0x00,0x00,0x0E,'?' }, // 302: IntP89; crowd.in; crowdin; hamP89; rickP89
    { 0x00,0x00,0xFA,0xD0,0xB3,0x05,0,0,0x00,0x00,0x0C,'?' }, // 303: IntP90; rickP90; taiko.in; taikoin
    { 0x32,0x11,0xF8,0xF5,0xFF,0x7F,0,0,0x84,0x00,0x0E,'?' }, // 304: IntP91; hamP91; rickP91; rlog.ins; rlogins
    { 0xD4,0xD4,0x9F,0x9F,0x08,0x08,0,0,0xC0,0xC0,0x04,'?' }, // 305: IntP92; hamP92; knock.in; knockin; rickP92
    { 0xC4,0xD2,0xB1,0xB1,0x53,0x83,2,0,0x8F,0x84,0x02,'O' }, // 306: Fantasy1; fantasy1; hamM52; rickM94
    { 0xC2,0xD1,0xF5,0xF2,0x75,0x74,1,0,0x21,0x83,0x0E,'O' }, // 307: guitar1; hamM53
    { 0x34,0xB1,0xFB,0x94,0xF6,0x43,1,0,0x83,0x00,0x0C,'c' }, // 308: hamM55; hamatmos
    { 0x22,0x21,0x87,0x64,0x65,0x18,0,0,0x46,0x80,0x00,'T' }, // 309: hamM56; hamcalio
    { 0x11,0x04,0xF2,0xA0,0xBD,0x9B,0,0,0x46,0x40,0x08,'T' }, // 310: hamM59; moonins
    { 0x21,0x22,0xF2,0x71,0x44,0x45,1,3,0x8A,0x40,0x00,'T' }, // 311: Polyham3; hamM62
    { 0x21,0x22,0xF2,0x71,0x44,0x44,1,1,0x8A,0x40,0x00,'T' }, // 312: Polyham; hamM63
    { 0x01,0x08,0xF1,0xF1,0x53,0x53,0,1,0x40,0x40,0x00,'X' }, // 313: hamM64; sitar2i
    { 0x00,0x00,0xFF,0xF2,0x02,0xFF,1,3,0x08,0x00,0x00,'T' }, // 314: hamM70; weird1a
    { 0x21,0x22,0xF2,0xA1,0x44,0x45,1,3,0x8A,0x40,0x00,'F' }, // 315: Polyham4; hamM71
    { 0x31,0x31,0xF1,0xF0,0x28,0x18,0,0,0x0F,0x00,0x0A,'F' }, // 316: hamM72; hamsynbs
    { 0xE1,0xE0,0xD7,0xB6,0x07,0x07,3,1,0x8D,0x00,0x01,'F' }, // 317: Ocasynth; hamM73
    { 0x00,0x00,0xD5,0xD5,0x3D,0x2C,0,0,0x11,0x00,0x0A,'G' }, // 318: hamM120; hambass1
    { 0x63,0x20,0x95,0xC4,0x19,0x19,1,2,0x46,0x00,0x08,'X' }, // 319: hamM121; hamguit1
    { 0x8C,0x81,0xD0,0xD1,0x44,0xF4,0,2,0xA1,0x80,0x08,'X' }, // 320: hamM122; hamharm2
    { 0x71,0x31,0x21,0x52,0x02,0x03,0,1,0x93,0x80,0x00,'X' }, // 321: hamM123; hamvox1
    { 0x61,0xA2,0x11,0x11,0x61,0x13,1,0,0x91,0x80,0x08,'X' }, // 322: hamM124; hamgob1
    { 0xE1,0xA1,0x66,0x65,0x56,0x26,2,0,0x4C,0x00,0x00,'X' }, // 323: hamM125; hamblow1
    { 0xC6,0xC1,0xF5,0xF6,0x44,0x18,1,0,0x5C,0x83,0x0E,'?' }, // 324: cowbell; hamP9
    { 0xC0,0xC1,0xCC,0xEA,0xD0,0x28,0,0,0x10,0x00,0x00,'?' }, // 325: conghi.i; conghii; hamP10; rickP100
    { 0x00,0x00,0xFB,0xF4,0x38,0xDA,0,0,0x00,0x00,0x04,'?' }, // 326: hamP12; hamkick; kick3.in; rickP15
    { 0x15,0x17,0xFB,0xF8,0xBF,0x1F,2,3,0x0A,0x00,0x00,'?' }, // 327: hamP13; rimshot2
    { 0x1E,0x0E,0xF0,0xF7,0xF0,0xB6,0,0,0x00,0x00,0x0E,'?' }, // 328: hamP14; hamsnr1; rickP16; snr1.ins
    { 0xC6,0xC9,0xFB,0xE8,0xBF,0x2F,0,0,0x00,0x00,0x0E,'?' }, // 329: hamP15; handclap
    { 0x06,0x00,0xF0,0xF8,0xF0,0xB7,2,2,0x00,0x00,0x0E,'?' }, // 330: hamP16; smallsnr
    { 0x0E,0x0E,0x90,0x99,0xB0,0xBF,0,0,0x03,0x03,0x0A,'?' }, // 331: clsdhhat; hamP17; rickP95
    { 0xC4,0xC0,0xE0,0x88,0x83,0x45,2,1,0x81,0x00,0x0E,'?' }, // 332: hamP18; openhht2; rickP94
    { 0x02,0x00,0xC8,0xE0,0x97,0x40,0,0,0x00,0x00,0x01,'M' }, // 333: hamP41; hamP42; hamP43; hamP44; tom2ins
    { 0x2E,0x02,0xFF,0xF6,0x0F,0x4A,0,0,0x0A,0x1B,0x0E,'C' }, // 334: hamP52; snarein
    { 0xEE,0xCE,0xF0,0x86,0xA5,0x67,3,3,0x00,0x00,0x0E,'D' }, // 335: hamP67; hamP68; tambour
    { 0xD6,0xD6,0xFC,0xBD,0xB0,0x08,0,0,0x00,0x05,0x0A,'D' }, // 336: clavesi; hamP73; hamP74
    { 0x07,0x00,0xF0,0x5C,0xF0,0xDC,0,0,0x08,0x00,0x0E,'?' }, // 337: hamP88; scratch
    { 0x01,0x11,0xF3,0xF2,0xE7,0x78,0,0,0x58,0x00,0x0A,'F' }, // 338: Bass.ins; rickM76
    { 0x30,0x21,0xF2,0xF5,0xEF,0x78,0,0,0x1E,0x00,0x0E,'F' }, // 339: Basnor04; rickM77
    { 0x30,0x71,0xD5,0x61,0x19,0x1B,0,0,0x88,0x80,0x0C,'F' }, // 340: Synbass1; rickM78
    { 0x01,0x11,0xF2,0xF2,0x01,0xB7,0,0,0x0D,0x0D,0x0A,'F' }, // 341: Synbass2; rickM79
    { 0x00,0x00,0xD5,0xD5,0x3D,0x2C,0,0,0x14,0x00,0x0A,'L' }, // 342: Pickbass; rickM80
    { 0x32,0x16,0xE0,0x7D,0x10,0x33,0,0,0x87,0x84,0x08,'L' }, // 343: Harpsi1.; rickM82
    { 0x61,0x62,0xF1,0xF0,0xF8,0x08,0,0,0x80,0x80,0x08,'L' }, // 344: Guit_el3; rickM83
    { 0x91,0x51,0x53,0x54,0x74,0x75,0,0,0x00,0x00,0x0A,'p' }, // 345: Orchit2.; rickM88
    { 0x21,0x31,0x61,0x72,0x8E,0x9E,0,0,0x15,0x00,0x0E,'p' }, // 346: Brass11.; rickM89
    { 0x21,0x21,0x73,0x81,0xBC,0xBC,0,0,0x19,0x00,0x0E,'p' }, // 347: Brass2.i; rickM90
    { 0x20,0x21,0x73,0x81,0x3C,0xBC,2,0,0x19,0x00,0x0E,'p' }, // 348: Brass3.i; rickM91
    { 0x60,0x14,0xA0,0xF0,0x09,0x0F,2,2,0x02,0x80,0x01,'p' }, // 349: Squ_wave; rickM92
    { 0x07,0x12,0xF2,0xF6,0x64,0x75,0,0,0x73,0x00,0x08,'X' }, // 350: Agogo.in; rickM99
    { 0x11,0x11,0xF8,0xF3,0xB7,0x06,2,0,0x46,0x00,0x08,'?' }, // 351: kick1.in; rickP13
    { 0x01,0x00,0xF4,0xF8,0x18,0x08,2,0,0x00,0x00,0x0C,'?' }, // 352: rickP17; rickP19; snare1.i; snare4.i
    { 0x09,0x00,0xF0,0xB7,0xF0,0xF7,0,0,0x0E,0x00,0x0E,'?' }, // 353: rickP18; rickP20; snare2.i; snare5.i
    { 0x12,0x11,0xF8,0xD5,0xFE,0x7E,0,0,0x47,0x00,0x0E,'?' }, // 354: rickP21; rickP22; rickP23; rickP24; rickP25; rickP26; rocktom.
    { 0x0E,0xCA,0xF5,0x20,0xF4,0x41,2,66,0x00,0x51,0x03,'?' }, // 355: openhht1; rickP93
    { 0xC2,0xC0,0xDC,0xC9,0xDF,0x26,0,0,0x17,0x00,0x00,'?' }, // 356: guirol.i; rickP97
    { 0xC0,0xC1,0xAC,0xEA,0xD0,0x28,0,0,0x18,0x00,0x00,'?' }, // 357: congas2.; rickP101; rickP102
    { 0xC2,0xC1,0xCD,0xEA,0xA7,0x28,0,0,0x2B,0x02,0x00,'?' }, // 358: bongos.i; rickP103; rickP104
};
static const struct
{
    unsigned short adlno;
    unsigned char tone;
    unsigned char bit27, byte17;
} adlins[] =
{
    {   0,  0, 1,0x04 }, // 0: GM0:AcouGrandPiano
    {   1,  0, 1,0x04 }, // 1: GM1:BrightAcouGrand
    {   2,  0, 1,0x04 }, // 2: GM2:ElecGrandPiano
    {   3,  0, 1,0x04 }, // 3: GM3:Honky-tonkPiano
    {   4,  0, 1,0x04 }, // 4: GM4:Rhodes Piano
    {   5,  0, 1,0x04 }, // 5: GM5:Chorused Piano
    {   6,  0, 1,0x04 }, // 6: GM6:Harpsichord
    {   7,  0, 1,0x04 }, // 7: GM7:Clavinet
    {   8,  0, 1,0x04 }, // 8: GM8:Celesta
    {   9,  0, 1,0x04 }, // 9: GM9:Glockenspiel
    {  10,  0, 1,0x04 }, // 10: GM10:Music box
    {  11,  0, 1,0x04 }, // 11: GM11:Vibraphone
    {  12,  0, 1,0x04 }, // 12: GM12:Marimba
    {  13,  0, 1,0x04 }, // 13: GM13:Xylophone
    {  14,  0, 1,0x04 }, // 14: GM14:Tubular Bells
    {  15,  0, 1,0x04 }, // 15: GM15:Dulcimer
    {  16,  0, 1,0x04 }, // 16: GM16:Hammond Organ; HMIGM16; am016.in
    {  17,  0, 1,0x04 }, // 17: GM17:Percussive Organ; HMIGM17; am017.in
    {  18,  0, 1,0x62 }, // 18: GM18:Rock Organ; HMIGM18; am018.in
    {  19,  0, 1,0x04 }, // 19: GM19:Church Organ; HMIGM19; am019.in
    {  20,  0, 1,0x62 }, // 20: GM20:Reed Organ; HMIGM20; am020.in
    {  21,  0, 1,0x0C }, // 21: GM21:Accordion; HMIGM21; am021.in
    {  22,  0, 1,0x04 }, // 22: GM22:Harmonica; HMIGM22; am022.in
    {  23,  0, 1,0x04 }, // 23: GM23:Tango Accordion; HMIGM23; am023.in
    {  24,  0, 1,0x04 }, // 24: GM24:Acoustic Guitar1; HMIGM24; am024.in
    {  25,  0, 1,0x04 }, // 25: GM25:Acoustic Guitar2; HMIGM25; am025.in
    {  26,  0, 1,0x04 }, // 26: GM26:Electric Guitar1; HMIGM26; am026.in
    {  27,  0, 1,0x04 }, // 27: GM27:Electric Guitar2
    {  28,  0, 1,0x00 }, // 28: GM28:Electric Guitar3; HMIGM28; am028.in
    {  29,  0, 1,0x04 }, // 29: GM29:Overdrive Guitar
    {  30,  0, 1,0x04 }, // 30: GM30:Distorton Guitar; HMIGM30; am030.in
    {  31,  0, 1,0xEE }, // 31: GM31:Guitar Harmonics; HMIGM31; am031.in
    {  32,  0, 1,0xEE }, // 32: GM32:Acoustic Bass; HMIGM32; am032.in
    {  33,  0, 1,0x0C }, // 33: GM33:Electric Bass 1; HMIGM39; am039.in
    {  34,  0, 1,0x04 }, // 34: GM34:Electric Bass 2; HMIGM34; am034.in
    {  35,  0, 1,0x04 }, // 35: GM35:Fretless Bass; HMIGM35; am035.in
    {  36,  0, 1,0x04 }, // 36: GM36:Slap Bass 1; HMIGM36; am036.in
    {  37,  0, 1,0x04 }, // 37: GM37:Slap Bass 2
    {  38,  0, 1,0x04 }, // 38: GM38:Synth Bass 1; HMIGM38; am038.in
    {  33,  0, 1,0x04 }, // 39: GM39:Synth Bass 2; HMIGM33; am033.in
    {  39,  0, 1,0x0C }, // 40: GM40:Violin; HMIGM40; am040.in
    {  40,  0, 1,0x04 }, // 41: GM41:Viola; HMIGM41; am041.in
    {  41,  0, 1,0x04 }, // 42: GM42:Cello; HMIGM42; am042.in
    {  42,  0, 1,0x04 }, // 43: GM43:Contrabass; HMIGM43; am043.in
    {  43,  0, 1,0x04 }, // 44: GM44:Tremulo Strings; HMIGM44; am044.in
    {  44,  0, 1,0x3D }, // 45: GM45:Pizzicato String; HMIGM45; am045.in
    {  45,  0, 1,0x0C }, // 46: GM46:Orchestral Harp; HMIGM46; am046.in
    {  46,  0, 1,0x04 }, // 47: GM47:Timpany; HMIGM47; am047.in
    {  47,  0, 1,0x04 }, // 48: GM48:String Ensemble1; HMIGM48; am048.in
    {  48,  0, 1,0x00 }, // 49: GM49:String Ensemble2; HMIGM49; am049.in
    {  49,  0, 1,0x04 }, // 50: GM50:Synth Strings 1; HMIGM50; am050.in
    {  50,  0, 1,0x04 }, // 51: GM51:SynthStrings 2; HMIGM51; am051.in
    {  51,  0, 1,0x04 }, // 52: Choir.in; GM52:Choir Aahs; HMIGM52; am052.in; rickM85
    {  52,  0, 1,0x04 }, // 53: GM53:Voice Oohs; HMIGM53; Oohs.ins; am053.in; rickM86
    {  53,  0, 1,0x62 }, // 54: GM54:Synth Voice; HMIGM54; am054.in
    {  54,  0, 1,0x04 }, // 55: GM55:Orchestra Hit; HMIGM55; am055.in
    {  55,  0, 1,0x04 }, // 56: GM56:Trumpet; HMIGM56; am056.in
    {  56,  0, 1,0x04 }, // 57: GM57:Trombone; HMIGM57; am057.in
    {  57,  0, 1,0x00 }, // 58: GM58:Tuba; HMIGM58; am058.in
    {  58,  0, 1,0x62 }, // 59: GM59:Muted Trumpet; HMIGM59; am059.in
    {  59,  0, 1,0x62 }, // 60: GM60:French Horn; HMIGM60; am060.in
    {  60,  0, 1,0x04 }, // 61: GM61:Brass Section; HMIGM61; am061.in
    {  61,  0, 1,0x04 }, // 62: GM62:Synth Brass 1
    {  62,  0, 1,0x62 }, // 63: GM63:Synth Brass 2; HMIGM63; am063.in
    {  63,  0, 1,0x04 }, // 64: GM64:Soprano Sax; HMIGM64; am064.in
    {  64,  0, 1,0x00 }, // 65: GM65:Alto Sax; HMIGM65; am065.in
    {  65,  0, 1,0x00 }, // 66: GM66:Tenor Sax; HMIGM66; am066.in
    {  66,  0, 1,0x62 }, // 67: GM67:Baritone Sax; HMIGM67; am067.in
    {  67,  0, 1,0x04 }, // 68: GM68:Oboe; HMIGM68; am068.in
    {  68,  0, 1,0x04 }, // 69: GM69:English Horn; HMIGM69; am069.in
    {  69,  0, 1,0x04 }, // 70: GM70:Bassoon; HMIGM70; am070.in
    {  70,  0, 1,0x62 }, // 71: GM71:Clarinet; HMIGM71; am071.in
    {  71,  0, 1,0x04 }, // 72: GM72:Piccolo; HMIGM72; am072.in
    {  72,  0, 1,0x3D }, // 73: GM73:Flute; HMIGM73; am073.in
    {  73,  0, 1,0x62 }, // 74: GM74:Recorder; HMIGM74; am074.in
    {  74,  0, 1,0x04 }, // 75: GM75:Pan Flute; HMIGM75; am075.in
    {  75,  0, 1,0x04 }, // 76: GM76:Bottle Blow; HMIGM76; am076.in
    {  76,  0, 1,0x04 }, // 77: GM77:Shakuhachi; HMIGM77; am077.in
    {  77,  0, 0,0x04 }, // 78: GM78:Whistle; HMIGM78; am078.in
    {  78,  0, 0,0x04 }, // 79: GM79:Ocarina; HMIGM79; am079.in
    {  79,  0, 1,0x04 }, // 80: GM80:Lead 1 squareea; HMIGM80; am080.in
    {  80,  0, 1,0xF4 }, // 81: GM81:Lead 2 sawtooth; HMIGM81; am081.in
    {  81,  0, 1,0x04 }, // 82: GM82:Lead 3 calliope; HMIGM82; am082.in
    {  82,  0, 1,0x04 }, // 83: GM83:Lead 4 chiff; HMIGM83; am083.in
    {  83,  0, 1,0x04 }, // 84: GM84:Lead 5 charang; HMIGM84; am084.in
    {  84,  0, 1,0x04 }, // 85: GM85:Lead 6 voice; HMIGM85; Solovox.; am085.in; rickM87
    {  85,  0, 1,0x04 }, // 86: GM86:Lead 7 fifths; HMIGM86; Saw_wave; am086.in; rickM93
    {  86,  0, 1,0x04 }, // 87: GM87:Lead 8 brass; HMIGM87; am087.in
    {  87,  0, 1,0x04 }, // 88: GM88:Pad 1 new age; HMIGM88; am088.in
    {  88,  0, 1,0x04 }, // 89: GM89:Pad 2 warm; HMIGM89; am089.in
    {  89,  0, 1,0x04 }, // 90: GM90:Pad 3 polysynth; HMIGM90; am090.in
    {  90,  0, 1,0x04 }, // 91: GM91:Pad 4 choir; HMIGM91; Spacevo.; am091.in; rickM95
    {  91,  0, 1,0x04 }, // 92: GM92:Pad 5 bowedpad; HMIGM92; am092.in
    {  92,  0, 1,0x04 }, // 93: GM93:Pad 6 metallic; HMIGM93; am093.in
    {  93,  0, 1,0x04 }, // 94: GM94:Pad 7 halo; HMIGM94; am094.in
    {  94,  0, 1,0x04 }, // 95: GM95:Pad 8 sweep; HMIGM95; am095.in
    {  95,  0, 1,0x04 }, // 96: GM96:FX 1 rain; HMIGM96; am096.in
    {  96,  0, 1,0x04 }, // 97: GM97:FX 2 soundtrack; HMIGM97; am097.in
    {  97,  0, 1,0xF4 }, // 98: GM98:FX 3 crystal; HMIGM98; am098.in
    {  98,  0, 1,0x04 }, // 99: GM99:FX 4 atmosphere; HMIGM99; am099.in
    {  99,  0, 1,0xF4 }, // 100: GM100:FX 5 brightness; HMIGM100; am100.in; am100in; hamM51
    { 100,  0, 1,0x04 }, // 101: GM101:FX 6 goblins; HMIGM101; am101.in
    { 101,  0, 1,0x3D }, // 102: Echodrp1; GM102:FX 7 echoes; HMIGM102; am102.in; rickM98
    { 102,  0, 1,0x04 }, // 103: GM103:FX 8 sci-fi; HMIGM103; am103.in
    { 103,  0, 1,0x04 }, // 104: GM104:Sitar; HMIGM104; am104.in
    { 104,  0, 1,0x04 }, // 105: GM105:Banjo; HMIGM105; am105.in
    { 105,  0, 1,0x04 }, // 106: GM106:Shamisen; HMIGM106; am106.in
    { 106,  0, 1,0x04 }, // 107: GM107:Koto; HMIGM107; am107.in
    { 107,  0, 1,0x04 }, // 108: GM108:Kalimba; HMIGM108; am108.in
    { 108,  0, 1,0xF4 }, // 109: GM109:Bagpipe; HMIGM109; am109.in
    { 109,  0, 1,0xF4 }, // 110: GM110:Fiddle; HMIGM110; am110.in
    { 110,  0, 1,0x04 }, // 111: GM111:Shanai; HMIGM111; am111.in
    { 111,  0, 1,0x04 }, // 112: GM112:Tinkle Bell; HMIGM112; am112.in
    { 112,  0, 1,0x04 }, // 113: GM113:Agogo Bells; HMIGM113; am113.in
    { 113,  0, 1,0x04 }, // 114: GM114:Steel Drums; HMIGM114; am114.in
    { 114,  0, 1,0x04 }, // 115: GM115:Woodblock; HMIGM115; am115.in
    { 115,  0, 1,0x04 }, // 116: GM116:Taiko Drum; HMIGM116; am116.in
    { 116,  0, 1,0x04 }, // 117: GM117:Melodic Tom; HMIGM117; am117.in
    { 117,  0, 1,0x62 }, // 118: GM118:Synth Drum; HMIGM118; am118.in
    { 118,  0, 1,0xF4 }, // 119: GM119:Reverse Cymbal; HMIGM119; am119.in
    { 119,  0, 1,0x04 }, // 120: Fretnos.; GM120:Guitar FretNoise; HMIGM120; am120.in; rickM101
    { 120,  0, 1,0x04 }, // 121: GM121:Breath Noise; HMIGM121; am121.in
    { 121,  0, 1,0x62 }, // 122: GM122:Seashore; HMIGM122; am122.in
    { 122,  0, 1,0x0C }, // 123: GM123:Bird Tweet; HMIGM123; am123.in
    { 123,  0, 1,0x04 }, // 124: GM124:Telephone; HMIGM124; am124.in
    { 124,  0, 1,0x62 }, // 125: GM125:Helicopter; HMIGM125; am125.in
    { 125,  0, 1,0x04 }, // 126: GM126:Applause/Noise; HMIGM126; am126.in
    { 126,  0, 1,0x04 }, // 127: GM127:Gunshot; HMIGM127; am127.in
    { 127, 35, 1,0x04 }, // 128: GP35:Ac Bass Drum; GP36:Bass Drum 1
    { 128, 52, 1,0x04 }, // 129: GP37:Side Stick
    { 129, 48, 1,0x04 }, // 130: GP38:Acoustic Snare
    { 130, 58, 1,0x04 }, // 131: GP39:Hand Clap
    { 129, 60, 1,0x04 }, // 132: GP40:Electric Snare
    { 131, 47, 1,0x04 }, // 133: GP41:Low Floor Tom; aps041i; hamP1
    { 132, 43, 1,0x04 }, // 134: GP42:Closed High Hat
    { 131, 49, 1,0x04 }, // 135: GP43:High Floor Tom; aps041i; hamP2
    { 133, 43, 1,0x04 }, // 136: GP44:Pedal High Hat
    { 131, 51, 1,0x04 }, // 137: GP45:Low Tom; aps041i; hamP3
    { 134, 43, 1,0x04 }, // 138: GP46:Open High Hat
    { 131, 54, 1,0x04 }, // 139: GP47:Low-Mid Tom; aps041i; hamP4
    { 131, 57, 1,0x04 }, // 140: GP48:High-Mid Tom; aps041i; hamP5
    { 135, 72, 1,0x62 }, // 141: GP49:Crash Cymbal 1
    { 131, 60, 1,0x04 }, // 142: GP50:High Tom; aps041i; hamP6
    { 136, 76, 1,0x04 }, // 143: GP51:Ride Cymbal 1
    { 137, 84, 1,0x04 }, // 144: GP52:Chinese Cymbal; aps052i; hamP19
    { 138, 36, 1,0x04 }, // 145: GP53:Ride Bell
    { 139, 65, 1,0x04 }, // 146: GP54:Tambourine
    { 140, 84, 1,0x04 }, // 147: GP55:Splash Cymbal
    { 141, 83, 1,0x04 }, // 148: GP56:Cow Bell
    { 135, 84, 1,0x04 }, // 149: GP57:Crash Cymbal 2
    { 142, 24, 1,0x04 }, // 150: GP58:Vibraslap
    { 136, 77, 1,0x04 }, // 151: GP59:Ride Cymbal 2
    { 143, 60, 1,0x04 }, // 152: GP60:High Bongo
    { 144, 65, 1,0x04 }, // 153: GP61:Low Bongo
    { 145, 59, 1,0x04 }, // 154: GP62:Mute High Conga
    { 146, 51, 1,0x04 }, // 155: GP63:Open High Conga
    { 147, 45, 1,0x04 }, // 156: GP64:Low Conga
    { 148, 71, 1,0x62 }, // 157: GP65:High Timbale; hamP8; rickP98; timbale; timbale.
    { 149, 60, 1,0x04 }, // 158: GP66:Low Timbale
    { 150, 58, 1,0x04 }, // 159: GP67:High Agogo
    { 151, 53, 1,0x04 }, // 160: GP68:Low Agogo
    { 152, 64, 1,0x04 }, // 161: GP69:Cabasa
    { 153, 71, 1,0x04 }, // 162: GP70:Maracas
    { 154, 61, 1,0x04 }, // 163: GP71:Short Whistle
    { 155, 61, 1,0x04 }, // 164: GP72:Long Whistle
    { 156, 44, 1,0x04 }, // 165: GP73:Short Guiro; guiros.i; rickP96
    { 157, 40, 1,0x04 }, // 166: GP74:Long Guiro
    { 158, 69, 1,0x04 }, // 167: GP75:Claves
    { 159, 68, 1,0x04 }, // 168: GP76:High Wood Block
    { 160, 63, 1,0x04 }, // 169: GP77:Low Wood Block
    { 161, 74, 1,0x04 }, // 170: GP78:Mute Cuica
    { 162, 60, 1,0x04 }, // 171: GP79:Open Cuica
    { 163, 80, 1,0x04 }, // 172: GP80:Mute Triangle
    { 164, 64, 1,0x04 }, // 173: GP81:Open Triangle
    { 165, 72, 1,0x04 }, // 174: GP82; aps082i; hamP7
    { 166, 73, 1,0x04 }, // 175: GP83
    { 167, 70, 1,0x04 }, // 176: GP84
    { 168, 68, 1,0x04 }, // 177: GP85
    { 169, 48, 1,0x04 }, // 178: GP86
    { 131, 53, 1,0x04 }, // 179: GP87
    { 170,  0, 1,0x62 }, // 180: HMIGM0; HMIGM29; am029.in
    { 171,  0, 1,0x04 }, // 181: HMIGM1; am001.in
    { 172,  0, 1,0x62 }, // 182: HMIGM2; am002.in
    { 173,  0, 1,0x62 }, // 183: HMIGM3; am003.in
    { 174,  0, 1,0x62 }, // 184: HMIGM4; am004.in
    { 175,  0, 1,0x62 }, // 185: HMIGM5; am005.in
    { 176,  0, 1,0x3D }, // 186: HMIGM6; am006.in
    { 177,  0, 1,0x62 }, // 187: HMIGM7; am007.in
    { 178,  0, 1,0x62 }, // 188: HMIGM8; am008.in
    { 179,  0, 1,0x62 }, // 189: HMIGM9; am009.in
    { 180,  0, 1,0x04 }, // 190: HMIGM10; am010.in
    { 181,  0, 1,0x04 }, // 191: HMIGM11; am011.in
    { 182,  0, 1,0x04 }, // 192: HMIGM12; am012.in
    { 183,  0, 1,0x04 }, // 193: HMIGM13; am013.in
    { 184,  0, 1,0x00 }, // 194: HMIGM14; am014.in
    { 185,  0, 1,0x04 }, // 195: HMIGM15; am015.in
    { 186,  0, 1,0x04 }, // 196: HMIGM27; am027.in
    { 187,  0, 1,0x3D }, // 197: HMIGM37; am037.in
    { 188,  0, 1,0x04 }, // 198: HMIGM62; am062.in
    { 189, 60, 1,0x3E }, // 199: Blank.in; HMIGP0; HMIGP1; HMIGP10; HMIGP100; HMIGP101; HMIGP102; HMIGP103; HMIGP104; HMIGP105; HMIGP106; HMIGP107; HMIGP108; HMIGP109; HMIGP11; HMIGP110; HMIGP111; HMIGP112; HMIGP113; HMIGP114; HMIGP115; HMIGP116; HMIGP117; HMIGP118; HMIGP119; HMIGP12; HMIGP120; HMIGP121; HMIGP122; HMIGP123; HMIGP124; HMIGP125; HMIGP126; HMIGP127; HMIGP13; HMIGP14; HMIGP15; HMIGP16; HMIGP17; HMIGP18; HMIGP19; HMIGP2; HMIGP20; HMIGP21; HMIGP22; HMIGP23; HMIGP24; HMIGP25; HMIGP26; HMIGP3; HMIGP4; HMIGP5; HMIGP6; HMIGP7; HMIGP8; HMIGP88; HMIGP89; HMIGP9; HMIGP90; HMIGP91; HMIGP92; HMIGP93; HMIGP94; HMIGP95; HMIGP96; HMIGP97; HMIGP98; HMIGP99
    { 190, 73, 1,0x06 }, // 200: HMIGP27; Wierd1.i
    { 190, 74, 1,0x06 }, // 201: HMIGP28; Wierd1.i
    { 190, 80, 1,0x06 }, // 202: HMIGP29; Wierd1.i
    { 190, 84, 1,0x06 }, // 203: HMIGP30; Wierd1.i
    { 190, 92, 1,0x06 }, // 204: HMIGP31; Wierd1.i
    { 191, 81, 1,0x3E }, // 205: HMIGP32; Wierd2.i
    { 191, 83, 1,0x3E }, // 206: HMIGP33; Wierd2.i
    { 191, 95, 1,0x3E }, // 207: HMIGP34; Wierd2.i
    { 192, 83, 1,0x3E }, // 208: HMIGP35; Wierd3.i
    { 193, 35, 1,0x06 }, // 209: HMIGP36; Kick.ins
    { 194, 36, 1,0x06 }, // 210: HMIGP37; HMIGP86; RimShot.; rimshot.
    { 195, 60, 1,0x06 }, // 211: HMIGP38; Snare.in
    { 196, 59, 1,0x06 }, // 212: Clap.ins; HMIGP39
    { 195, 44, 1,0x06 }, // 213: HMIGP40; Snare.in
    { 197, 41, 0,0x06 }, // 214: HMIGP41; Toms.ins
    { 198, 97, 1,0x3E }, // 215: HMIGP42; HMIGP44; clshat97
    { 197, 44, 0,0x06 }, // 216: HMIGP43; Toms.ins
    { 197, 48, 0,0x06 }, // 217: HMIGP45; Toms.ins
    { 199, 96, 1,0x06 }, // 218: HMIGP46; Opnhat96
    { 197, 51, 0,0x06 }, // 219: HMIGP47; Toms.ins
    { 197, 54, 0,0x06 }, // 220: HMIGP48; Toms.ins
    { 200, 40, 1,0x06 }, // 221: Crashcym; HMIGP49; HMIGP52; HMIGP55; HMIGP57
    { 197, 57, 0,0x06 }, // 222: HMIGP50; Toms.ins
    { 201, 58, 1,0x06 }, // 223: HMIGP51; HMIGP53; Ridecym.
    { 202, 97, 1,0x06 }, // 224: HMIGP54; Tamb.ins
    { 203, 50, 1,0x06 }, // 225: Cowbell.; HMIGP56
    { 204, 28, 1,0x06 }, // 226: HMIGP58; vibrasla
    { 201, 60, 1,0x06 }, // 227: HMIGP59; ridecym.
    { 205, 53, 1,0x06 }, // 228: HMIGP60; mutecong
    { 206, 46, 1,0x06 }, // 229: HMIGP61; conga.in
    { 205, 57, 1,0x06 }, // 230: HMIGP62; mutecong
    { 207, 42, 1,0x06 }, // 231: HMIGP63; loconga.
    { 207, 37, 1,0x06 }, // 232: HMIGP64; loconga.
    { 208, 41, 1,0x06 }, // 233: HMIGP65; timbale.
    { 208, 37, 1,0x06 }, // 234: HMIGP66; timbale.
    { 209, 77, 1,0x06 }, // 235: HMIGP67; agogo.in
    { 209, 72, 1,0x06 }, // 236: HMIGP68; agogo.in
    { 210, 70, 1,0x06 }, // 237: HMIGP69; HMIGP82; shaker.i
    { 210, 90, 1,0x06 }, // 238: HMIGP70; shaker.i
    { 211, 39, 1,0x3E }, // 239: HMIGP71; hiwhist.
    { 212, 36, 1,0x3E }, // 240: HMIGP72; lowhist.
    { 213, 46, 1,0x06 }, // 241: HMIGP73; higuiro.
    { 214, 48, 1,0x06 }, // 242: HMIGP74; loguiro.
    { 215, 85, 1,0x4E }, // 243: HMIGP75; clavecb.
    { 216, 66, 1,0x06 }, // 244: HMIGP76; woodblok
    { 216, 61, 1,0x06 }, // 245: HMIGP77; woodblok
    { 217, 41, 1,0x3E }, // 246: HMIGP78; hicuica.
    { 218, 41, 1,0x3E }, // 247: HMIGP79; locuica.
    { 219, 81, 1,0x4E }, // 248: HMIGP80; mutringl
    { 220, 81, 1,0x26 }, // 249: HMIGP81; triangle
    { 220, 76, 1,0x26 }, // 250: HMIGP83; triangle
    { 220,103, 1,0x26 }, // 251: HMIGP84; triangle
    { 194, 60, 1,0x06 }, // 252: HMIGP85; rimShot.
    { 221, 53, 1,0x06 }, // 253: HMIGP87; taiko.in
    { 222,  0, 0,0x00 }, // 254: IntP0; IntP1; IntP10; IntP100; IntP101; IntP102; IntP103; IntP104; IntP105; IntP106; IntP107; IntP108; IntP109; IntP11; IntP110; IntP111; IntP112; IntP113; IntP114; IntP115; IntP116; IntP117; IntP118; IntP119; IntP12; IntP120; IntP121; IntP122; IntP123; IntP124; IntP125; IntP126; IntP127; IntP13; IntP14; IntP15; IntP16; IntP17; IntP18; IntP19; IntP2; IntP20; IntP21; IntP22; IntP23; IntP24; IntP25; IntP26; IntP3; IntP4; IntP5; IntP6; IntP7; IntP8; IntP9; IntP94; IntP95; IntP96; IntP97; IntP98; IntP99; hamM0; hamM100; hamM101; hamM102; hamM103; hamM104; hamM105; hamM106; hamM107; hamM108; hamM109; hamM110; hamM111; hamM112; hamM113; hamM114; hamM115; hamM116; hamM117; hamM118; hamM119; hamM126; hamM127; hamM49; hamM74; hamM75; hamM76; hamM77; hamM78; hamM79; hamM80; hamM81; hamM82; hamM83; hamM84; hamM85; hamM86; hamM87; hamM88; hamM89; hamM90; hamM91; hamM92; hamM93; hamM94; hamM95; hamM96; hamM97; hamM98; hamM99; hamP100; hamP101; hamP102; hamP103; hamP104; hamP105; hamP106; hamP107; hamP108; hamP109; hamP110; hamP111; hamP112; hamP113; hamP114; hamP115; hamP116; hamP117; hamP118; hamP119; hamP120; hamP121; hamP122; hamP123; hamP124; hamP125; hamP126; hamP127; hamP20; hamP21; hamP22; hamP23; hamP24; hamP25; hamP26; hamP93; hamP94; hamP95; hamP96; hamP97; hamP98; hamP99; intM0; intM100; intM101; intM102; intM103; intM104; intM105; intM106; intM107; intM108; intM109; intM110; intM111; intM112; intM113; intM114; intM115; intM116; intM117; intM118; intM119; intM120; intM121; intM122; intM123; intM124; intM125; intM126; intM127; intM50; intM51; intM52; intM53; intM54; intM55; intM56; intM57; intM58; intM59; intM60; intM61; intM62; intM63; intM64; intM65; intM66; intM67; intM68; intM69; intM70; intM71; intM72; intM73; intM74; intM75; intM76; intM77; intM78; intM79; intM80; intM81; intM82; intM83; intM84; intM85; intM86; intM87; intM88; intM89; intM90; intM91; intM92; intM93; intM94; intM95; intM96; intM97; intM98; intM99; nosound; nosound.; rickM0; rickM102; rickM103; rickM104; rickM105; rickM106; rickM107; rickM108; rickM109; rickM110; rickM111; rickM112; rickM113; rickM114; rickM115; rickM116; rickM117; rickM118; rickM119; rickM120; rickM121; rickM122; rickM123; rickM124; rickM125; rickM126; rickM127; rickM49; rickM50; rickM51; rickM52; rickM53; rickM54; rickM55; rickM56; rickM57; rickM58; rickM59; rickM60; rickM61; rickM62; rickM63; rickM64; rickM65; rickM66; rickM67; rickM68; rickM69; rickM70; rickM71; rickM72; rickM73; rickM74; rickM75; rickP0; rickP1; rickP10; rickP106; rickP107; rickP108; rickP109; rickP11; rickP110; rickP111; rickP112; rickP113; rickP114; rickP115; rickP116; rickP117; rickP118; rickP119; rickP12; rickP120; rickP121; rickP122; rickP123; rickP124; rickP125; rickP126; rickP127; rickP2; rickP3; rickP4; rickP5; rickP6; rickP7; rickP8; rickP9
    { 223,  0, 0,0x00 }, // 255: DBlock.i; DBlocki; hamM1; intM1; rickM1
    { 224,  0, 0,0x00 }, // 256: GClean.i; GCleani; hamM2; intM2; rickM2
    {  28,  0, 0,0x00 }, // 257: BPerc.in; BPercin; hamM3; intM3; rickM3
    { 225,  0, 0,0x00 }, // 258: DToms.in; DTomsin; hamM4; intM4; rickM4
    {  31,  0, 0,0x00 }, // 259: GFeedbck; hamM5; intM5; rickM5
    {  30,  0, 0,0x00 }, // 260: GDist.in; GDistin; hamM6; intM6; rickM6
    { 226,  0, 0,0x00 }, // 261: GOverD.i; GOverDi; hamM7; intM7; rickM7
    { 227,  0, 0,0x00 }, // 262: GMetal.i; GMetali; hamM8; intM8; rickM8
    { 228,  0, 0,0x00 }, // 263: BPick.in; BPickin; hamM9; intM9; rickM9
    { 229,  0, 0,0x00 }, // 264: BSlap.in; BSlapin; hamM10; intM10; rickM10
    { 230,  0, 0,0x00 }, // 265: BSynth1; BSynth1.; hamM11; intM11; rickM11
    { 231,  0, 0,0x00 }, // 266: BSynth2; BSynth2.; hamM12; intM12; rickM12
    {  38,  0, 0,0x00 }, // 267: BSynth3; BSynth3.; hamM13; intM13; rickM13
    {  46,  0, 0,0x00 }, // 268: BSynth4; BSynth4.; hamM14; intM14; rickM14
    { 232,  0, 0,0x00 }, // 269: PSoft.in; PSoftin; hamM15; intM15; rickM15
    {  79,  0, 0,0x00 }, // 270: LSquare; LSquare.; hamM16; intM16; rickM16
    {  84,  0, 0,0x00 }, // 271: PFlutes; PFlutes.; hamM17; intM17; rickM17
    { 233,  0, 0,0x00 }, // 272: PRonStr1; hamM18; intM18; rickM18
    { 234,  0, 0,0x00 }, // 273: PRonStr2; hamM19; intM19; rickM19
    {  49,  0, 0,0x00 }, // 274: PMellow; PMellow.; hamM20; intM20; rickM20
    {  89,  0, 0,0x00 }, // 275: LTriang; LTriang.; hamM21; intM21; rickM21
    {  92,  0, 0,0x00 }, // 276: PSlow.in; PSlowin; hamM22; intM22; rickM22
    {  93,  0, 0,0x00 }, // 277: PSweep.i; PSweepi; hamM23; intM23; rickM23
    { 105,  0, 0,0x00 }, // 278: LDist.in; LDistin; hamM24; intM24; rickM24
    { 235,  0, 0,0x00 }, // 279: LTrap.in; LTrapin; hamM25; intM25; rickM25
    { 236,  0, 0,0x00 }, // 280: LSaw.ins; LSawins; hamM26; intM26; rickM26
    { 237,  0, 0,0x00 }, // 281: PolySyn; PolySyn.; hamM27; intM27; rickM27
    { 238,  0, 0,0x00 }, // 282: Pobo.ins; Poboins; hamM28; intM28; rickM28
    { 239,  0, 0,0x00 }, // 283: PSweep2; PSweep2.; hamM29; intM29; rickM29
    { 240,  0, 0,0x00 }, // 284: LBright; LBright.; hamM30; intM30; rickM30
    { 241,  0, 0,0x00 }, // 285: SynStrin; hamM31; intM31; rickM31
    { 242,  0, 0,0x00 }, // 286: SynStr2; SynStr2.; hamM32; intM32; rickM32
    { 243,  0, 0,0x00 }, // 287: hamM33; intM33; low_blub; rickM33
    { 244,  0, 0,0x00 }, // 288: DInsect; DInsect.; hamM34; intM34; rickM34
    { 245,  0, 0,0x00 }, // 289: hamM35; hardshak; intM35; rickM35
    { 119,  0, 0,0x00 }, // 290: DNoise1; DNoise1.; hamM36; intM36; rickM36
    { 246,  0, 0,0x00 }, // 291: WUMP.ins; WUMPins; hamM37; intM37; rickM37
    { 247,  0, 0,0x00 }, // 292: DSnare.i; DSnarei; hamM38; intM38; rickM38
    { 248,  0, 0,0x00 }, // 293: DTimp.in; DTimpin; hamM39; intM39; rickM39
    { 249,  0, 0,0x00 }, // 294: DRevCym; DRevCym.; hamM40; intM40; rickM40
    { 250,  0, 0,0x00 }, // 295: Dorky.in; Dorkyin; hamM41; intM41; rickM41
    { 251,  0, 0,0x00 }, // 296: DFlab.in; DFlabin; hamM42; intM42; rickM42
    { 252,  0, 0,0x00 }, // 297: DInsect2; hamM43; intM43; rickM43
    { 253,  0, 0,0x00 }, // 298: DChopper; hamM44; intM44; rickM44
    { 254,  0, 0,0x00 }, // 299: DShot.in; DShotin; hamM45; intM45; rickM45
    { 255,  0, 0,0x00 }, // 300: KickAss; KickAss.; hamM46; intM46; rickM46
    { 256,  0, 0,0x00 }, // 301: RVisCool; hamM47; intM47; rickM47
    { 257,  0, 0,0x00 }, // 302: DSpring; DSpring.; hamM48; intM48; rickM48
    { 258,  0, 0,0x00 }, // 303: Chorar22; intM49
    { 259, 36, 0,0x00 }, // 304: IntP27; hamP27; rickP27; timpani; timpani.
    { 260, 50, 0,0x00 }, // 305: IntP28; hamP28; rickP28; timpanib
    { 261, 37, 0,0x00 }, // 306: APS043.i; APS043i; IntP29; rickP29
    { 262, 39, 0,0x00 }, // 307: IntP30; mgun3.in; mgun3in; rickP30
    { 263, 39, 0,0x00 }, // 308: IntP31; hamP31; kick4r.i; kick4ri; rickP31
    { 264, 86, 0,0x00 }, // 309: IntP32; hamP32; rickP32; timb1r.i; timb1ri
    { 265, 43, 0,0x00 }, // 310: IntP33; hamP33; rickP33; timb2r.i; timb2ri
    { 127, 24, 0,0x00 }, // 311: IntP34; apo035.i; apo035i; rickP34
    { 127, 29, 0,0x00 }, // 312: IntP35; apo035.i; apo035i; rickP35
    { 266, 50, 0,0x00 }, // 313: IntP36; hamP36; hartbeat; rickP36
    { 267, 30, 0,0x00 }, // 314: IntP37; hamP37; rickP37; tom1r.in; tom1rin
    { 267, 33, 0,0x00 }, // 315: IntP38; hamP38; rickP38; tom1r.in; tom1rin
    { 267, 38, 0,0x00 }, // 316: IntP39; hamP39; rickP39; tom1r.in; tom1rin
    { 267, 42, 0,0x00 }, // 317: IntP40; hamP40; rickP40; tom1r.in; tom1rin
    { 268, 24, 0,0x00 }, // 318: IntP41; rickP41; tom2.ins; tom2ins
    { 268, 27, 0,0x00 }, // 319: IntP42; rickP42; tom2.ins; tom2ins
    { 268, 29, 0,0x00 }, // 320: IntP43; rickP43; tom2.ins; tom2ins
    { 268, 32, 0,0x00 }, // 321: IntP44; rickP44; tom2.ins; tom2ins
    { 269, 32, 0,0x00 }, // 322: IntP45; rickP45; tom.ins; tomins
    { 270, 53, 0,0x00 }, // 323: IntP46; conga.in; congain; hamP46; rickP46
    { 270, 57, 0,0x00 }, // 324: IntP47; conga.in; congain; hamP47; rickP47
    { 271, 60, 0,0x00 }, // 325: IntP48; hamP48; rickP48; snare01r
    { 272, 55, 0,0x00 }, // 326: IntP49; hamP49; rickP49; slap.ins; slapins
    { 254, 85, 0,0x00 }, // 327: IntP50; hamP50; rickP50; shot.ins; shotins
    { 273, 90, 0,0x00 }, // 328: IntP51; rickP51; snrsust; snrsust.
    { 274, 84, 0,0x00 }, // 329: IntP52; rickP52; snare.in; snarein
    { 275, 48, 0,0x00 }, // 330: IntP53; hamP53; rickP53; synsnar; synsnar.
    { 276, 48, 0,0x00 }, // 331: IntP54; rickP54; synsnr1; synsnr1.
    { 132, 72, 0,0x00 }, // 332: IntP55; aps042.i; aps042i; rickP55
    { 277, 72, 0,0x00 }, // 333: IntP56; hamP56; rickP56; rimshotb
    { 278, 72, 0,0x00 }, // 334: IntP57; rickP57; rimshot; rimshot.
    { 279, 63, 0,0x00 }, // 335: IntP58; crash.in; crashin; hamP58; rickP58
    { 279, 65, 0,0x00 }, // 336: IntP59; crash.in; crashin; hamP59; rickP59
    { 280, 79, 0,0x00 }, // 337: IntP60; cymbal.i; cymbali; rickP60
    { 281, 38, 0,0x00 }, // 338: IntP61; cymbals; cymbals.; hamP61; rickP61
    { 282, 94, 0,0x00 }, // 339: IntP62; hamP62; hammer5r; rickP62
    { 283, 87, 0,0x00 }, // 340: IntP63; hamP63; hammer3; hammer3.; rickP63
    { 283, 94, 0,0x00 }, // 341: IntP64; hamP64; hammer3; hammer3.; rickP64
    { 284, 80, 0,0x00 }, // 342: IntP65; hamP65; rickP65; ride2.in; ride2in
    { 285, 47, 0,0x00 }, // 343: IntP66; hamP66; hammer1; hammer1.; rickP66
    { 286, 61, 0,0x00 }, // 344: IntP67; rickP67; tambour; tambour.
    { 286, 68, 0,0x00 }, // 345: IntP68; rickP68; tambour; tambour.
    { 287, 61, 0,0x00 }, // 346: IntP69; hamP69; rickP69; tambou2; tambou2.
    { 287, 68, 0,0x00 }, // 347: IntP70; hamP70; rickP70; tambou2; tambou2.
    { 268, 60, 0,0x00 }, // 348: IntP71; hamP71; rickP71; woodbloc
    { 288, 60, 0,0x00 }, // 349: IntP72; hamP72; rickP72; woodblok
    { 289, 36, 0,0x00 }, // 350: IntP73; claves.i; clavesi; rickP73
    { 289, 60, 0,0x00 }, // 351: IntP74; claves.i; clavesi; rickP74
    { 290, 60, 0,0x00 }, // 352: IntP75; claves2; claves2.; hamP75; rickP75
    { 291, 60, 0,0x00 }, // 353: IntP76; claves3; claves3.; hamP76; rickP76
    { 292, 68, 0,0x00 }, // 354: IntP77; clave.in; clavein; hamP77; rickP77
    { 293, 71, 0,0x00 }, // 355: IntP78; agogob4; agogob4.; hamP78; rickP78
    { 293, 72, 0,0x00 }, // 356: IntP79; agogob4; agogob4.; hamP79; rickP79
    { 294,101, 0,0x00 }, // 357: IntP80; clarion; clarion.; hamP80; rickP80
    { 295, 36, 0,0x00 }, // 358: IntP81; rickP81; trainbel
    { 296, 25, 0,0x00 }, // 359: IntP82; gong.ins; gongins; hamP82; rickP82
    { 297, 37, 0,0x00 }, // 360: IntP83; hamP83; kalimbai; rickP83
    { 298, 36, 0,0x00 }, // 361: IntP84; rickP84; xylo1.in; xylo1in
    { 298, 41, 0,0x00 }, // 362: IntP85; rickP85; xylo1.in; xylo1in
    { 299, 84, 0,0x00 }, // 363: IntP86; hamP86; match.in; matchin; rickP86
    { 300, 54, 0,0x00 }, // 364: IntP87; breathi; breathi.; hamP87; rickP87
    { 301, 36, 0,0x00 }, // 365: IntP88; rickP88; scratch; scratch.
    { 302, 60, 0,0x00 }, // 366: IntP89; crowd.in; crowdin; hamP89; rickP89
    { 303, 37, 0,0x00 }, // 367: IntP90; rickP90; taiko.in; taikoin
    { 304, 36, 0,0x00 }, // 368: IntP91; rickP91; rlog.ins; rlogins
    { 305, 32, 0,0x00 }, // 369: IntP92; hamP92; knock.in; knockin; rickP92
    { 249, 48, 0,0x00 }, // 370: IntP93; drevcym
    { 112,  0, 1,0x62 }, // 371: agogoin; hamM50
    { 306,  0, 1,0x62 }, // 372: Fantasy1; fantasy1; hamM52; rickM94
    { 307,  0, 1,0x62 }, // 373: guitar1; hamM53
    {  93,  0, 1,0x62 }, // 374: Halopad.; halopad; hamM54; rickM96
    { 308,  0, 1,0x62 }, // 375: hamM55; hamatmos
    { 309,  0, 1,0x62 }, // 376: hamM56; hamcalio
    { 107,  0, 1,0x62 }, // 377: hamM57; kalimba
    { 116,  0, 1,0x62 }, // 378: hamM58; melotom
    { 310,  0, 1,0x62 }, // 379: hamM59; moonins
    {  28,  0, 1,0x62 }, // 380: hamM60; muteguit
    {  78,  0, 0,0x62 }, // 381: hamM61; ocarina
    { 311,  0, 1,0x62 }, // 382: Polyham3; hamM62
    { 312,  0, 1,0x62 }, // 383: Polyham; hamM63
    { 313,  0, 1,0x62 }, // 384: hamM64; sitar2i
    {  79,  0, 1,0x62 }, // 385: hamM65; squarewv
    {  94,  0, 1,0x62 }, // 386: Sweepad.; hamM66; rickM97; sweepad
    {  38,  0, 1,0x62 }, // 387: hamM67; synbass1
    {  33,  0, 1,0x62 }, // 388: hamM68; synbass2
    { 115,  0, 1,0x62 }, // 389: Taikoin; hamM69
    { 314,  0, 1,0x62 }, // 390: hamM70; weird1a
    { 315,  0, 1,0x62 }, // 391: Polyham4; hamM71
    { 316,  0, 1,0x62 }, // 392: hamM72; hamsynbs
    { 317,  0, 0,0x62 }, // 393: Ocasynth; hamM73
    { 318,  0, 1,0x62 }, // 394: hamM120; hambass1
    { 319,  0, 1,0x62 }, // 395: hamM121; hamguit1
    { 320,  0, 1,0x62 }, // 396: hamM122; hamharm2
    { 321,  0, 1,0x62 }, // 397: hamM123; hamvox1
    { 322,  0, 1,0x62 }, // 398: hamM124; hamgob1
    { 323,  0, 1,0x62 }, // 399: hamM125; hamblow1
    { 135, 49, 1,0x62 }, // 400: crash1i; hamP0
    { 324, 72, 1,0x62 }, // 401: cowbell; hamP9
    { 325, 74, 1,0x62 }, // 402: conghi.i; conghii; hamP10; rickP100
    { 127, 35, 1,0x3D }, // 403: aps035i; hamP11; kick2.in; rickP14
    { 326, 35, 1,0x62 }, // 404: hamP12; hamkick; kick3.in; rickP15
    { 327, 41, 1,0x62 }, // 405: hamP13; rimshot2
    { 328, 38, 1,0x62 }, // 406: hamP14; hamsnr1; rickP16; snr1.ins
    { 329, 39, 1,0x62 }, // 407: hamP15; handclap
    { 330, 49, 1,0x62 }, // 408: hamP16; smallsnr
    { 331, 83, 1,0x62 }, // 409: clsdhhat; hamP17; rickP95
    { 332, 59, 1,0x62 }, // 410: hamP18; openhht2
    { 261, 37, 1,0x62 }, // 411: APS043i; hamP29
    { 262, 39, 1,0x62 }, // 412: hamP30; mgun3in
    { 127, 24, 1,0x3D }, // 413: apo035i; hamP34
    { 127, 29, 1,0x3D }, // 414: apo035i; hamP35
    { 333, 24, 0,0x00 }, // 415: hamP41; tom2ins
    { 333, 27, 0,0x00 }, // 416: hamP42; tom2ins
    { 333, 29, 0,0x00 }, // 417: hamP43; tom2ins
    { 333, 32, 0,0x00 }, // 418: hamP44; tom2ins
    { 269, 32, 1,0x04 }, // 419: hamP45; tomins
    { 273, 90, 1,0x1A }, // 420: hamP51; snrsust
    { 334, 84, 1,0x07 }, // 421: hamP52; snarein
    { 276, 48, 1,0x39 }, // 422: hamP54; synsnr1
    { 132, 72, 1,0x62 }, // 423: aps042i; hamP55
    { 278, 72, 1,0x62 }, // 424: hamP57; rimshot
    { 281, 79, 1,0x07 }, // 425: cymbali; hamP60
    { 335, 61, 1,0x62 }, // 426: hamP67; tambour
    { 335, 68, 1,0x62 }, // 427: hamP68; tambour
    { 336, 36, 1,0x62 }, // 428: clavesi; hamP73
    { 336, 60, 1,0x62 }, // 429: clavesi; hamP74
    { 295, 36, 1,0x00 }, // 430: hamP81; trainbel
    { 298, 36, 1,0x06 }, // 431: hamP84; xylo1in
    { 298, 41, 1,0x06 }, // 432: hamP85; xylo1in
    { 337, 36, 1,0x62 }, // 433: hamP88; scratch
    { 115, 37, 1,0x62 }, // 434: hamP90; taikoin
    { 304, 36, 1,0x07 }, // 435: hamP91; rlogins
    { 338,  0, 1,0x62 }, // 436: Bass.ins; rickM76
    { 339,  0, 1,0x62 }, // 437: Basnor04; rickM77
    { 340,  0, 0,0x00 }, // 438: Synbass1; rickM78
    { 341,  0, 1,0x62 }, // 439: Synbass2; rickM79
    { 342,  0, 1,0x62 }, // 440: Pickbass; rickM80
    {  34,  0, 1,0x3D }, // 441: Slapbass; rickM81
    { 343,  0, 1,0x62 }, // 442: Harpsi1.; rickM82
    { 344,  0, 1,0x16 }, // 443: Guit_el3; rickM83
    { 226,  0, 1,0x62 }, // 444: Guit_fz2; rickM84
    { 345,  0, 1,0x16 }, // 445: Orchit2.; rickM88
    { 346,  0, 1,0x62 }, // 446: Brass11.; rickM89
    { 347,  0, 1,0x0C }, // 447: Brass2.i; rickM90
    { 348,  0, 1,0x62 }, // 448: Brass3.i; rickM91
    { 349,  0, 0,0x00 }, // 449: Squ_wave; rickM92
    { 350,  0, 0,0x00 }, // 450: Agogo.in; rickM99
    { 114,  0, 1,0x62 }, // 451: Woodblk.; rickM100
    { 351, 35, 1,0x62 }, // 452: kick1.in; rickP13
    { 352, 38, 1,0x3D }, // 453: rickP17; snare1.i
    { 353, 38, 1,0x3D }, // 454: rickP18; snare2.i
    { 352, 38, 1,0x0C }, // 455: rickP19; snare4.i
    { 353, 38, 1,0x62 }, // 456: rickP20; snare5.i
    { 354, 31, 1,0x0C }, // 457: rickP21; rocktom.
    { 354, 35, 1,0x0C }, // 458: rickP22; rocktom.
    { 354, 38, 1,0x0C }, // 459: rickP23; rocktom.
    { 354, 41, 1,0x0C }, // 460: rickP24; rocktom.
    { 354, 45, 1,0x0C }, // 461: rickP25; rocktom.
    { 354, 50, 1,0x0C }, // 462: rickP26; rocktom.
    { 355, 50, 0,0x00 }, // 463: openhht1; rickP93
    { 332, 50, 1,0x62 }, // 464: openhht2; rickP94
    { 356, 72, 1,0x62 }, // 465: guirol.i; rickP97
    { 148, 59, 1,0x62 }, // 466: rickP99; timbale.
    { 357, 64, 1,0x62 }, // 467: congas2.; rickP101
    { 357, 60, 1,0x62 }, // 468: congas2.; rickP102
    { 358, 72, 1,0x62 }, // 469: bongos.i; rickP103
    { 358, 62, 1,0x62 }, // 470: bongos.i; rickP104
    { 131, 53, 1,0x37 }, // 471: rickP105; surdu.in
};
static const unsigned short banks[][256] =
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
