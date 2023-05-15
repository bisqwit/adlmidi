#include "adlui.h"
#include "adlglob.h"
#include "adlpriv.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>  // exp, log, ceil

#if !defined(__WIN32__) || defined(__CYGWIN__)
# include <termios.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <csignal>
#elif defined(__WIN32__)
# include <windows.h>
#endif

#ifdef __DJGPP__
# include <conio.h>
# include <csignal>
# include <signal.h>
# define RawPrn cprintf
#endif

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
"DDDDDDDDDDDDDDDD"  // Prc 64-79
"DD??????????????"  // Prc 80-95
"????????????????"  // Prc 96-111
"????????????????"; // Prc 112-127

static unsigned WindowLines = 0;


#ifdef SUPPORT_PUZZLE_GAME
#include "puzzlegame.inc"
#endif


#ifdef SUPPORT_VIDEO_OUTPUT
class UIfontBase {};
static unsigned UnicodeToASCIIapproximation(unsigned n) { return n; }
//#include "6x9.inc"
#include "9x15.inc"
#endif

#if (!defined(__WIN32__) || defined(__CYGWIN__)) && defined(TIOCGWINSZ)
extern "C" { static void SigWinchHandler(int); }
static void SigWinchHandler(int)
{
    struct winsize w;
    if(ioctl(2, TIOCGWINSZ, &w) >= 0 || ioctl(1, TIOCGWINSZ, &w) >= 0 || ioctl(0, TIOCGWINSZ, &w) >= 0)
        WindowLines = w.ws_row;
}
#else
static void SigWinchHandler(int) {}
#endif

static unsigned WinHeight()
{
    unsigned result =
        AdlPercussionMode
        ? std::min(2u, NumCards) * 23
        : std::min(3u, NumCards) * 18;

    if(WindowLines) result = std::min(WindowLines-1, result);
    return result;
}

static void GuessInitialWindowHeight()
{
    auto s = std::getenv("LINES");
    if(s && std::atoi(s)) WindowLines = std::atoi(s);
    SigWinchHandler(0);
}

ADLMIDI_EXPORT ADL_UserInterface::ADL_UserInterface(): x(0), y(0), color(-1), txtline(0),
    maxy(0), cursor_visible(true)
{
#ifdef SUPPORT_PUZZLE_GAME
    ADLMIDI_PuzzleGame::UI = this;
#endif
    GuessInitialWindowHeight();
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
        WindowLines = tmp.dwSize.Y;
        //COORD size = { NColumns, 23*NumCards+5 };
        //SetConsoleScreenBufferSize(handle,size);
    }
#endif

#if (!defined(__WIN32__) && !defined(__DJGPP__) || defined(__CYGWIN__)) && defined(TIOCGWINSZ)
    std::signal(SIGWINCH, SigWinchHandler);
#endif
#ifdef __DJGPP__
    color = 7;
#endif
    std::memset(slots, '.',      sizeof(slots));
    std::memset(background, '.', sizeof(background));
    std::memset(backgroundcolor, 1, sizeof(backgroundcolor));
    std::setvbuf(stderr, stderr_buffer, _IOFBF, sizeof(stderr_buffer));
    RawPrn("\r"); // Ensure cursor is at the x=0 we imagine it being
    Print(0, 7, true, "Hit Ctrl-C to quit");
}

ADL_UserInterface::~ADL_UserInterface()
{
#ifdef SUPPORT_PUZZLE_GAME
    ADLMIDI_PuzzleGame::UI = nullptr;
#endif
}

ADLMIDI_EXPORT void ADL_UserInterface::HideCursor()
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
    RawPrn("\33[?25l"); // hide cursor
}

ADLMIDI_EXPORT void ADL_UserInterface::ShowCursor()
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
    RawPrn("\33[?25h"); // show cursor
    std::fflush(stderr);
}

ADLMIDI_EXPORT void ADL_UserInterface::VidPut(char c)
{
#ifndef SUPPORT_VIDEO_OUTPUT
    c = c;
#else
    unsigned clr = color, tx = x, ty = y;
    unsigned cell_index = ty*TxWidth + tx;
    if(cell_index < TxWidth*TxHeight)
    {
        unsigned short what = (clr << 8) + (unsigned(c) & 0xFF);
        if(what != CharBuffer[cell_index])
        {
            CharBuffer[cell_index] = what;
            if(!DirtyCells[cell_index])
            {
                DirtyCells[cell_index] = true;
                ++NDirty;
            }
        }
    }
#endif
}

#ifdef SUPPORT_VIDEO_OUTPUT

ADLMIDI_EXPORT unsigned ADL_UserInterface::VidTranslateColor(unsigned c)
{
    static const unsigned colors[16] =
    {
        0x000000,0x00005F,0x00AA00,0x5F5FAF,
        0xAA0000,0xAA00AA,0x87875F,0xAAAAAA,
        0x005F87,0x5555FF,0x55FF55,0x55FFFF,
        0xFF5555,0xFF55FF,0xFFFF55,0xFFFFFF
    };
    return colors[c % 16];
}

