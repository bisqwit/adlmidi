#pragma once

#ifndef ADLUI_H
#define ADLUI_H

#include <stdarg.h>
#include <cstdio>

#ifndef ADLCPP_H
#include "adlcpp.h"
#endif

#ifndef ADLGLOBAL_H
#include "adlglob.h"
#endif

class ADLMIDI_DECLSPEC ADL_UserInterface
{
public:
    static constexpr unsigned NColumns = 1216/20;
  #ifdef SUPPORT_VIDEO_OUTPUT
    static constexpr unsigned VidWidth  = 1216, VidHeight = 2160;
    static constexpr unsigned FontWidth =   20, FontHeight = 45;
    static constexpr unsigned TxWidth   = (VidWidth/FontWidth), TxHeight = (VidHeight/FontHeight);
    unsigned int   PixelBuffer[VidWidth*VidHeight] = {0};
    unsigned short CharBuffer[TxWidth*TxHeight] = {0};
    bool           DirtyCells[TxWidth*TxHeight] = {false};
    unsigned       NDirty = 0;
  #endif
  #ifdef __WIN32__
    void* handle;
  #endif
    int x, y, color, txtline, maxy;

    // Text:
    char background[NColumns][1 + 23*MaxCards];
    unsigned char backgroundcolor[NColumns][1 + 23*MaxCards];
    bool          touched[1 + 23*MaxCards]{false};
    // Notes:
    char slots[NColumns][1 + 23*MaxCards];
    unsigned char slotcolors[NColumns][1 + 23*MaxCards];

    bool cursor_visible;
    char stderr_buffer[256];
public:
    ADL_UserInterface();
    ~ADL_UserInterface();
    void HideCursor();
    void ShowCursor();
    void VidPut(char c);

#ifdef SUPPORT_VIDEO_OUTPUT
    static unsigned VidTranslateColor(unsigned c);
    void VidRenderCh(unsigned x,unsigned y, unsigned attr, const unsigned char* bitmap);
    void VidRender();
#endif

    void PutC(char c);

#ifndef __DJGPP__
    static void RawPrn(const char* fmt, ...) __attribute__((format(printf,1,2)));
#endif

    int Print(unsigned beginx, unsigned color, bool ln, const char* fmt, va_list ap);
    int Print(unsigned beginx, unsigned color, bool ln, const char* fmt, ...) __attribute__((format(printf,5,6)));
    int PrintLn(const char* fmt, ...) __attribute__((format(printf,2,3)));
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend);

    void Draw(int notex,int notey, int color, char ch);

    void IllustrateVolumes(double left, double right);

    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy);
    // Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor);
    // Choose a permanent color for given instrument
    int AllocateColor(int ins);

    void SetTetrisInput(ADL_Input *input);
    bool DoCheckTetris();

    bool TetrisLaunched = false;
    bool CheckTetris();
};

#endif // ADLUI_H
