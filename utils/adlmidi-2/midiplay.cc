//#ifdef __MINGW32__
//typedef struct vswprintf {} swprintf;
//#endif

#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <cmath>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdarg.h>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil

#include <assert.h>

#include "input.hpp"

//#ifndef _WIN32
//#define SUPPORT_VIDEO_OUTPUT// MOVED TO CMake build script
//#define SUPPORT_PUZZLE_GAME// MOVED TO CMake build script
//#endif

#if !defined(_WIN32) || !defined(_MSC_VER)
#define ATTRIBUTE_FORMAT_PRINTF(x, y) __attribute__((format(printf, x, y)))
#else
#define ATTRIBUTE_FORMAT_PRINTF(x, y)
#endif

#ifdef _WIN32
# include <cctype>
# define WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX //To don't damage std::min and std::max
# endif
# include <windows.h>
# include <mmsystem.h>
#endif

#if defined(_WIN32) || defined(__DJGPP__)
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned Uint32;
#else
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

class MutexType
{
    SDL_mutex *mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType()
    {
        SDL_DestroyMutex(mut);
    }
    void Lock()
    {
        SDL_mutexP(mut);
    }
    void Unlock()
    {
        SDL_mutexV(mut);
    }
};
#endif



#include <deque>
#include <algorithm>

#include <signal.h>

#include "adlmidi.h"
#include "adlmidi.hpp"

#ifndef __DJGPP__

static const unsigned long PCM_RATE = 48000;
static const unsigned MaxCards = 100;
static const unsigned MaxSamplesAtTime = 512; // 512=dbopl limitation
#else // DJGPP
static const unsigned MaxCards = 1;
static const unsigned OPLBase = 0x388;
#endif
static unsigned NumCards   = 2;
static bool AdlPercussionMode = false;
static bool ReverbIsOn = true;
static bool QuitFlag = false, FakeDOSshell = false;
static bool DoingInstrumentTesting = false;
static bool WritePCMfile = false;
static std::string PCMfilepath = "adlmidi.wav";
#ifdef SUPPORT_VIDEO_OUTPUT
static std::string VidFilepath = "adlmidi.mkv";
static bool WriteVideoFile = false;
#endif
static unsigned WindowLines = 0;
static bool WritingToTTY;

static unsigned WinHeight()
{
    unsigned result =
        AdlPercussionMode
        ? std::min(2u, NumCards) * 23
        : std::min(3u, NumCards) * 18;

    if(WindowLines) result = std::min(WindowLines - 1, result);
    return result;
}

#if (!defined(_WIN32) || defined(__CYGWIN__)) && defined(TIOCGWINSZ)
extern "C" {
    static void SigWinchHandler(int);
}
static void SigWinchHandler(int)
{
    struct winsize w;
    if(ioctl(2, TIOCGWINSZ, &w) >= 0 || ioctl(1, TIOCGWINSZ, &w) >= 0 || ioctl(0, TIOCGWINSZ, &w) >= 0)
        WindowLines = w.ws_row;
}
#else
static void SigWinchHandler(int) {}
#endif

static void GuessInitialWindowHeight()
{
    char *s = std::getenv("LINES");
    if(s && std::atoi(s)) WindowLines = (unsigned)std::atoi(s);
    SigWinchHandler(0);
}



xInput Input;

#ifdef SUPPORT_PUZZLE_GAME
#include "puzzlegame.hpp"
#endif

#ifdef SUPPORT_VIDEO_OUTPUT
class UIfontBase
{
public:
    explicit UIfontBase() {}
    virtual ~UIfontBase() {}
};
static unsigned UnicodeToASCIIapproximation(unsigned n)
{
    return n;
}
//#include "6x9.hpp"
#include "9x15.hpp"
#endif

static const char MIDIsymbols[256 + 1] =
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

static class UserInterface
{
#ifdef SUPPORT_PUZZLE_GAME
    ADLMIDI_PuzzleGame::TetrisAI    player;
    ADLMIDI_PuzzleGame::TetrisAI    computer;
#endif
public:
    static constexpr unsigned NColumns = 1216 / 20;
    #ifdef SUPPORT_VIDEO_OUTPUT
    static constexpr unsigned VidWidth  = 1216, VidHeight = 2160;
    static constexpr unsigned FontWidth =   20, FontHeight = 45;
    static constexpr unsigned TxWidth   = (VidWidth / FontWidth), TxHeight = (VidHeight / FontHeight);
    unsigned int   PixelBuffer[VidWidth * VidHeight] = {0};
    unsigned short CharBuffer[TxWidth * TxHeight] = {0};
    bool           DirtyCells[TxWidth * TxHeight] = {false};
    unsigned       NDirty = 0;
    #endif
    #ifdef _WIN32
    void *handle;
    #endif
    int x, y, color, txtline, maxy;

    // Text:
    char background[NColumns][1 + 23 * MaxCards];
    unsigned char backgroundcolor[NColumns][1 + 23 * MaxCards];
    bool          touched[1 + 23 * MaxCards] {false};
    // Notes:
    char slots[NColumns][1 + 23 * MaxCards];
    unsigned char slotcolors[NColumns][1 + 23 * MaxCards];

    bool cursor_visible;
    char stderr_buffer[256];
public:
#ifdef __DJGPP__
#   define RawPrn cprintf
#else
    static void RawPrn(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2)
    {
        // Note: should essentially match PutC, except without updates to x
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        #ifdef _WIN32
        fflush(stderr);
        #endif
    }
#endif