ADLMIDI_EXPORT void ADL_UserInterface::VidRenderCh(unsigned x, unsigned y, unsigned attr, const unsigned char *bitmap)
{
    unsigned bg = VidTranslateColor(attr >> 4), fg = VidTranslateColor(attr);
    for(unsigned line=0; line<FontHeight; ++line)
    {
        unsigned int* pix = PixelBuffer + (y*FontHeight+line)*VidWidth + x*FontWidth;

        unsigned char bm = bitmap[line*15/FontHeight];
        for(unsigned w=0; w<FontWidth; ++w)
        {
            int shift0 = 7 - w    *9/FontWidth;
            int shift1 = 7 - (w-1)*9/FontWidth;
            int shift2 = 7 - (w+1)*9/FontWidth;
            bool flag = (shift0 >= 0 && (bm & (1u << shift0)));
            if(!flag && (attr & 8))
            {
                flag = (shift1 >= 0 && (bm & (1u << shift1)))
                        || (shift2 >= 0 && (bm & (1u << shift2)));
            }
            pix[w] = flag ? fg : bg;
        }
    }
}

ADLMIDI_EXPORT void ADL_UserInterface::VidRender()
{
    static const font9x15 font;
    if(NDirty)
    {
#pragma omp parallel for schedule(static)
        for(unsigned scan=0; scan<TxWidth*TxHeight; ++scan)
            if(DirtyCells[scan])
            {
                --NDirty;
                DirtyCells[scan] = false;
                VidRenderCh(scan%TxWidth, scan/TxWidth,
                            CharBuffer[scan] >> 8,
                            font.GetBitmap() + 15 * font.GetIndex(CharBuffer[scan] & 0xFF));
            }
    }
}
#endif

ADLMIDI_EXPORT void ADL_UserInterface::PutC(char c)
{
#ifdef __WIN32__
    if(handle) WriteConsole(handle,&c,1, 0,0);
    else
#endif
    {
#ifdef __DJGPP__
        putch(c);
#else
        std::fputc(c, stderr);
#endif
    }
    VidPut(c);
    ++x; // One letter drawn. Update cursor position.
}


#ifndef __DJGPP__
void ADL_UserInterface::RawPrn(const char *fmt, ...)
{
    // Note: should essentially match PutC, except without updates to x
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#endif

ADLMIDI_EXPORT int ADL_UserInterface::Print(unsigned beginx, unsigned color, bool ln, const char *fmt, va_list ap)
{
    char Line[1024];
#ifndef __CYGWIN__
    int nchars = vsnprintf(Line, sizeof(Line), fmt, ap);
#else
    int nchars = std::vsprintf(Line, fmt, ap); /* SECURITY: POSSIBLE BUFFER OVERFLOW (Cygwin) */
#endif
    //if(nchars == 0) return nchars;

    HideCursor();
    for(unsigned tx = beginx; tx < NColumns; ++tx)
    {
        int n = tx-beginx; // Index into Line[]
        if(n < nchars && Line[n] != '\n')
        {
            GotoXY(tx, txtline);
            Color( backgroundcolor[tx][txtline] = (/*Line[n] == '.' ? 1 :*/ color) );
            PutC( background[tx][txtline] = Line[n] );
        }
        else //if(background[tx][txtline]!='.' && slots[tx][txtline]=='.')
        {
            if(!ln) break;
            GotoXY(tx,txtline);
            Color( backgroundcolor[tx][txtline] = 1);
            PutC( background[tx][txtline] = '.' );
        }
    }
    std::fflush(stderr);

    if(ln)
    {
        txtline = (txtline + 1) % WinHeight();
    }

    return nchars;
}

ADLMIDI_EXPORT int ADL_UserInterface::Print(unsigned beginx, unsigned color, bool ln, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = Print(beginx, color, ln, fmt, ap);
    va_end(ap);
    return r;
}

ADLMIDI_EXPORT int ADL_UserInterface::PrintLn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = Print(2/*column*/, 8/*color*/, true/*line*/, fmt, ap);
    va_end(ap);
    return r;
}

ADLMIDI_EXPORT void ADL_UserInterface::IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
{
    HideCursor();
#if 1
    int notex = 2 + (note+55)%(NColumns-3);
    int limit = WinHeight(), minline = 3;

    int notey = minline + adlchn;
    notey %= std::max(1, limit-minline);
    notey += minline;
    notey %= limit;

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
    // Special exceptions for '.' (background slot)
    //                        '&' (tetris edges)
    Draw(notex,notey,
         pressure!=0
            ? AllocateColor(ins)        /* use note's color if active */
            : illustrate_char=='.' ? backgroundcolor[notex][notey]
                                     : illustrate_char=='&' ? 1
                                                            : 8,
         illustrate_char);
#endif
    std::fflush(stderr);
}

