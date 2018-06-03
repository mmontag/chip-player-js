/* A block puzzle game.
 * Most of the source code comes from https://www.youtube.com/watch?v=V65mtR08fH0
 * Copyright (c) 2012 Joel Yliluoma (http://iki.fi/bisqwit/)
 * License: MIT
 */

#ifndef TETRIS_PUZZLEGAME
#define TETRIS_PUZZLEGAME

// Standard C++ includes:
#include <cstdio>    // for std::puts
#include <cstdlib>   // for std::rand
#include <cstring>   // for std::memcpy
#include <algorithm> // for std::abs, std::max, std::random_shuffle
#include <utility>   // for std::pair
#include <vector>    // for std::vector
#include <cstdint>
#include <chrono>

// Coroutine library. With thanks to Simon Tatham.
#define ccrVars      struct ccrR { int p; ccrR() : p(0) { } } ccrL
#define ccrBegin(x)  auto& ccrC=(x); switch(ccrC.ccrL.p) { case 0:;
#define ccrReturn(z) do { ccrC.ccrL.p=__LINE__; return(z); case __LINE__:; } while(0)
#define ccrFinish(z) } ccrC.ccrL.p=0; return(z)

namespace ADLMIDI_PuzzleGame
{
    // Field & rendering definitions
    const auto Width = 18, Height = 25; // Width and height, including borders.
    const auto SHeight = 12+9;

    const auto Occ = 0x10000u;
    const auto Empty = 0x12Eu, Border = Occ+0x7DBu, Garbage = Occ+0x6DBu;

    unsigned long TimerRead();
    void Sound(unsigned/*freq*/, unsigned/*duration*/);
    void PutCell(int x, int y, unsigned cell);
    void ScreenPutString(const char* str, unsigned attr, unsigned column, unsigned row);
    extern char peeked_input;
    bool kbhit();
    char getch();

    // Game engine
    using uint64_t = std::uint_fast64_t;
    struct Piece
    {
        struct { uint64_t s[4]; } shape;
        int x:8, y:8, color:14;
        unsigned r:2;

        template<typename T> // walkthrough operator
        inline bool operator>(T it) const
        {
            uint64_t p = 1ull, q = shape.s[r];
            for(int by=0; by<8; ++by)
            {
                for(int bx=0; bx<8; ++bx)
                {
                    if((q & p) && it(x+bx, y+by)) return true;
                    //p >>= 1ull;
                    p <<= 1ull;
                }
            }
            return false;
        }
        template<typename T> // transmogrify operator
        inline Piece operator*(T it) const { Piece res(*this); it(res); return res; }
    };

    //template<bool DoDraw>
    struct TetrisArea
    {
        bool DoDraw;
        int Area[Height][Width];
        unsigned RenderX;
        unsigned n_full, list_full, animx;
        unsigned long timer;
        struct { ccrVars; } cascadescope;
    public:
        TetrisArea(bool doDraw, unsigned x=0);
        virtual ~TetrisArea();
        bool Occupied(int x,int y) const;

        template<typename T>
        inline void DrawRow(unsigned y, T get)
        {
            for(int x=1; x<Width-1; ++x) DrawBlock(x,y, get(x));
        }
        inline bool TestFully(unsigned y, bool state) const
        {
            for(int x=1; x<Width-1; ++x) if(state != !!(Area[y][x]&Occ)) return false;
            return true;
        }
        void DrawBlock(unsigned x,unsigned y, int color);
        void DrawPiece(const Piece& piece, int color);
        bool CollidePiece(const Piece& piece) const;
        bool CascadeEmpty(int FirstY);
    protected:
        virtual int MyKbHit() = 0;
        virtual int MyGetCh() = 0;
    };

    //template<bool DoReturns = true>
    class TetrisAIengine: TetrisArea /*<false>*/
    {
        bool DoReturns;
    public://protected:
        TetrisAIengine(bool doReturns = true);
        virtual ~TetrisAIengine();

        typedef std::pair<int/*score*/, int/*prio*/> scoring;
        struct position
        {
            // 1. Rotate to this position
            int rot;
            // 2. Move to this column
            int x;
            // 3. Drop to ground
            // 4. Rotate to this position
            int rot2;
            // 5. Move maximally to this direction
            int x2;
            // 6. Drop to ground again
        };

        std::vector<position> positions1, positions2;

        struct Recursion
        {
            // Copy of the field before this recursion level
            decltype(Area) bkup;
            // Currently testing position
            unsigned pos_no;
            // Best outcome so far from this recursion level
            scoring  best_score, base_score;
            position best_pos;
            Recursion() : best_pos( {1,5, 1,0} ) { }
        } data[8];
        unsigned ply, ply_limit;

        bool restart;   // Begin a new set of calculations
        bool confident; // false = Reset best-score
        bool resting;   // true  = No calculations to do
        bool doubting;
        struct { ccrVars; } aiscope;

    public:
        void AI_Signal(int signal);
        int AI_Run(const decltype(Area)& in_area, const Piece* seq);
    };

    class Tetris: protected TetrisArea /*<true>*/
    {
    public:
        Tetris(unsigned rx);
        virtual ~Tetris();

    protected:
        // These variables should be local to GameLoop(),
        // but because of coroutines, they must be stored
        // in a persistent wrapper instead. Such persistent
        // wrapper is provided by the game object itself.
        Piece seq[4];
        unsigned hiscore, score, lines, combo, pieces;
        unsigned long hudtimer;
        bool escaped, dropping, ticked, kicked, spinned, atground, first;

        struct { ccrVars; } loopscope;
    public:
        unsigned incoming;

        int GameLoop();

    protected:
        int Level() const;

        void MakeNext();

        void HudPrint(int c, int y,const char*a,const char*b,int v=0) const;

        void HUD_Run();
        void HUD_Add(int bonus, int extra, int combo);

        virtual void AI_Signal(int);
        virtual int AI_Run();
        virtual int MyKbHit() = 0;
        virtual int MyGetCh() = 0;
    public:
        // Return -1 = don't want sleep, +1 = want sleep, 0 = don't mind
        virtual char DelayOpinion() const;//dropping ? -1 : 0; }
    };

    class TetrisHuman: public Tetris
    {
    public:
        TetrisHuman(unsigned rx);
    protected:
        virtual int MyKbHit();
        virtual int MyGetCh();
    };

    class TetrisAI: public Tetris, TetrisAIengine /*<true>*/
    {
    protected:
        virtual void AI_Signal(int s);
        virtual int AI_Run();
        virtual int MyKbHit();
        virtual int MyGetCh();
        char d(char c, int maxdelay);
        int GenerateAIinput();

        int PendingAIinput, delay;
        unsigned long intimer;
    public:
        virtual char DelayOpinion() const;

        TetrisAI(unsigned rx);
        ~TetrisAI();
    };
}

#endif //TETRIS_PUZZLEGAME