    UserInterface():
    #ifdef SUPPORT_PUZZLE_GAME
        player(2),
        computer(31),
    #endif
        x(0), y(0), color(-1), txtline(0),
        maxy(0), cursor_visible(true)
    {
        GuessInitialWindowHeight();
        #ifdef _WIN32
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
        GotoXY(41, 13);
        CONSOLE_SCREEN_BUFFER_INFO tmp;
        GetConsoleScreenBufferInfo(handle, &tmp);
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
        #if (!defined(_WIN32) || defined(__CYGWIN__)) && defined(TIOCGWINSZ)
        std::signal(SIGWINCH, SigWinchHandler);
        #endif
        #ifdef __DJGPP__
        color = 7;
        #endif
        std::memset(slots, '.',      sizeof(slots));
        std::memset(background, '.', sizeof(background));
        std::memset(backgroundcolor, 1, sizeof(backgroundcolor));
        #ifndef _WIN32
        setbuffer(stderr, stderr_buffer, sizeof(stderr_buffer));
        #endif
        RawPrn("\r"); // Ensure cursor is at the x=0 we imagine it being
        Print(0, 7, true, "Hit Ctrl-C to quit");
    }
    void HideCursor()
    {
        if(!cursor_visible) return;
        cursor_visible = false;
        #ifdef _WIN32
        if(handle)
        {
            const CONSOLE_CURSOR_INFO info = {100, false};
            SetConsoleCursorInfo(handle, &info);
            if(!DoingInstrumentTesting)
                CheckTetris();
            return;
        }
        #endif
        if(!DoingInstrumentTesting)
            CheckTetris();
        #ifdef __DJGPP__
        {
            _setcursortype(_NOCURSOR);
            return;
        }
        #endif
        RawPrn("\33[?25l"); // hide cursor
    }
    void ShowCursor()
    {
        if(cursor_visible) return;
        cursor_visible = true;
        GotoXY(0, maxy);
        Color(7);
        #ifdef _WIN32
        if(handle)
        {
            const CONSOLE_CURSOR_INFO info = {100, true};
            SetConsoleCursorInfo(handle, &info);
            return;
        }
        #endif
        #ifdef __DJGPP__
        {
            _setcursortype(_NORMALCURSOR);
            return;
        }
        #endif
        RawPrn("\33[?25h"); // show cursor
        std::fflush(stderr);
    }
    void VidPut(char c)
    {
        #ifndef SUPPORT_VIDEO_OUTPUT
        c = c;
        #else
        unsigned clr = (unsigned)color, tx = (unsigned)x, ty = (unsigned)y;
        unsigned cell_index = ty * TxWidth + tx;
        if(cell_index < TxWidth * TxHeight)
        {
            unsigned short what = (unsigned short)(clr << 8) + (unsigned short)(unsigned(c) & 0xFF);
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
    static unsigned VidTranslateColor(unsigned c)
    {
        static const unsigned colors[16] =
        {
            0x000000, 0x00005F, 0x00AA00, 0x5F5FAF,
            0xAA0000, 0xAA00AA, 0x87875F, 0xAAAAAA,
            0x005F87, 0x5555FF, 0x55FF55, 0x55FFFF,
            0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
        };
        return colors[c % 16];
    }
    void VidRenderCh(unsigned x, unsigned y, unsigned attr, const unsigned char *bitmap)
    {
        unsigned bg = VidTranslateColor(attr >> 4), fg = VidTranslateColor(attr);
        for(unsigned line = 0; line < FontHeight; ++line)
        {
            unsigned int *pix = PixelBuffer + (y * FontHeight + line) * VidWidth + x * FontWidth;

            unsigned char bm = bitmap[line * 15 / FontHeight];
            for(unsigned w = 0; w < FontWidth; ++w)
            {
                int shift0 = 7 - w    * 9 / FontWidth;
                int shift1 = 7 - (w - 1) * 9 / FontWidth;
                int shift2 = 7 - (w + 1) * 9 / FontWidth;
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
    void VidRender()
    {
        static const font9x15 font;
        if(NDirty)
        {
            #pragma omp parallel for schedule(static)
            for(unsigned scan = 0; scan < TxWidth * TxHeight; ++scan)
                if(DirtyCells[scan])
                {
                    --NDirty;
                    DirtyCells[scan] = false;
                    VidRenderCh(scan % TxWidth, scan / TxWidth,
                                CharBuffer[scan] >> 8,
                                font.GetBitmap() + 15 * font.GetIndex(CharBuffer[scan] & 0xFF));
                }
        }
    }
    #endif
    void PutC(char c)
    {
        #ifdef _WIN32
        if(handle) WriteConsole(handle, &c, 1, 0, 0);
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

    int Print(unsigned beginx, unsigned color, bool ln, const char *fmt, va_list ap)
    {
        char Line[1024];
        #ifndef __CYGWIN__
        int nchars = std::vsnprintf(Line, sizeof(Line), fmt, ap);
        #else
        int nchars = std::vsprintf(Line, fmt, ap); /* SECURITY: POSSIBLE BUFFER OVERFLOW (Cygwin) */
        #endif
        //if(nchars == 0) return nchars;

        HideCursor();
        for(unsigned tx = beginx; tx < NColumns; ++tx)
        {
            int n = (int)(tx - beginx); // Index into Line[]
            if(n < nchars && Line[n] != '\n')
            {
                GotoXY((int)tx, txtline);
                Color(backgroundcolor[tx][txtline] = (unsigned char)(/*Line[n] == '.' ? 1 :*/ color));
                PutC(background[tx][txtline] = Line[n]);
            }
            else //if(background[tx][txtline]!='.' && slots[tx][txtline]=='.')
            {
                if(!ln) break;
                GotoXY((int)tx, txtline);
                Color(backgroundcolor[tx][txtline] = 1);
                PutC(background[tx][txtline] = '.');
            }
        }
        std::fflush(stderr);

        if(ln)
        {
            txtline = (txtline + 1) % (int)WinHeight();
        }

        return nchars;
    }
    int Print(unsigned beginx, unsigned color, bool ln, const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(5, 6)
    {
        va_list ap;
        va_start(ap, fmt);
        int r = Print(beginx, color, ln, fmt, ap);
        va_end(ap);
        return r;
    }
    int PrintLn(const char *fmt, ...) ATTRIBUTE_FORMAT_PRINTF(2, 3)
    {
        va_list ap;
        va_start(ap, fmt);
        int r = Print(2/*column*/, 8/*color*/, true/*line*/, fmt, ap);
        va_end(ap);
        return r;
    }
    void IllustrateNote(int adlchn, int note, int ins, int pressure, double bend)
    {
        HideCursor();
        #if 1
        int notex = 2 + (note + 55) % ((int)NColumns - 3);
        int limit = (int)WinHeight(), minline = 3;

        int notey = minline + adlchn;
        notey %= std::max(1, limit - minline);
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
        Draw(notex, notey,
             pressure != 0
             ? AllocateColor(ins)        /* use note's color if active */
             : illustrate_char == '.' ? backgroundcolor[notex][notey]
             : illustrate_char == '&' ? 1
             : 8,
             illustrate_char);
        #endif
        std::fflush(stderr);
    }

    void Draw(int notex, int notey, int color, char ch)
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
                for(int tx = 0; tx < int(NColumns); ++tx)
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

    void IllustrateVolumes(double left, double right)
    {
        const unsigned maxy = WinHeight();
        const unsigned white_threshold  = maxy / 23;
        const unsigned red_threshold    = maxy * 4 / 23;
        const unsigned yellow_threshold = maxy * 8 / 23;

        double amp[2] = {left * maxy, right * maxy};
        for(unsigned y = 2; y < maxy; ++y)
            for(unsigned w = 0; w < 2; ++w)
            {
                char c = amp[w] > (maxy - 1) - y ? '|' : background[w][y + 1];
                Draw((int)w, (int)y + 1,
                     c == '|' ? y < white_threshold ? 15
                     : y < red_threshold ? 12
                     : y < yellow_threshold ? 14
                     : 10 : (c == '.' ? 1 : 8),
                     c);
            }
        std::fflush(stderr);
    }

    // Move tty cursor to the indicated position.
    // Movements will be done in relative terms
    // to the current cursor position only.
    void GotoXY(int newx, int newy)
    {
        // Record the maximum line count seen
        if(newy > maxy) maxy = newy;
        // Go down with '\n' (resets cursor at beginning of line)
        while(newy > y)
        {
            std::fputc('\n', stderr);
            y += 1;
            x = 0;
        }
        #ifdef _WIN32
        if(handle)
        {
            CONSOLE_SCREEN_BUFFER_INFO tmp;
            GetConsoleScreenBufferInfo(handle, &tmp);
            COORD tmp2 = { (SHORT)(x = newx), (SHORT)tmp.dwCursorPosition.Y };
            if(newy < y)
            {
                tmp2.Y -= (y - newy);
                y = newy;
            }
            SetConsoleCursorPosition(handle, tmp2);
        }
        #endif
        #ifdef __DJGPP__
        {
            gotoxy(x = newx, wherey() - (y - newy));
            y = newy;
            return;
        }
        #endif
        // Go up with \33[A
        if(newy < y)
        {
            RawPrn("\33[%dA", y - newy);
            y = newy;
        }
        // Adjust X coordinate
        if(newx != x)
        {
            // Use '\r' to go to column 0
            if(newx == 0) // || (newx<10 && std::abs(newx-x)>=10))
            {
                std::fputc('\r', stderr);
                x = 0;
            }
            // Go left  with \33[D
            if(newx < x) RawPrn("\33[%dD", x - newx);
            // Go right with \33[C
            if(newx > x) RawPrn("\33[%dC", newx - x);
            x = newx;
        }
    }
    // Set color (4-bit). Bits: 1=blue, 2=green, 4=red, 8=+intensity
    void Color(int newcolor)
    {
        if(color != newcolor)
        {
            #ifdef _WIN32
            if(handle)
                SetConsoleTextAttribute(handle, newcolor);
            else
            #endif
            #ifdef __DJGPP__
                textattr(newcolor);
            if(0)
            #endif
            {
                static const char map[8 + 1] = "04261537";
                RawPrn("\33[0;%s40;3%c", (newcolor & 8) ? "1;" : "", map[newcolor & 7]);
                // If xterm-256color is used, try using improved colors:
                //        Translate 8 (dark gray) into #003366 (bluish dark cyan)
                //        Translate 1 (dark blue) into #000033 (darker blue)
                if(newcolor == 8) RawPrn(";38;5;24;25");
                if(newcolor == 6) RawPrn(";38;5;101;25");
                if(newcolor == 3) RawPrn(";38;5;61;25");
                if(newcolor == 1) RawPrn(";38;5;17;25");
                RawPrn("m");
            }
            color = newcolor;
        }
    }
    // Choose a permanent color for given instrument
    int AllocateColor(int ins)
    {
        static char ins_colors[256] = { 0 }, ins_color_counter = 0;
        if(ins_colors[ins])
            return ins_colors[ins];
        if(ins & 0x80)
        {
            static const char shuffle[] = {2, 3, 4, 5, 6, 7};
            return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
        }
        else
        {
            static const char shuffle[] = {10, 11, 12, 13, 14, 15};
            return ins_colors[ins] = shuffle[ins_color_counter++ % 6];
        }
    }

    bool DoCheckTetris()
    {
        #ifdef SUPPORT_PUZZLE_GAME
        int a = player.GameLoop();
        int b = computer.GameLoop();

        if(a >= 0 && b >= 0)
        {
            player.incoming   += (unsigned)b;
            computer.incoming += (unsigned)a;
        }
        return player.DelayOpinion() >= 0
               && computer.DelayOpinion() >= 0;
        #else
        return true;
        #endif
    }

    bool TetrisLaunched = false;
    bool CheckTetris()
    {
        if(TetrisLaunched) return DoCheckTetris();
        return true;
    }
} UI;

#ifdef SUPPORT_PUZZLE_GAME
void ADLMIDI_PuzzleGame::PutCell(int x, int y, unsigned cell)
{
    static const unsigned char valid_attrs[] = {8, 6, 5, 3};
    unsigned char ch = (unsigned char)cell, attr = (unsigned char)(cell >> 8);
    int height = (int)WinHeight();//std::min(NumCards*18, 50u);
    y = std::max(0, int(std::min(height, 40) - 25 + y));
    if(ch == 0xDB) ch = '#';
    if(ch == 0xB0) ch = '*';
    if(attr != 1) attr = valid_attrs[attr % sizeof(valid_attrs)];

    //bool diff = UI.background[x][y] != UI.slots[x][y];
    UI.backgroundcolor[x][y] = attr;
    UI.background[x][y]      = (char)ch;
    UI.GotoXY(x, y);
    UI.Color(attr);
    UI.PutC((char)ch);
    //UI.Draw(x,y, attr, ch);
}
#endif

#ifndef __DJGPP__
struct Reverb /* This reverb implementation is based on Freeverb impl. in Sox */
{
    float feedback, hf_damping, gain;
    struct FilterArray
    {
        struct Filter
        {
            std::vector<float> Ptr;
            size_t pos;
            float Store;
            void Create(size_t size)
            {
                Ptr.resize(size);
                pos = 0;
                Store = 0.f;
            }
            float Update(float a, float b)
            {
                Ptr[pos] = a;
                if(!pos) pos = Ptr.size() - 1;
                else --pos;
                return b;
            }
            float ProcessComb(float input, const float feedback, const float hf_damping)
            {
                Store = Ptr[pos] + (Store - Ptr[pos]) * hf_damping;
                return Update(input + feedback * Store, Ptr[pos]);
            }
            float ProcessAllPass(float input)
            {
                return Update(input + Ptr[pos] * .5f, Ptr[pos] - input);
            }
        } comb[8], allpass[4];
        void Create(double rate, double scale, double offset)
        {
            /* Filter delay lengths in samples (44100Hz sample-rate) */
            static const int comb_lengths[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
            static const int allpass_lengths[4] = {225, 341, 441, 556};
            double r = rate * (1 / 44100.0); // Compensate for actual sample-rate
            const int stereo_adjust = 12;
            for(size_t i = 0; i < 8; ++i, offset = -offset)
                comb[i].Create(scale * r * (comb_lengths[i] + stereo_adjust * offset) + .5);
            for(size_t i = 0; i < 4; ++i, offset = -offset)
                allpass[i].Create(r * (allpass_lengths[i] + stereo_adjust * offset) + .5);
        }
        void Process(size_t length,
                     const std::deque<float> &input, std::vector<float> &output,
                     const float feedback, const float hf_damping, const float gain)
        {
            for(size_t a = 0; a < length; ++a)
            {
                float out = 0, in = input[a];
                for(size_t i = 8; i-- > 0;) out += comb[i].ProcessComb(in, feedback, hf_damping);
                for(size_t i = 4; i-- > 0;) out += allpass[i].ProcessAllPass(out);
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
        feedback = 1 - std::exp((reverberance * 100.0 - b) / (a * b));
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
        for(size_t i = 0; i < 2; ++i)
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
        for(size_t i = 0; i < 2; ++i)
            chan[i].Create(PCM_RATE,
                           6.0,  // wet_gain_dB  (-10..10)
                           .7,   // room_scale   (0..1)
                           .6,   // reverberance (0..1)
                           .8,   // hf_damping   (0..1)
                           .000, // pre_delay_s  (0.. 0.5)
                           1,   // stereo_depth (0..1)
                           MaxSamplesAtTime);
    }
} reverb_data;

#ifdef _WIN32
namespace WindowsAudio
{
    static const unsigned BUFFER_COUNT = 16;
    static const unsigned BUFFER_SIZE  = 8192;
    static HWAVEOUT hWaveOut;
    static WAVEHDR headers[BUFFER_COUNT];
    static volatile unsigned buf_read = 0, buf_write = 0;

    static void CALLBACK Callback(HWAVEOUT, UINT msg, DWORD, DWORD, DWORD)
    {
        if(msg == WOM_DONE)
        {
            buf_read = (buf_read + 1) % BUFFER_COUNT;
        }
    }
    static void Open(const int rate, const int channels, const int bits)
    {
        WAVEFORMATEX wformat;
        MMRESULT result;

        //fill waveformatex
        memset(&wformat, 0, sizeof(wformat));
        wformat.nChannels       = channels;
        wformat.nSamplesPerSec  = rate;
        wformat.wFormatTag      = WAVE_FORMAT_PCM;
        wformat.wBitsPerSample  = bits;
        wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
        wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;

        //open sound device
        //WAVE_MAPPER always points to the default wave device on the system
        result = waveOutOpen
                 (
                     &hWaveOut, WAVE_MAPPER, &wformat,
                     (DWORD_PTR)Callback, 0, CALLBACK_FUNCTION
                 );
        if(result == WAVERR_BADFORMAT)
        {
            fprintf(stderr, "ao_win32: format not supported\n");
            return;
        }
        if(result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "ao_win32: unable to open wave mapper device\n");
            return;
        }
        char *buffer = new char[BUFFER_COUNT * BUFFER_SIZE];
        std::memset(headers, 0, sizeof(headers));
        std::memset(buffer, 0, BUFFER_COUNT * BUFFER_SIZE);
        for(unsigned a = 0; a < BUFFER_COUNT; ++a)
            headers[a].lpData = buffer + a * BUFFER_SIZE;
    }
    static void Close()
    {
        waveOutReset(hWaveOut);
        waveOutClose(hWaveOut);
    }
    static void Write(const unsigned char *Buf, unsigned len)
    {
        static std::vector<unsigned char> cache;
        size_t cache_reduction = 0;
        if(0 && len < BUFFER_SIZE && cache.size() + len <= BUFFER_SIZE)
        {
            cache.insert(cache.end(), Buf, Buf + len);
            Buf = &cache[0];
            len = cache.size();
            if(len < BUFFER_SIZE / 2)
                return;
            cache_reduction = cache.size();
        }

        while(len > 0)
        {
            unsigned buf_next = (buf_write + 1) % BUFFER_COUNT;
            WAVEHDR *Work = &headers[buf_write];
            while(buf_next == buf_read)
            {
                /*UI.Color(4);
                UI.GotoXY(60,-5+5); fprintf(stderr, "waits\r"); UI.x=0; std::fflush(stderr);
                UI.Color(4);
                UI.GotoXY(60,-4+5); fprintf(stderr, "r%u w%u n%u\r",buf_read,buf_write,buf_next); UI.x=0; std::fflush(stderr);
                */
                /* Wait until at least one of the buffers is free */
                Sleep(0);
                /*UI.Color(2);
                UI.GotoXY(60,-3+5); fprintf(stderr, "wait completed\r"); UI.x=0; std::fflush(stderr);*/
            }

            unsigned npending = (buf_write + BUFFER_COUNT - buf_read) % BUFFER_COUNT;
            static unsigned counter = 0, lo = 0;
            if(!counter-- || npending < lo)
            {
                lo = npending;
                counter = 100;
            }

            if(!DoingInstrumentTesting)
            {
                if(UI.maxy >= 5)
                {
                    UI.Color(9);
                    UI.GotoXY(70, -5 + 6);
                    fprintf(stderr, "%3u bufs\r", (unsigned)npending);
                    UI.x = 0;
                    std::fflush(stderr);
                    UI.GotoXY(71, -4 + 6);
                    fprintf(stderr, "lo:%3u\r", lo);
                    UI.x = 0;
                }
            }

            //unprepare the header if it is prepared
            if(Work->dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
            unsigned x = BUFFER_SIZE;
            if(x > len) x = len;
            std::memcpy(Work->lpData, Buf, x);
            Buf += x;
            len -= x;
            //prepare the header and write to device
            Work->dwBufferLength = x;
            {
                int err = waveOutPrepareHeader(hWaveOut, Work, sizeof(WAVEHDR));
                if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutPrepareHeader: %d\n", err);
            }
            {
                int err = waveOutWrite(hWaveOut, Work, sizeof(WAVEHDR));
                if(err != MMSYSERR_NOERROR) fprintf(stderr, "waveOutWrite: %d\n", err);
            }
            buf_write = buf_next;
            //if(npending>=BUFFER_COUNT-2)
            //    buf_read=(buf_read+1)%BUFFER_COUNT; // Simulate a read
        }
        if(cache_reduction)
            cache.erase(cache.begin(), cache.begin() + cache_reduction);
    }
}
#else
static std::deque<short> AudioBuffer;
static MutexType AudioBuffer_lock;
static void AdlAudioCallback(void *, Uint8 *stream, int len)
{
    SDL_LockAudio();
    short *target = (short *) stream;
    AudioBuffer_lock.Lock();
    /*if(len != AudioBuffer.size())
        fprintf(stderr, "len=%d stereo samples, AudioBuffer has %u stereo samples",
            len/4, (unsigned) AudioBuffer.size()/2);*/
    unsigned ate = len / 2; // number of shorts
    if(ate > AudioBuffer.size()) ate = AudioBuffer.size();
    for(unsigned a = 0; a < ate; ++a)
        target[a] = AudioBuffer[a];
    AudioBuffer.erase(AudioBuffer.begin(), AudioBuffer.begin() + ate);
    //fprintf(stderr, " - remain %u\n", (unsigned) AudioBuffer.size()/2);
    AudioBuffer_lock.Unlock();
    SDL_UnlockAudio();
}
#endif // WIN32

struct FourChars
{
    char ret[4];

    FourChars(const char *s)
    {
        for(unsigned c = 0; c < 4; ++c) ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c = 0; c < 4; ++c) ret[c] = (w >> (c * 8)) & 0xFF;
    }
};



static void SendStereoAudio(unsigned long count, short *samples)
{
    if(count > MaxSamplesAtTime)
    {
        SendStereoAudio(MaxSamplesAtTime, samples);
        SendStereoAudio(count - MaxSamplesAtTime, samples + MaxSamplesAtTime);
        return;
    }
    #if 0
    if(count % 2 == 1)
    {
        // An uneven number of samples? To avoid complicating matters,
        // just ignore the odd sample.
        count   -= 1;
        samples += 1;
    }
    #endif
    if(!count) return;

    // Attempt to filter out the DC component. However, avoid doing
    // sudden changes to the offset, for it can be audible.
    double average[2] = {0, 0};
    for(unsigned w = 0; w < 2; ++w)
        for(unsigned long p = 0; p < count; ++p)
            average[w] += samples[p * 2 + w];
    static float prev_avg_flt[2] = {0, 0};
    float average_flt[2] =
    {
        prev_avg_flt[0] = (float)((double(prev_avg_flt[0]) + average[0] * 0.04 / double(count)) / 1.04),
        prev_avg_flt[1] = (float)((double(prev_avg_flt[1]) + average[1] * 0.04 / double(count)) / 1.04)
    };

    // Figure out the amplitude of both channels
    if(!DoingInstrumentTesting)
    {
        static unsigned amplitude_display_counter = 0;
        if(!amplitude_display_counter--)
        {
            amplitude_display_counter = (PCM_RATE / count) / 24;
            double amp[2] = {0, 0};
            for(unsigned w = 0; w < 2; ++w)
            {
                average[w] /= double(count);
                for(unsigned long p = 0; p < count; ++p)
                    amp[w] += std::fabs(samples[p * 2 + w] - average[w]);
                amp[w] /= double(count);
                // Turn into logarithmic scale
                const double dB = std::log(amp[w] < 1 ? 1 : amp[w]) * 4.328085123;
                const double maxdB = 3 * 16; // = 3 * log2(65536)
                amp[w] = dB / maxdB;
            }
            UI.IllustrateVolumes(amp[0], amp[1]);
        }
    }

    //static unsigned counter = 0; if(++counter < 8000)  return;

    #if defined(_WIN32) && 0
    // Cheat on dosbox recording: easier on the cpu load.
    {
        count *= 2;
        std::vector<short> AudioBuffer(count);
        for(unsigned long p = 0; p < count; ++p)
            AudioBuffer[p] = samples[p];
        WindowsAudio::Write((const unsigned char *) &AudioBuffer[0], count * 2);
        return;
    }
    #endif

    #ifdef _WIN32
    std::vector<short> AudioBuffer(count * 2);
    const size_t pos = 0;
    #else
    AudioBuffer_lock.Lock();
    size_t pos = AudioBuffer.size();
    AudioBuffer.resize(pos + count * 2);
    #endif

    if(ReverbIsOn)
    {
        // Convert input to float format
        std::vector<float> dry[2];
        for(unsigned w = 0; w < 2; ++w)
        {
            dry[w].resize(count);
            float a = average_flt[w];
            for(unsigned long p = 0; p < count; ++p)
            {
                int   s = samples[p * 2 + w];
                dry[w][p] = (s - a) * double(0.3 / 32768.0);
            }
            // ^  Note: ftree-vectorize causes an error in this loop on g++-4.4.5
            reverb_data.chan[w].input_fifo.insert(
                reverb_data.chan[w].input_fifo.end(),
                dry[w].begin(), dry[w].end());
        }
        // Reverbify it
        for(unsigned w = 0; w < 2; ++w)
            reverb_data.chan[w].Process(count);

        // Convert to signed 16-bit int format and put to playback queue
        for(unsigned long p = 0; p < count; ++p)
            for(unsigned w = 0; w < 2; ++w)
            {
                float out = ((1 - reverb_data.wetonly) * dry[w][p] +
                             .5 * (reverb_data.chan[0].out[w][p]
                                   + reverb_data.chan[1].out[w][p])) * 32768.0f
                            + average_flt[w];
                AudioBuffer[pos + p * 2 + w] =
                    out < -32768.f ? -32768 :
                    out > 32767.f ?  32767 : out;
            }
    }
    else
    {
        for(unsigned long p = 0; p < count; ++p)
            for(unsigned w = 0; w < 2; ++w)
            {
//                float out = ((1 - reverb_data.wetonly) * dry[w][p] +
//                             .5 * (reverb_data.chan[0].out[w][p]
//                                   + reverb_data.chan[1].out[w][p])) * 32768.0f
//                            + average_flt[w];
                AudioBuffer[pos + p * 2 + w] = samples[p * 2 + w];
            }
    }


    if(WritePCMfile)
    {
        /* HACK: Cheat on DOSBox recording: Record audio separately on Windows. */
        static FILE *fp = nullptr;
        if(!fp)
        {
            fp = PCMfilepath == "-" ? stdout
                 : fopen(PCMfilepath.c_str(), "wb");
            if(fp)
            {
                FourChars Bufs[] =
                {
                    "RIFF", (0x24u),  // RIFF type, file length - 8
                    "WAVE",           // WAVE file
                    "fmt ", (0x10u),  // fmt subchunk, which is 16 bytes:
                    "\1\0\2\0",     // PCM (1) & stereo (2)
                    (48000u),     // sampling rate
                    (48000u * 2 * 2), // byte rate
                    "\2\0\20\0",    // block align & bits per sample
                    "data", (0x00u)  //  data subchunk, which is so far 0 bytes.
                };
                for(unsigned c = 0; c < sizeof(Bufs) / sizeof(*Bufs); ++c)
                    std::fwrite(Bufs[c].ret, 1, 4, fp);
            }
        }

        // Using a loop, because our data type is a deque, and
        // the data might not be contiguously stored in memory.
        for(unsigned long p = 0; p < 2 * count; ++p)
            std::fwrite(&AudioBuffer[pos + p], 1, 2, fp);

        /* Update the WAV header */
        if(true)
        {
            long pos = std::ftell(fp);
            if(pos != -1)
            {
                long datasize = pos - 0x2C;
                if(std::fseek(fp, 4,  SEEK_SET) == 0) // Patch the RIFF length
                    std::fwrite(FourChars(0x24u + datasize).ret, 1, 4, fp);
                if(std::fseek(fp, 40, SEEK_SET) == 0) // Patch the data length
                    std::fwrite(FourChars(datasize).ret, 1, 4, fp);
                std::fseek(fp, pos, SEEK_SET);
            }
        }

        std::fflush(fp);

        //if(std::ftell(fp) >= 48000*4*10*60)
        //    raise(SIGINT);
    }

    #ifdef SUPPORT_VIDEO_OUTPUT
    if(WriteVideoFile)
    {
        static constexpr unsigned framerate = 15;
        static FILE *fp = nullptr;
        static unsigned long samples_carry = 0;

        if(!fp)
        {
            std::string cmdline =
                "ffmpeg -f rawvideo"
                " -pixel_format bgra "
                " -video_size " + std::to_string(UI.VidWidth) + "x" + std::to_string(UI.VidHeight) +
                " -framerate " + std::to_string(framerate) +
                " -i -"
                " -c:v h264"
                " -aspect " + std::to_string(UI.VidWidth) + "/" + std::to_string(UI.VidHeight) +
                " -pix_fmt yuv420p"
                " -preset superfast -partitions all -refs 2 -tune animation -y '" + VidFilepath + "'"; // FIXME: escape filename
            cmdline += " >/dev/null 2>/dev/null";
            fp = popen(cmdline.c_str(), "w");
        }
        if(fp)
        {
            samples_carry += count;
            while(samples_carry >= PCM_RATE / framerate)
            {
                UI.VidRender();

                const unsigned char *source = (const unsigned char *)&UI.PixelBuffer;
                std::size_t bytes_remain    = sizeof(UI.PixelBuffer);
                while(bytes_remain)
                {
                    int r = std::fwrite(source, 1, bytes_remain, fp);
                    if(r == 0) break;
                    bytes_remain -= r;
                    source       += r;
                }
                samples_carry -= PCM_RATE / framerate;
            }
        }
    }
    #endif

    #ifndef _WIN32
    AudioBuffer_lock.Unlock();
    #else
    if(!WritePCMfile)
        WindowsAudio::Write((const unsigned char *) &AudioBuffer[0], 2 * AudioBuffer.size());
    #endif
}
#endif /* not DJGPP */

/*
 * THIS CLASS USES !!!ADL PRIVATE!!!
 */

//class OPL3;
//class AdlInstrumentTester
//{
//    uint32_t cur_gm;
//    uint32_t ins_idx;
//    std::vector<uint32_t> adl_ins_list;
//    OPL3 *opl;
//    MIDIplay * play;

//public:
//    AdlInstrumentTester(ADL_MIDIPlayer *device)
//    {
//        cur_gm   = 0;
//        ins_idx  = 0;
//        play = reinterpret_cast<MIDIplay*>(device->adl_midiPlayer);
//        if(!play)
//            return;
//        opl = &play->opl;
//    }

//    ~AdlInstrumentTester()
//    {}

//    // Find list of adlib instruments that supposedly implement this GM
//    void FindAdlList()
//    {
//        const unsigned NumBanks = (unsigned)adl_getBanksCount();
//        std::set<unsigned> adl_ins_set;
//        for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
//            adl_ins_set.insert(banks[bankno][cur_gm]);
//        adl_ins_list.assign(adl_ins_set.begin(), adl_ins_set.end());
//        ins_idx = 0;
//        NextAdl(0);
//        opl->Silence();
//    }


//    void Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
//    {
//        if(opl->LogarithmicVolumes)// !!!ADL PRIVATE!!!
//            opl->Touch_Real(c, volume * 127 / (127 * 127 * 127) / 2);// !!!ADL PRIVATE!!!
//        else
//        {
//            // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
//            opl->Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log(volume) * 11.541561 + (0.5 - 104.22845)) : 0);// !!!ADL PRIVATE!!!
//            // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
//            //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
//        }
//    }

//    void DoNote(int note)
//    {
//        if(adl_ins_list.empty()) FindAdlList();
//        const unsigned meta = adl_ins_list[ins_idx];
//        const adlinsdata &ains = opl->GetAdlMetaIns(meta);// !!!ADL PRIVATE!!!

//        int tone = (cur_gm & 128) ? (cur_gm & 127) : (note + 50);
//        if(ains.tone)
//        {
//            /*if(ains.tone < 20)
//                tone += ains.tone;
//            else */
//            if(ains.tone < 128)
//                tone = ains.tone;
//            else
//                tone -= ains.tone - 128;
//        }
//        double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
//        int i[2] = { ains.adlno1, ains.adlno2 };
//        int32_t adlchannel[2] = { 0, 3 };
//        if(i[0] == i[1])
//        {
//            adlchannel[1] = -1;
//            adlchannel[0] = 6; // single-op
//            std::printf("noteon at %d(%d) for %g Hz\n",
//                        adlchannel[0], i[0], hertz);
//        }
//        else
//        {
//            std::printf("noteon at %d(%d) and %d(%d) for %g Hz\n",
//                        adlchannel[0], i[0], adlchannel[1], i[1], hertz);
//        }

//        opl->NoteOff(0);
//        opl->NoteOff(3);
//        opl->NoteOff(6);
//        for(unsigned c = 0; c < 2; ++c)
//        {
//            if(adlchannel[c] < 0) continue;
//            opl->Patch((uint16_t)adlchannel[c], (uint16_t)i[c]);
//            opl->Touch_Real((uint16_t)adlchannel[c], 127 * 127 * 100);
//            opl->Pan((uint16_t)adlchannel[c], 0x30);
//            opl->NoteOn((uint16_t)adlchannel[c], hertz);
//        }
//    }

//    void NextGM(int offset)
//    {
//        cur_gm = (cur_gm + 256 + (uint32_t)offset) & 0xFF;
//        FindAdlList();
//    }

//    void NextAdl(int offset)
//    {
//        if(adl_ins_list.empty()) FindAdlList();
//        const unsigned NumBanks = (unsigned)adl_getBanksCount();
//        ins_idx = (uint32_t)((int32_t)ins_idx + (int32_t)adl_ins_list.size() + offset) % adl_ins_list.size();

//        UI.Color(15);
//        std::fflush(stderr);
//        std::printf("SELECTED G%c%d\t%s\n",
//                    cur_gm < 128 ? 'M' : 'P', cur_gm < 128 ? cur_gm + 1 : cur_gm - 128,
//                    "<-> select GM, ^v select ins, qwe play note");
//        std::fflush(stdout);
//        UI.Color(7);
//        std::fflush(stderr);
//        for(unsigned a = 0; a < adl_ins_list.size(); ++a)
//        {
//            const unsigned i = adl_ins_list[a];
//            const adlinsdata &ains = opl->GetAdlMetaIns(i);

//            char ToneIndication[8] = "   ";
//            if(ains.tone)
//            {
//                /*if(ains.tone < 20)
//                    std::sprintf(ToneIndication, "+%-2d", ains.tone);
//                else*/
//                if(ains.tone < 128)
//                    std::sprintf(ToneIndication, "=%-2d", ains.tone);
//                else
//                    std::sprintf(ToneIndication, "-%-2d", ains.tone - 128);
//            }
//            std::printf("%s%s%s%u\t",
//                        ToneIndication,
//                        ains.adlno1 != ains.adlno2 ? "[2]" : "   ",
//                        (ins_idx == a) ? "->" : "\t",
//                        i
//                       );

//            for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
//                if(banks[bankno][cur_gm] == i)
//                    std::printf(" %u", bankno);

//            std::printf("\n");
//        }
//    }

//    bool HandleInputChar(char ch)
//    {
//        static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
//        //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
//        switch(ch)
//        {
//        case '/':
//        case 'H':
//        case 'A':
//            NextAdl(-1);
//            break;
//        case '*':
//        case 'P':
//        case 'B':
//            NextAdl(+1);
//            break;
//        case '-':
//        case 'K':
//        case 'D':
//            NextGM(-1);
//            break;
//        case '+':
//        case 'M':
//        case 'C':
//            NextGM(+1);
//            break;
//        case 3:
//            #if !((!defined(_WIN32) || defined(__CYGWIN__)) && !defined(__DJGPP__))
//        case 27:
//            #endif
//            return false;
//            break;
//        default:
//            const char *p = strchr(notes, ch);
//            if(p && *p)
//                DoNote((p - notes) - 12);
//        }
//        return true;
//    }
//};

static void TidyupAndExit(int sig)
{
    bool hookSignal = false;
    hookSignal |= (sig == SIGINT);
    #ifdef __DJGPP__
        hookSignal |= (sig == SIGQUIT);
    #endif
    if(hookSignal)
    {
        UI.ShowCursor();
        UI.Color(7);
        std::fflush(stderr);
        //signal(SIGINT, SIG_DFL);
        //raise(SIGINT);
        std::printf("\nBye!\n");
        QuitFlag = true;
    }
}

#ifdef _WIN32
/* Parse a command line buffer into arguments */
static void UnEscapeQuotes(char *arg)
{
    for(char *last = 0; *arg != '\0'; last = arg++)
        if(*arg == '"' && *last == '\\')
        {
            char *c_last = last;
            for(char *c_curr = arg; *c_curr; ++c_curr)
            {
                *c_last = *c_curr;
                c_last = c_curr;
            }
            *c_last = '\0';
        }
}
static int ParseCommandLine(char *cmdline, char **argv)
{
    char *bufp, *lastp = NULL;
    int argc = 0, last_argc = 0;
    for(bufp = cmdline; *bufp;)
    {
        /* Skip leading whitespace */
        while(std::isspace(*bufp)) ++bufp;
        /* Skip over argument */
        if(*bufp == '"')
        {
            ++bufp;
            if(*bufp)
            {
                if(argv) argv[argc] = bufp;
                ++argc;
            }
            /* Skip over word */
            while(*bufp && (*bufp != '"' || *lastp == '\\'))
            {
                lastp = bufp;
                ++bufp;
            }
        }
        else
        {
            if(*bufp)
            {
                if(argv) argv[argc] = bufp;
                ++argc;
            }
            /* Skip over word */
            while(*bufp && ! std::isspace(*bufp)) ++bufp;
        }
        if(*bufp)
        {
            if(argv) *bufp = '\0';
            ++bufp;
        }
        /* Strip out \ from \" sequences */
        if(argv && last_argc != argc) UnEscapeQuotes(argv[last_argc]);
        last_argc = argc;
    }
    if(argv) argv[argc] = 0;
    return(argc);
}

extern int main(int argc, char **argv);
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    //extern int main(int, char **);
    char *cmdline = GetCommandLine();
    int argc = ParseCommandLine(cmdline, NULL);
    char **argv = new char *[argc + 1];
    ParseCommandLine(cmdline, argv);
    return main(argc, argv);
}
#endif

static void adlEventHook(void *ui, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 /*channel*/, const ADL_UInt8 *data, size_t len)
{
    UserInterface *mUI = (UserInterface *)ui;

    if(type == 0xF7 || type  == 0xF0) // Ignore SysEx
    {
        mUI->PrintLn("SysEx %02X: %u bytes", type, (unsigned)len/*, data.c_str()*/);
        return;
    }

    if(type == 0xFF)
    {
        std::string eData((const char *)data, len);
        if(subtype >= 1 && subtype <= 6)
            mUI->PrintLn("Meta %d: %s", subtype, eData.c_str());
    }
}

static void adlNoteHook(void *userdata, int adlchn, int note, int ins, int pressure, double bend)
{
    UserInterface *mUI = (UserInterface *)userdata;
    mUI->IllustrateNote(adlchn, note, ins, pressure, bend);
}

static void adlDebugMsgHook(void *userdata, const char *fmt, ...)
{
    UserInterface *mUI = (UserInterface *)userdata;
    va_list ap;
    va_start(ap, fmt);
    /*int r = */ mUI->Print(2/*column*/, 8/*color*/, true/*line*/, fmt, ap);
    va_end(ap);
    //return r;
}

static bool is_number(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while(it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

int main(int argc, char **argv)
{
#ifndef HARDWARE_OPL3
    // How long is SDL buffer, in seconds?
    // The smaller the value, the more often AdlAudioCallBack()
    // is called.
    const double AudioBufferLength = 0.045;
    // How much do WE buffer, in seconds? The smaller the value,
    // the more prone to sound chopping we are.
    const double OurHeadRoomLength = 0.1;
    // The lag between visual content and audio content equals
    // the sum of these two buffers.
#endif

#ifndef _WIN32
    WritingToTTY = isatty(STDOUT_FILENO);
    if(WritingToTTY)
    {
        UI.Print(0, 15, true,
                 #ifdef __DJGPP__
                 "ADLMIDI_A: MIDI player for OPL3 hardware"
                 #else
                 "ADLMIDI: MIDI (etc.) player with OPL3 emulation"
                 #endif
                );
    }
#else
    WritingToTTY = true;
#endif
    if(WritingToTTY)
    {
        UI.Print(0, 3, true, "(C) -- http://iki.fi/bisqwit/source/adlmidi.html");
    }
    UI.Color(7);
    std::fflush(stderr);

    signal(SIGINT, TidyupAndExit);
#ifdef __DJGPP__
    signal(SIGQUIT, TidyupAndExit);
#endif

    if(argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        UI.Color(7);
        std::fflush(stderr);
        std::printf(
            "\n\n"
            "Usage: adlmidi <midifilename> [ <options> ] [ <banknumber> [ <numcards> [ <numfourops>] ] ]\n"
            "       adlmidi <midifilename> -1   To enter instrument tester\n"
            " -p Enables adlib percussion instrument mode (use with CMF files)\n"
            " -t Enables tremolo amplification mode\n"
            " -v Enables vibrato amplification mode\n"
            " -s Enables scaling of modulator volumes\n"
            " -frb Enables full-ranged CC74 XG Brightness controller\n"
            " -nl Quit without looping\n"
            " -nr Disables the reverb effect\n"
            " -w [<filename>] Write WAV file rather than playing\n"
            " -d [<filename>] Write video file using ffmpeg\n"
            #ifndef HARDWARE_OPL3
            " -fp Enables full-panning stereo support\n"
            " --emu-nuked  Uses Nuked OPL3 v 1.8 emulator\n"
            " --emu-nuked7 Uses Nuked OPL3 v 1.7.4 emulator\n"
            " --emu-dosbox Uses DosBox 0.74 OPL3 emulator\n"
            #endif
        );
        int banksCount = adl_getBanksCount();
        const char *const *bankNames = adl_getBankNames();
        for(int a = 0; a < banksCount; ++a)
            std::printf("%10s%2u = %s\n",
                        a ? "" : "Banks:",
                        a,
                        bankNames[a]);
        std::printf(
            "     Use banks 2-5 to play Descent \"q\" soundtracks.\n"
            "     Look up the relevant bank number from descent.sng.\n"
            "\n"
            "     You can pass the file path instead of bank number\n"
            "     to use a custom bank. (WOPL format is supported)\n"
            "\n"
            "     <numfourops> can be used to specify the number\n"
            "     of four-op channels to use. Each four-op channel eats\n"
            "     the room of two regular channels. Use as many as required.\n"
            "     The Doom & Hexen sets require one or two, while\n"
            "     Miles four-op set requires the maximum of numcards*6.\n"
            "\n"
            "     When playing Creative Music Files (CMF), try the\n"
            "     -p and -v options if it sounds wrong otherwise.\n"
            "\n"
        );
        return 0;
    }

#ifndef __DJGPP__
    std::srand((unsigned int)std::time(0));
#endif

    ADL_MIDIPlayer *myDevice;
    #ifndef __DJGPP__
    myDevice = adl_init(PCM_RATE);
    #else
    myDevice = adl_init(48000);
    #endif

    // Set hooks
    adl_setNoteHook(myDevice, adlNoteHook, (void *)&UI);
    adl_setRawEventHook(myDevice, adlEventHook, (void *)&UI);
    adl_setDebugMessageHook(myDevice, adlDebugMsgHook, (void *)&UI);

    int loopEnabled = 1;
#ifndef HARDWARE_OPL3
    int emulator = ADLMIDI_EMU_NUKED;
#endif

    while(argc > 2)
    {
        bool had_option = false;

        if(!std::strcmp("-p", argv[2]))
            adl_setPercMode(myDevice, 1);
        else if(!std::strcmp("-v", argv[2]))
            adl_setHVibrato(myDevice, 1);
        else if(!std::strcmp("-t", argv[2]))
            adl_setHTremolo(myDevice, 1);
        else if(!std::strcmp("-nl", argv[2]))
            loopEnabled = 0;
        else if(!std::strcmp("-frb", argv[2]))
            adl_setFullRangeBrightness(myDevice, 1);
#ifndef HARDWARE_OPL3
        else if(!std::strcmp("-fp", argv[2]))
            adl_setSoftPanEnabled(myDevice, 1);
        else if(!std::strcmp("--emu-nuked", argv[2]))
            emulator = ADLMIDI_EMU_NUKED;
        else if(!std::strcmp("--emu-nuked7", argv[2]))
            emulator = ADLMIDI_EMU_NUKED_174;
        else if(!std::strcmp("--emu-dosbox", argv[2]))
            emulator = ADLMIDI_EMU_DOSBOX;
#endif
        else if(!std::strcmp("-w", argv[2]))
        {
            loopEnabled = 0;
            WritePCMfile = true;
            if(argc > 3 && argv[3][0] != '\0' && (argv[3][0] != '-' || argv[3][1] == '\0'))
            {
                // Allow the option argument if
                // - it's not empty, and...
                // - it does not begin with "-" or it is "-"
                // - it is not a positive integer
                char *endptr = 0;
                if(std::strtol(argv[3], &endptr, 10) < 0 || (endptr && *endptr))
                {
                    PCMfilepath = argv[3];
                    had_option  = true;
                }
            }
        }
        #ifdef SUPPORT_VIDEO_OUTPUT
        else if(!std::strcmp("-d", argv[2]))
        {
            loopEnabled = 0;
            WriteVideoFile = true;
            if(argc > 3 && argv[3][0] != '\0' && (argv[3][0] != '-' || argv[3][1] == '\0'))
            {
                char *endptr = 0;
                if(std::strtol(argv[3], &endptr, 10) < 0 || (endptr && *endptr))
                {
                    VidFilepath = argv[3];
                    had_option  = true;
                }
            }
        }
        #endif
        else if(!std::strcmp("-s", argv[2]))
            adl_setScaleModulators(myDevice, 1);
        else if(!std::strcmp("-nr", argv[2]))
            ReverbIsOn = false;
        else break;

        std::copy(argv + (had_option ? 4 : 3), argv + argc,
                  argv + 2);
        argc -= (had_option ? 2 : 1);
    }

#ifndef HARDWARE_OPL3
    adl_switchEmulator(myDevice, emulator);
#endif
    adl_setLoopEnabled(myDevice, loopEnabled);

    #ifndef __DJGPP__

    #ifndef _WIN32
    static SDL_AudioSpec spec, obtained;
    spec.freq     = PCM_RATE;
    spec.format   = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples  = (Uint16)(spec.freq * AudioBufferLength);
    spec.callback = AdlAudioCallback;
    if(!WritePCMfile)
    {
        // Set up SDL
        if(SDL_OpenAudio(&spec, &obtained) < 0)
        {
            std::fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
            //return 1;
        }
        if(spec.samples != obtained.samples)
            std::fprintf(stderr, "Wanted (samples=%u,rate=%u,channels=%u); obtained (samples=%u,rate=%u,channels=%u)\n",
                         spec.samples,    spec.freq,    spec.channels,
                         obtained.samples, obtained.freq, obtained.channels);
    }
    #endif

    #endif /* not DJGPP */

    if(argc >= 3)
    {
        if(is_number(argv[2]) || !strcmp(argv[2], "-1"))
        {
            int bankno = std::atoi(argv[2]);
            if(bankno == -1)
            {
                bankno = 0;
                DoingInstrumentTesting = true;
            }
            else
            {
                if(WritingToTTY)
                    UI.PrintLn("FM instrument bank %u selected.", bankno);

                if(adl_setBank(myDevice, bankno) < 0)
                {
                    std::fprintf(stderr, "ERROR: %s\n", adl_errorInfo(myDevice));
                    UI.ShowCursor();
                    return 1;
                }
            }
        }
        else
        {
            //Open external bank file (WOPL format is supported)
            //to create or edit them, use OPL3 Bank Editor you can take here https://github.com/Wohlstand/OPL3BankEditor
            if(adl_openBankFile(myDevice, argv[2]) != 0)
            {
                std::fprintf(stdout, "FAILED: %s\n", adl_errorInfo(myDevice));
                UI.ShowCursor();
                return 1;
            }

            if(WritingToTTY)
                UI.PrintLn("FM instrument bank %s loaded.", argv[2]);
        }
    }

#if 0
    unsigned n_fourop[2] = {0, 0}, n_total[2] = {0, 0};
    for(unsigned a = 0; a < 256; ++a)
    {
        unsigned insno = banks[AdlBank][a];
        if(insno == 198) continue;
        ++n_total[a / 128];
        if(adlins[insno].adlno1 != adlins[insno].adlno2)
            ++n_fourop[a / 128];
    }
    if(WritingToTTY)
    {
        UI.PrintLn("This bank has %u/%u four-op melodic instruments", n_fourop[0], n_total[0]);
        UI.PrintLn("          and %u/%u percussive ones.", n_fourop[1], n_total[1]);
    }
#endif

    if(argc >= 4)
    {
        int numChips = std::atoi(argv[3]);
        if(adl_setNumChips(myDevice, (int)numChips) < 0)
        {
            std::fprintf(stderr, "ERROR: %s\n", adl_errorInfo(myDevice));
            UI.ShowCursor();
            return 0;
        }
    }

    if(argc >= 5)
    {
        int numFourOps = std::atoi(argv[4]);
        if(adl_setNumFourOpsChn(myDevice, (int)numFourOps) < 0)
        {
            std::fprintf(stderr, "ERROR: %s\n", adl_errorInfo(myDevice));
            UI.ShowCursor();
            return 0;
        }
    }
    /*
    else
        NumFourOps =
            DoingInstrumentTesting ? 2
            : (n_fourop[0] >= n_total[0] * 7 / 8) ? NumCards * 6
            : (n_fourop[0] < n_total[0] * 1 / 8) ? 0
            : (NumCards == 1 ? 1 : NumCards * 4);
    */
    if(WritingToTTY)
    {
        int numChips = adl_getNumChips(myDevice);
        int numFourOpChans = adl_getNumFourOpsChn(myDevice);
        UI.PrintLn("Simulating %u OPL3 cards for a total of %u operators.", numChips, numChips * 36);
        std::string s = "Operator set-up: "
                        + std::to_string(numFourOpChans)
                        + " 4op, "
                        + std::to_string((AdlPercussionMode ? 15 : 18) * numChips - numFourOpChans * 2)
                        + " 2op";
        if(AdlPercussionMode)
            s += ", " + std::to_string(numChips * 5) + " percussion";
        s += " channels";
        UI.PrintLn("%s", s.c_str());
    }

    UI.Color(7);
    if(adl_openFile(myDevice, argv[1]) != 0)
    {
        std::fprintf(stderr, "%s\n", adl_errorInfo(myDevice));
        UI.ShowCursor();
        return 2;
    }

    #ifdef __DJGPP__

    unsigned TimerPeriod = 0x1234DDul / NewTimerFreq;
    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, TimerPeriod & 0xFF);
    outportb(0x40, TimerPeriod >>   8);
    //enable();
    unsigned long BIOStimer_begin = BIOStimer;

    #else

    //const double maxdelay = MaxSamplesAtTime / (double)PCM_RATE;

    #ifdef _WIN32
    WindowsAudio::Open(PCM_RATE, 2, 16);
    #else
    SDL_PauseAudio(0);
    #endif

    #endif /* djgpp */

    AdlInstrumentTester InstrumentTester(myDevice);

    //static std::vector<int> sample_buf;
    #ifdef __DJGPP__
    double tick_delay = 0.0;
    #endif

    #ifndef __DJGPP__
    //sample_buf.resize(1024);
    short buff[1024];
    #endif

    UI.TetrisLaunched = true;
    while(!QuitFlag)
    {
#ifndef __DJGPP__
        //const double eat_delay = delay < maxdelay ? delay : maxdelay;
        //delay -= eat_delay;
        size_t got = 0;

        if(!DoingInstrumentTesting)
            got = (size_t)adl_play(myDevice, 1024, buff);
        else
            got = (size_t)adl_generate(myDevice, 1024, buff);
        if(got <= 0)
            break;
        /* Process it */
        SendStereoAudio(got / 2, buff);

        //static double carry = 0.0;
        //carry += PCM_RATE * eat_delay;
        //const unsigned long n_samples = (unsigned) carry;
        //carry -= n_samples;

        //if(SkipForward > 0)
        //    SkipForward -= 1;
        //else
        //{
        //    if(NumCards == 1)
        //    {
        //        player.opl.cards[0].Generate(0, SendStereoAudio, n_samples);
        //    }
        //    else if(n_samples > 0)
        //    {
        //        /* Mix together the audio from different cards */
        //        static std::vector<int> sample_buf;
        //        sample_buf.clear();
        //        sample_buf.resize(n_samples*2);
        //        struct Mix
        //        {
        //            static void AddStereoAudio(unsigned long count, int* samples)
        //            {
        //                for(unsigned long a=0; a<count*2; ++a)
        //                    sample_buf[a] += samples[a];
        //            }
        //        };
        //        for(unsigned card = 0; card < NumCards; ++card)
        //        {
        //            player.opl.cards[card].Generate(
        //                0,
        //                Mix::AddStereoAudio,
        //                n_samples);
        //        }
        //        /* Process it */
        //        SendStereoAudio(n_samples, &sample_buf[0]);
        //    }

        //fprintf(stderr, "Enter: %u (%.2f ms)\n", (unsigned)AudioBuffer.size(),
        //    AudioBuffer.size() * .5e3 / obtained.freq);
        #ifndef _WIN32
        const SDL_AudioSpec &spec_ = (WritePCMfile ? spec : obtained);
        for(unsigned grant = 0; AudioBuffer.size() > spec_.samples + (spec_.freq * 2) * OurHeadRoomLength; ++grant)
        {
            if(!WritePCMfile)
            {
                if(UI.CheckTetris() || grant % 4 == 0)
                {
                    SDL_Delay(1); // std::min(10.0, 1e3 * eat_delay) );
                }
            }
            else
            {
                for(unsigned n = 0; n < 128; ++n) UI.CheckTetris();
                AudioBuffer_lock.Lock();
                AudioBuffer.clear();
                AudioBuffer_lock.Unlock();
            }
        }
        #else
        //Sleep(1e3 * eat_delay);
        #endif
        //fprintf(stderr, "Exit: %u\n", (unsigned)AudioBuffer.size());
        //}
#else /* DJGPP */
        UI.IllustrateVolumes(0, 0);
        const double mindelay = 1.0 / NewTimerFreq;

        //__asm__ volatile("sti\nhlt");
        //usleep(10000);
        __dpmi_yield();

        static unsigned long PrevTimer = BIOStimer;
        const unsigned long CurTimer = BIOStimer;
        const double eat_delay = (CurTimer - PrevTimer) / (double)NewTimerFreq;
        PrevTimer = CurTimer;

        if(kbhit())
        {   // Quit on ESC key!
            int c = getch();
            if(c == 27)
                QuitFlag = true;
        }
#endif

        //double nextdelay =
        if(DoingInstrumentTesting)
        {
            if(!InstrumentTester.HandleInputChar(Input.PeekInput()))
                QuitFlag = true;
            #ifdef __DJGPP__
            else
                tick_delay = adl_tickEvents(myDevice, eat_delay, mindelay);
            #endif
        }
        #ifdef __DJGPP__
        else
        {
            tick_delay = adl_tickEvents(myDevice, eat_delay, mindelay);
            if(adl_atEnd(myDevice) && tick_delay <= 0)
                QuitFlag = true;
        }
        #endif
        //: player.Tick(eat_delay, mindelay);

        UI.GotoXY(0, 0);
        UI.ShowCursor();

        /*
         * TODO: Implement the public "tick()" function for the Hardware OPL3 chip support on DJGPP
         */

        //tick_delay = nextdelay;
    }

    //Shut up all sustaining notes
    adl_panic(myDevice);

    #ifdef __DJGPP__
    // Fix the skewed clock and reset BIOS tick rate
    _farpokel(_dos_ds, 0x46C, BIOStimer_begin +
              (BIOStimer - BIOStimer_begin)
              * (0x1234DD / 65536.0) / NewTimerFreq);
    //disable();
    outportb(0x43, 0x34);
    outportb(0x40, 0);
    outportb(0x40, 0);
    //enable();

    UI.GotoXY(0, 0);
    UI.ShowCursor();
    UI.Color(7);
    clrscr();
    #else
    #ifdef _WIN32
    WindowsAudio::Close();
    #else
    SDL_CloseAudio();
    #endif

    #endif /* djgpp */

    adl_close(myDevice);

    if(FakeDOSshell)
    {
        std::fprintf(stderr,
                    "Going TSR. Type 'EXIT' to return to ADLMIDI.\n"
                    "\n"
                    /*"Megasoft(R) Orifices 98\n"
                    "    (C)Copyright Megasoft Corp 1981 - 1999.\n"*/
                    "");
    }
    return 0;
}