ADLMIDI_EXPORT void ADL_UserInterface::Draw(int notex, int notey, int color, char ch)
{
    if(slots[notex][notey] != ch
            || slotcolors[notex][notey] != color)
    {
        slots[notex][notey] = ch;
        slotcolors[notex][notey] = color;
        GotoXY(notex, notey);
        Color(color);
        PutC(ch);

        if(!touched[notey])
        {
            touched[notey] = true;

            GotoXY(0, notey);
            for(int tx=0; tx<int(NColumns); ++tx)
            {
                if(slots[tx][notey] != '.')
                {
                    Color(slotcolors[tx][notey]);
                    PutC(slots[tx][notey]);
                }
                else
                {
                    Color(backgroundcolor[tx][notey]);
                    PutC(background[tx][notey]);
                }
            }
        }
    }
}

ADLMIDI_EXPORT void ADL_UserInterface::IllustrateVolumes(double left, double right)
{
    const unsigned maxy = WinHeight();
    const unsigned white_threshold  = maxy/23;
    const unsigned red_threshold    = maxy*4/23;
    const unsigned yellow_threshold = maxy*8/23;

    double amp[2] = {left*maxy, right*maxy};
    for(unsigned y=2; y<maxy; ++y)
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
    std::fflush(stderr);
}

ADLMIDI_EXPORT void ADL_UserInterface::GotoXY(int newx, int newy)
{
    // Record the maximum line count seen
    if(newy > maxy) maxy = newy;
    // Go down with '\n' (resets cursor at beginning of line)
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
    // Go up with \33[A
    if(newy < y) { RawPrn("\33[%dA", y-newy); y = newy; }
    // Adjust X coordinate
    if(newx != x)
    {
        // Use '\r' to go to column 0
        if(newx == 0) // || (newx<10 && std::abs(newx-x)>=10))
        { std::fputc('\r', stderr); x = 0; }
        // Go left  with \33[D
        if(newx < x) RawPrn("\33[%dD", x-newx);
        // Go right with \33[C
        if(newx > x) RawPrn("\33[%dC", newx-x);
        x = newx;
    }
}

ADLMIDI_EXPORT void ADL_UserInterface::Color(int newcolor)
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
            RawPrn("\33[0;%s40;3%c", (newcolor&8) ? "1;" : "", map[newcolor&7]);
            // If xterm-256color is used, try using improved colors:
            //        Translate 8 (dark gray) into #003366 (bluish dark cyan)
            //        Translate 1 (dark blue) into #000033 (darker blue)
            if(newcolor==8) RawPrn(";38;5;24;25");
            if(newcolor==6) RawPrn(";38;5;101;25");
            if(newcolor==3) RawPrn(";38;5;61;25");
            if(newcolor==1) RawPrn(";38;5;17;25");
            RawPrn("m");
        }
        color = newcolor;
    }
}

ADLMIDI_EXPORT int ADL_UserInterface::AllocateColor(int ins)
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

void ADL_UserInterface::SetTetrisInput(ADL_Input *input)
{
#ifdef SUPPORT_PUZZLE_GAME
    ADLMIDI_PuzzleGame::Input = input;
#else
    (void)input;
#endif
}

ADLMIDI_EXPORT bool ADL_UserInterface::DoCheckTetris()
{
#ifdef SUPPORT_PUZZLE_GAME
    static ADLMIDI_PuzzleGame::TetrisAI    player(2);
    static ADLMIDI_PuzzleGame::TetrisHuman computer(31);

    int a = player.GameLoop();
    int b = computer.GameLoop();

    if(a >= 0 && b >= 0)
    {
        player.incoming   += b;
        computer.incoming += a;
    }
    return player.DelayOpinion() >= 0
            && computer.DelayOpinion() >= 0;
#else
    return true;
#endif
}

ADLMIDI_EXPORT bool ADL_UserInterface::CheckTetris()
{
    if(TetrisLaunched) return DoCheckTetris();
    return true;
}

#ifdef SUPPORT_PUZZLE_GAME
namespace ADLMIDI_PuzzleGame
{
    static void PutCell(int x, int y, unsigned cell)
    {
        static const unsigned char valid_attrs[] = {8,6,5,3};
        unsigned char ch = cell, attr = cell >> 8;
        int height = WinHeight();//std::min(NumCards*18, 50u);
        y = std::max(0, int(std::min(height, 40) - 25 + y));
        if(ch == 0xDB) ch = '#';
        if(ch == 0xB0) ch = '*';
        if(attr != 1) attr = valid_attrs[attr % sizeof(valid_attrs)];

        //bool diff = UI.background[x][y] != UI.slots[x][y];
        if(UI)
        {
            UI->backgroundcolor[x][y] = attr;
            UI->background[x][y]      = ch;
            UI->GotoXY(x,y);
            UI->Color(attr);
            UI->PutC(ch);
        }
        //UI.Draw(x,y, attr, ch);
    }
}
#endif
