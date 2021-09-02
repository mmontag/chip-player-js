
#define Timer TimerRead()

#include "input.hpp"
#include "puzzlegame.hpp"

char ADLMIDI_PuzzleGame::peeked_input = 0;

unsigned long ADLMIDI_PuzzleGame::TimerRead()
{
    static std::chrono::time_point<std::chrono::system_clock> begin = std::chrono::system_clock::now();
    return (unsigned long)(519 * std::chrono::duration<double>(std::chrono::system_clock::now() - begin).count());
}

void ADLMIDI_PuzzleGame::Sound(unsigned, unsigned)
{
}

void ADLMIDI_PuzzleGame::ScreenPutString(const char *str, unsigned attr, unsigned column, unsigned row)
{
    for(; *str; ++column, ++str)
        PutCell(column, row, ((unsigned char)(*str & 0xFF)) | (attr << 8));
}

bool ADLMIDI_PuzzleGame::kbhit()
{
    if(peeked_input) return true;
    peeked_input = Input.PeekInput();
    return peeked_input != 0;
}

char ADLMIDI_PuzzleGame::getch()
{
    char r = peeked_input;
    peeked_input = 0;
    return r;
}

ADLMIDI_PuzzleGame::TetrisArea::TetrisArea(bool doDraw, unsigned x) : DoDraw(doDraw), RenderX(x) { }

ADLMIDI_PuzzleGame::TetrisArea::~TetrisArea()
{}

bool ADLMIDI_PuzzleGame::TetrisArea::Occupied(int x, int y) const
{
    return x < 1 || (x > Width - 2) || (y >= 0 && (Area[y][x] & Occ));
}

void ADLMIDI_PuzzleGame::TetrisArea::DrawBlock(unsigned x, unsigned y, int color)
{
    if(x < (unsigned)Width && y < (unsigned)Height) Area[y][x] = color;
    if(DoDraw) PutCell(x + RenderX, y, color);
}

void ADLMIDI_PuzzleGame::TetrisArea::DrawPiece(const ADLMIDI_PuzzleGame::Piece &piece, int color)
{
    (void)(piece > [&](int x, int y)->bool { this->DrawBlock(x, y, color); return false; });
}

bool ADLMIDI_PuzzleGame::TetrisArea::CollidePiece(const ADLMIDI_PuzzleGame::Piece &piece) const
{
    return piece > [&](int x, int y)
    {
        return this->Occupied(x, y);
    };
}

bool ADLMIDI_PuzzleGame::TetrisArea::CascadeEmpty(int FirstY)
{
    if(DoDraw)
    {
        ccrBegin(cascadescope);

        // Count full lines
        n_full = list_full = 0;
        for(int y = std::max(0, FirstY); y < Height - 1; ++y)
            if(TestFully(y, true))
            {
                ++n_full;
                list_full |= 1u << y;
            }
        if(n_full)
        {
            // Clear all full lines in Tengen Tetris style.
            for(animx = 1; animx < Width - 1; ++animx)
            {
                for(timer = Timer; Timer < timer + 6;)
                    ccrReturn(true);
                /*
                 * FIXME: Improve weird code fragment disliked by CLang
                 * /////////BEGIN////////////
                 */
                auto label = &
                    "  SINGLE  "
                    "  DOUBLE  "
                    "  TRIPLE  "
                    "  TETRIS  "
                    "QUADRUPLE "
                    "QUINTUPLE "
                    " SEXTUPLE "
                    " SEPTUPLE "
                    " OCTUPLE  "
                    [(n_full - 1) * 10];
                for(int y = FirstY; y < Height - 1; ++y)
                {
                    if(list_full & (1u << y))
                        DrawBlock(animx, y, label[(animx % 10)] + 0x100);
                }
                /*
                 * /////////END////////////
                 */

                if(DoDraw) Sound(10 + animx * n_full * 40, 2);
            }
            if(DoDraw) Sound(50, 15);
            // Cascade non-empty lines
            int target = Height - 2, y = Height - 2;
            for(; y >= 0; --y)
                if(!(list_full & (1u << y)))
                    DrawRow(target--, [&](unsigned x)
                {
                    return this->Area[y][x];
                });
            // Clear the top lines
            for(auto n = n_full; n-- > 0;)
                DrawRow(target--, [](unsigned)
            {
                return Empty;
            });
        }
        ccrFinish(false);
    }
    else
    {
        // Cascade non-empty lines
        int target = Height - 2, y = Height - 2;
        n_full = 0;
        for(int miny = std::max(0, FirstY); y >= miny; --y)
            if(TestFully(y, true))
            {
                ++n_full;
                miny = 0;
            }
            else
            {
                if(target != y)
                    memcpy(&Area[target], &Area[y], sizeof(Area[0][0])*Width);
                --target;
            }
        // Clear the top lines
        for(auto n = n_full; n-- > 0;)
            DrawRow(target--, [](unsigned)
        {
            return Empty;
        });
        return false;
    }
}


ADLMIDI_PuzzleGame::TetrisAIengine::TetrisAIengine(bool doReturns) :
    TetrisArea(false),
    DoReturns(doReturns)
{}

ADLMIDI_PuzzleGame::TetrisAIengine::~TetrisAIengine()
{}

void ADLMIDI_PuzzleGame::TetrisAIengine::AI_Signal(int signal)
{
    // any = piece moved; 0 = new piece, 2 = at ground
    switch(signal)
    {
    case 0: // new piece
        // do full scan and reset score
        confident = false;
        restart = true;
        resting = false;
        doubting = false;
        break;
    case 1: // piece moved
        // keep scanning (low prio), no resets
        resting = false;
        break;
    case 2: // now at ground
        // do full scan without resetting score
        resting = false;
        restart = true;
        doubting = true;
        break;
    }
}

int ADLMIDI_PuzzleGame::TetrisAIengine::AI_Run(const decltype(Area)&in_area, const ADLMIDI_PuzzleGame::Piece *seq)
{
    std::memcpy(Area, in_area, sizeof(Area));

    // For AI, we use Pierre Dellacherie's algorithm,
    // but extended for arbitrary ply lookahead.
    enum
    {
        landingHeightPenalty = -1, erodedPieceCellMetricFactor = 2,
        rowTransitionPenalty = -2, columnTransitionPenalty     = -2,
        buriedHolePenalty    = -8, wellPenalty                 = -2/*,
                occlusionPenalty     = 0*/
    };

    ccrBegin(aiscope);

    positions1.clear();
    positions2.clear();
    for(int x = -1; x < Width; ++x)
        for(unsigned rot = 0; rot < 4; ++rot)
            for(unsigned rot2 = 0; rot2 < 4; ++rot2)
                for(int x2 = -1; x2 <= 1; ++x2)
                {
                    positions1.push_back(position{int(rot), x, int(rot2), x2});
                    if(rot2 == rot && x2 == 0)
                        positions2.push_back(position{int(rot), x, int(rot2), x2});
                }

    confident = false;
    doubting  = false;
Restart:
    restart   = false;
    resting   = false;

    /*std::random_shuffle(positions1.begin(), positions1.end());*/
    std::random_shuffle(positions2.begin(), positions2.end());

    ply = 0;

Recursion:
    // Take current board as testing platform
    std::memcpy(data[ply].bkup, Area, sizeof(Area));
    if(!confident || ply > 0)
    {
        data[ply].best_score = { -999999, 0};
        if(ply == 0)
        {
            //int heap_room = 0;
            //while(TestFully(heap_room,false)) ++heap_room;
            ply_limit = 2;
        }
    }
    for(;;)
    {
        data[ply].pos_no = 0;
        do
        {
            if(DoReturns)
            {
                ccrReturn(0);
                if(restart) goto Restart;
            }
            if(ply > 0)
            {
                // Game might have changed the the board contents.
                std::memcpy(data[0].bkup, in_area, sizeof(Area));

                // Now go on and test with the current testing platform
                std::memcpy(Area, data[ply].bkup, sizeof(Area));
            }

            // Fix the piece in place, cascade rows, and analyze the result.
            {
                const position &goal = (/*ply==0&&!doubting ? positions1 :*/ positions2)
                                       [ data[ply].pos_no ];
                Piece n = seq[ply];
                n.x = goal.x;
                n.r = goal.rot;
                if(ply) n.y = 0;

                // If we are analyzing a mid-fall maneuver, verify whether
                // the piece can be actually maneuvered into this position.
                //if(ply==0 && n.y >= 0)
                for(Piece q, t = *seq; t.x != n.x && t.r != n.r;)
                    if((t.r == n.r || (q = t, ++t.r, CollidePiece(t) && (t = q, true)))
                       && (t.x <= n.x || (q = t, --t.x, CollidePiece(t) && (t = q, true)))
                       && (t.x >= n.x || (q = t, ++t.x, CollidePiece(t) && (t = q, true))))
                        goto next_move; // no method of maneuvering.

                // Land the piece if it's not already landed
                do ++n.y;
                while(!CollidePiece(n));
                --n.y;
                if(n.y < 0 || CollidePiece(n)) goto next_move; // cannot place piece?

                /*
                    // Rotate to ground-rotation
                    if(n.r != goal.rot2 || goal.x2 != 0)
                    {
                        while(n.r != goal.rot2)
                        {
                            ++n.r;
                            if(CollidePiece(n)) goto next_move;
                        }
                        if(goal.x2 != 0)
                        {
                            do n.x += goal.x2; while(!CollidePiece(n));
                            n.x -= goal.x2;
                        }

                        do ++n.y; while(!CollidePiece(n)); --n.y;
                        if(n.y < 0 || CollidePiece(n)) goto next_move; // cannot place piece?
                    }
                  */

                DrawPiece(n, Occ); // place piece

                // Find out the extents of this piece, and how many
                // cells of the piece contribute into full (completed) rows.
                signed char full[4] = {-1, -1, -1, -1};
                int miny = n.y + 9, maxy = n.y - 9, minx = n.x + 9, maxx = n.x - 9, num_eroded = 0;
                (void)(n > [&](int x, int y) -> bool
                {
                    if(x < minx)
                        minx = x;
                    if(x > maxx)
                        maxx = x;
                    if(y < miny)
                        miny = y;
                    if(y > maxy)
                        maxy = y;
                    if(full[y - n.y] < 0) full[y - n.y] = this->TestFully(y, true);
                    num_eroded += full[y - n.y];
                    return false;
                });

                CascadeEmpty(n.y);

                // Analyze the board and assign penalties
                int penalties = 0;
                for(int y = 0; y < Height - 1; ++y)
                    for(int q = 1, r, x = 1; x < Width; ++x, q = r)
                        if(q != (r = Occupied(x, y)))
                            penalties += rowTransitionPenalty;
                for(int x = 1; x < Width - 1; ++x)
                    for(int ceil = 0/*,heap=0*/, q = 0, r, y = 0; y < Height; ++y, q = r)
                    {
                        if(q != (r = Occupied(x, y)))
                        {
                            penalties += columnTransitionPenalty;
                            /*if(!r) { penalties += heap * occlusionPenalty; heap = 0; }*/
                        }
                        if(r)
                        {
                            ceil = 1; /*++heap;*/ continue;
                        }
                        if(ceil) penalties += buriedHolePenalty;
                        if(Occupied(x - 1, y) && Occupied(x + 1, y))
                            for(int y2 = y; y2 < Height - 1 && !Occupied(x, y2); ++y2)
                                penalties += wellPenalty;
                    }

                data[ply].base_score =
                {
                    // score
                    (erodedPieceCellMetricFactor * int(n_full) * num_eroded +
                    penalties +
                    landingHeightPenalty * ((Height - 1) * 2 - (miny + maxy))) * 16,
                    // tie-breaker
                    50 * std::abs(Width - 2 - minx - maxx) +
                    (minx + maxx < Width - 2 ? 10 : 0) - n.r
                    - 400 * (goal.rot != goal.rot2)
                    - 800 * (goal.x2  != 0)
                };
            }
            if(ply + 1 < ply_limit)
            {
                ++ply;
                goto Recursion;
Unrecursion:
                --ply;
            }

            /*fprintf(stdout, "ply %u: [%u]%u,%u gets %d,%d\n",
                        ply,
                        data[ply].pos_no,
                        positions[data[ply].pos_no].x,
                        positions[data[ply].pos_no].rot,
                        data[ply].base_score.first,
                        data[ply].base_score.second);*/

            if(data[ply].best_score < data[ply].base_score)
            {
                data[ply].best_score  = data[ply].base_score;
                data[ply].best_pos    = (/*ply==0&&!doubting ? positions1 :*/ positions2) [ data[ply].pos_no ];
            }
next_move:
            ;
        }
        while(++data[ply].pos_no < (/*ply==0&&!doubting ? positions1 :*/ positions2).size());

        if(ply > 0)
        {
            int v = data[ply].best_score.first;
            v /= 2;
            //if(ply_limit == 4) v /= 2; else v *= 2;
            data[ply - 1].base_score.first += v; // >> (2-ply);
            goto Unrecursion;
            /*
                    parent += child / 2 :
                    Game 0x7ffff0d94ce0 over with score=91384,lines=92
                    Game 0x7ffff0d96a40 over with score=153256,lines=114
                    parent += child     :
                    Game 0x7fff4a4eb8a0 over with score=83250,lines=86
                    Game 0x7fff4a4ed600 over with score=295362,lines=166
                    parent += child * 2 :
                    Game 0x7fff000a2e00 over with score=182306,lines=131
                    Game 0x7fff000a10a0 over with score=383968,lines=193
                    parent += child * 4 :
                    Game 0x7fff267867b0 over with score=62536,lines=75
                    Game 0x7fff26788510 over with score=156352,lines=114
                    */
        }
        // all scanned; unless piece placement changes we're as good as we can be.
        confident = true;
        resting   = true;
        doubting  = false;
        while(resting) ccrReturn(0);
        if(restart) goto Restart;
    } // infinite refining loop
    ccrFinish(0);
}

ADLMIDI_PuzzleGame::Tetris::Tetris(unsigned rx) : TetrisArea(true, rx), seq(), hiscore(0), hudtimer(0) {}

ADLMIDI_PuzzleGame::Tetris::~Tetris() { }

int ADLMIDI_PuzzleGame::Tetris::GameLoop()
{
    Piece &cur = *seq;
    ccrBegin(loopscope);

    // Initialize area
    for(auto y = Height; y-- > 0;)
        for(auto x = Width; x-- > 0;)
            DrawBlock(x, y, (x > 0 && x < Width - 1 && y < Height - 1) ? Empty : Border);

    score = lines = combo = incoming = pieces = 0;
    MakeNext();
    MakeNext();
    first = true;

    for(escaped = false; !escaped;)  // Main loop
    {
        // Generate new piece
        MakeNext();
        /*if(first) // Use if making 4 pieces
        {
            first=false;
            ccrReturn(0);
            MakeNext();
        }*/

        dropping = false;
        atground = false;
re_collide:
        timer    = Timer;
        AI_Signal(0); // signal changed board configuration

        // Gameover if cannot spawn piece
        if(CollidePiece(cur))
            break;

        ccrReturn(0);

        while(!escaped)
        {
            atground = CollidePiece(cur * [](Piece & p)
            {
                ++p.y;
            });
            if(atground) dropping = false;
            // If we're about to hit the floor, give the AI a chance for sliding.
            AI_Signal(atground ? 2 : 1);

            DrawPiece(cur, cur.color);

            // Wait for input
            for(ticked = false; ;)
            {
                AI_Run();
                HUD_Run();
                ccrReturn(0);

                if(incoming)
                {
                    // Receive some lines of garbage from opponent
                    DrawPiece(cur, Empty);
                    for(int threshold = Height - 1 - incoming, y = 0; y < Height - 1; ++y)
                    {
                        unsigned mask = 0x1EF7BDEF >> (rand() % 10);
                        DrawRow(y, [&](unsigned x)
                        {
                            return
                                y < threshold
                                ? Area[y + incoming][x]
                                : (mask & (1 << x)) ? Garbage : Empty;
                        });
                    }
                    // The new rows may push the piece up a bit. Allow that.
                    for(; incoming-- > 0 && CollidePiece(cur); --cur.y) {}
                    incoming = 0;
                    goto re_collide;
                }

                ticked = Timer >= timer + std::max(atground ? 40 : 10, int(((17 - Level()) * 8)));
                if(ticked || MyKbHit() || dropping) break;
            }

            Piece n = cur;
            if(MyKbHit()) dropping = false;

            switch(ticked ? 's' : (MyKbHit() ? MyGetCh() : (dropping ? 's' : 0)))
            {
            case 'H'<<8:
            case 'w':
                ++n.r; /*Sound(120, 1);*/ break;
            case 'P'<<8:
            case 's':
                ++n.y;
                break;
            case 'K'<<8:
            case 'a':
                --n.x; /*Sound(120, 1);*/ break;
            case 'M'<<8:
            case 'd':
                ++n.x; /*Sound(120, 1);*/ break;
            case 'q':
            case '\033': /*escaped = true;*/
                break;
            case ' ':
                dropping = true; /*fallthrough*/
            default:
                continue;
            }
            if(n.x != cur.x) kicked = false;

            if(CollidePiece(n))
            {
                if(n.y == cur.y + 1) break; // fix piece if collide against ground
                // If tried rotating, and was unsuccessful, try wall kicks
                if(n.r != cur.r && !CollidePiece(n*[](Piece & p)
            {
                ++p.x;
            }))
                {
                    kicked = true;
                    ++n.x;
                }
                else if(n.r != cur.r && !CollidePiece(n*[](Piece & p)
            {
                --p.x;
            }))
                {
                    kicked = true;
                    --n.x;
                }
                else
                    continue; // no move
            }
            DrawPiece(cur, Empty);
            if(n.y > cur.y) timer = Timer; // Reset autodrop timer
            cur = n;
        }

        // If the piece cannot be moved sideways or up from its final position,
        // determine that it must have been spin-fixed into its place.
        // It is a bonus-worthy accomplishment if it ends up clearing lines.
        spinned = CollidePiece(cur * [](Piece & p)
        {
            --p.y;
        })
        &&CollidePiece(cur*[](Piece & p)
        {
            --p.x;
        })
        &&CollidePiece(cur*[](Piece & p)
        {
            ++p.x;
        });
        DrawPiece(cur, cur.color | Occ);
        Sound(50, 30);

        while(CascadeEmpty(cur.y)) ccrReturn(0);
        if(n_full > 1) ccrReturn(n_full - 1); // Send these rows to opponent

        pieces += 1;
        lines += n_full;
        static const unsigned clr[] = {0, 1, 3, 5, 8, 13, 21, 34, 45};
        int multiplier = Level(), clears = clr[n_full];
        int bonus = (clears * 100 + (cur.y * 50 / Height));
        int extra = 0;
        if(spinned) extra = ((clears + 1) * ((kicked ? 3 : 4) / 2) - clears) * 100;
        combo = n_full ? combo + n_full : 0;
        int comboscore = combo > n_full ? combo * 50 * multiplier : 0;
        bonus *= multiplier;
        extra *= multiplier;
        score += bonus + extra + comboscore;
        HUD_Add(bonus, extra, comboscore);
        //if(n_full) std::fprintf(stdout, "Game %p += %u lines -> score=%u,lines=%u\n", this, n_full, score,lines);
    }
    //std::fprintf(stdout, "Game %p over with score=%u,lines=%u\n", this, score,lines);
    //over_lines += lines;

    if(score > hiscore) hiscore = score;
    HudPrint(7, 4, "", "%-7u", hiscore);

    ccrFinish(-1);
}

int ADLMIDI_PuzzleGame::Tetris::Level() const
{
    return 1 + lines / 10;
}

void ADLMIDI_PuzzleGame::Tetris::MakeNext()
{
    const int which = 2; // Index within seq[] to populate
    static const Piece b[] =
    {
        { { { 0x04040404, 0x00000F00, 0x04040404, 0x00000F00 } }, 0, 0, 0xBDB, 0 }, // I
        { { { 0x0000080E, 0x000C0808, 0x00000E02, 0x00020206 } }, 0, 0, 0x3DB, 0 }, // J
        { { { 0x0000020E, 0x0008080C, 0x00000E08, 0x00060202 } }, 0, 0, 0x6DB, 0 }, // L
        { { { 0x00000606, 0x00000606, 0x00000606, 0x00000606 } }, 0, 0, 0xEDB, 0 }, // O
        { { { 0x00080C04, 0x0000060C, 0x00080C04, 0x0000060C } }, 0, 0, 0xADB, 0 }, // S
        { { { 0x00000E04, 0x00040C04, 0x00040E00, 0x00040604 } }, 0, 0, 0x5DB, 0 }, // T
        { { { 0x00020604, 0x00000C06, 0x00020604, 0x00000C06 } }, 0, 0, 0x4DB, 0 }, // Z
        // Add some pentaminos to create a challenge worth the AI:
        { { { 0x00020702, 0x00020702, 0x00020702, 0x00020702 } }, 0, 0, 0x2DB, 0 }, // +
        { { { 0x000E0404, 0x00020E02, 0x0004040E, 0x00080E08 } }, 0, 0, 0x9DB, 0 }, // T5
        { { { 0x00000A0E, 0x000C080C, 0x00000E0A, 0x00060206 } }, 0, 0, 0x3DB, 0 }, // C
        { { { 0x00060604, 0x000E0600, 0x02060600, 0x00060700 } }, 0, 0, 0x7DB, 0 }, // P
        { { { 0x00060602, 0x00070600, 0x04060600, 0x00060E00 } }, 0, 0, 0xFDB, 0 }, // Q
        { { { 0x04040404, 0x00000F00, 0x04040404, 0x00000F00 } }, 0, 0, 0xBDB, 0 }, // I
        { { { 0x00040702, 0x00030602, 0x00020701, 0x00020306 } }, 0, 0, 0xDDB, 0 }, // R
        { { { 0x00010702, 0x00020603, 0x00020704, 0x00060302 } }, 0, 0, 0x8DB, 0 }, // F
        // I is included twice, otherwise it's just a bit too unlikely.
    };
    int c = Empty;
    auto fx = [this]()
    {
        seq[0].x = 1;
        seq[0].y = -1;
        seq[1].x = Width + 1;
        seq[1].y = SHeight - 8;
        seq[2].x = Width + 5;
        seq[2].y = SHeight - 8;
        /*seq[3].x=Width+9; seq[3].y=SHeight-8;
                             seq[4].x=Width+1; seq[4].y=SHeight-4;
                             seq[5].x=Width+5; seq[5].y=SHeight-4;
                             seq[6].x=Width+9; seq[6].y=SHeight-4;*/
    };
    auto db = [&](int x, int y)
    {
        PutCell(x + RenderX, y, c);
        return false;
    };
    fx();
    (void)(seq[1] > db);
    seq[0] = seq[1];
    (void)(seq[2] > db);
    seq[1] = seq[2];
    /*seq[3]>db; seq[2] = seq[3];
        seq[4]>db; seq[3] = seq[4];
        seq[5]>db; seq[4] = seq[5];
        seq[6]>db; seq[5] = seq[6];*/
    fx();
    const unsigned npieces = sizeof(b) / sizeof(*b);
    unsigned rnd = (unsigned)((std::rand() / double(RAND_MAX)) * (4 * npieces));
    #ifndef EASY
    /*if(pieces > 3)
        {
            int heap_room = 0;
            while(TestFully(heap_room,false)) ++heap_room;
            bool cheat_good = heap_room <= (7 + lines/20);
            bool cheat_evil = heap_room >= 18;
            if(heap_room >= 16 && (pieces % 5) == 0) cheat_good = false;
            if( (pieces % 11) == 0) cheat_good = true;
            if(cheat_good) cheat_evil = false;
            if(cheat_good || cheat_evil)
            {
                // Don't tell anyone, but in desperate situations,
                // we let AI judge what's best for upcoming pieces,
                // in order to prolong the game!
                // So, the AI cheats, but it is an equal benefactor.
                // EXCEPTION: if there is an abundance of space,
                // bring out the _worst_ possible pieces!
                TetrisAIengine<>::scoring best_score;
                unsigned best_choice = ~0u;
                for(unsigned test=0; test<npieces; ++test)
                {
                    unsigned choice = (test + rnd) % npieces;
                    if(choice == 0) continue; // Ignore the duplicate I (saves time)
                    if(cheat_evil && choice == 7) continue; // Don't give +, it's too evil

                    seq[which]   = b[ choice ];
                    seq[which].r = 0;

                    TetrisAIengine<false> chooser;
                    chooser.AI_Signal(0);
                    chooser.AI_Run(Area, seq);
                    auto& s = chooser.data[0].best_score;
                    if(best_choice == ~0u
                    || (cheat_evil ? s < best_score : s > best_score)
                      )
                        { best_score = s; best_choice = choice; }
                }
                rnd = best_choice * 4 + (rnd % 4);
            }
        }*/
    #endif
    seq[which] = b[rnd / 4];
    seq[which].r = rnd % 4;
    fx();
    c = seq[1].color;
    (void)(seq[1] > db);
    c = (seq[2].color & 0xF00) + 176;
    (void)(seq[2] > db);
    /*c=(seq[3].color&0xF00) + 176; seq[3]>db;
        c=(seq[4].color&0xF00) + 176; seq[4]>db;
        c=(seq[5].color&0xF00) + 176; seq[5]>db;
        c=(seq[6].color&0xF00) + 176; seq[6]>db;*/
    HudPrint(0,  SHeight - 9, "NEXT:", "", 0);
    HudPrint(12, 0, "SCORE:", "%-7u", score);
    HudPrint(12, 2, "LINES:", "%-5u", lines);
    HudPrint(12, 4, "LEVEL:", "%-3u", Level());
}

void ADLMIDI_PuzzleGame::Tetris::HudPrint(int c, int y, const char *a, const char *b, int v) const
{
    char Buf[64];
    snprintf(Buf, sizeof Buf, b, v);
    ScreenPutString(a,   15, RenderX + Width + 2, y);
    ScreenPutString(Buf, c,  RenderX + Width + 2, y + 1);
}

void ADLMIDI_PuzzleGame::Tetris::HUD_Run()
{
    if(!hudtimer || Timer < hudtimer) return;
    HUD_Add(0, 0, 0);
    hudtimer = 0;
}

void ADLMIDI_PuzzleGame::Tetris::HUD_Add(int bonus, int extra, int combo)
{
    hudtimer = Timer + 180;
    static const char blank[] = "      ";
    HudPrint(10,  6, bonus ? blank  : blank, bonus ? "%+-6d" : blank, bonus);
    HudPrint(10,  8, combo ? "Combo" : blank, combo ? "%+-6d" : blank, combo);
    HudPrint(13, 10, extra ? "Skill" : blank, extra ? "%+-6d" : blank, extra);
}

void ADLMIDI_PuzzleGame::Tetris::AI_Signal(int) { }

int ADLMIDI_PuzzleGame::Tetris::AI_Run()
{
    return 0;
}

char ADLMIDI_PuzzleGame::Tetris::DelayOpinion() const
{
    return 0;
}




ADLMIDI_PuzzleGame::TetrisHuman::TetrisHuman(unsigned rx) : Tetris(rx) { }

int ADLMIDI_PuzzleGame::TetrisHuman::MyKbHit()
{
    return kbhit();
}

int ADLMIDI_PuzzleGame::TetrisHuman::MyGetCh()
{
    int c;
    return (c = getch()) ? c : (getch() << 8);
}



void ADLMIDI_PuzzleGame::TetrisAI::AI_Signal(int s)
{
    TetrisAIengine::AI_Signal(s);
}

int ADLMIDI_PuzzleGame::TetrisAI::AI_Run()
{
    #ifdef __DJGPP__
    char buf1[16]/*, buf2[16], buf3[16]*/;
    sprintf(buf1, "COM%u%c%c%c%c", ply_limit,
            resting   ? 'r' : '_',
            confident ? 'C' : '_',
            doubting  ? 'D' : '_',
            atground  ? 'G' : '_'
           );
    ScreenPutString(buf1, 0x70, Tetris::RenderX + Width, 0);
    /*
            sprintf(buf2, "%-8d",   data[0].best_score.first);
            sprintf(buf3, "%2d,%d", data[0].best_pos.x,
                                    data[0].best_pos.rot);
            ScreenPutString(buf2, 0x08, Tetris::RenderX+Width, 1);
            ScreenPutString(buf3, 0x08, Tetris::RenderX+Width, 4);
            */
    #endif
    return TetrisAIengine::AI_Run(Tetris::Area, seq);
}

int ADLMIDI_PuzzleGame::TetrisAI::MyKbHit()
{
    return PendingAIinput ? 1 : (PendingAIinput = GenerateAIinput()) != 0;
}

int ADLMIDI_PuzzleGame::TetrisAI::MyGetCh()
{
    int r = PendingAIinput;
    return r ? static_cast<void>((PendingAIinput = 0)), r : GenerateAIinput();
}

char ADLMIDI_PuzzleGame::TetrisAI::d(char c, int maxdelay)
{
    if(Timer >= intimer+maxdelay) { intimer=Timer; return c; }
    return 0;
}

int ADLMIDI_PuzzleGame::TetrisAI::GenerateAIinput()
{
    /*if(TetrisAIengine::atground)
            {
                if(seq->r != data[0].best_pos.rot2) return d('w',1);
                if(data[0].best_pos.x2 < 0) return d('a', 1);
                if(data[0].best_pos.x2 > 0) return d('d', 1);
            }
            else*/
    {
        if(seq->r != data[0].best_pos.rot) return d('w',1);
        if(seq->y >= 0 && seq->x > data[0].best_pos.x) return d('a', 2);
        if(seq->y >= 0 && seq->x < data[0].best_pos.x) return d('d', 2);
    }
    if(doubting || !confident) return 0;
    return d('s',3);
}

char ADLMIDI_PuzzleGame::TetrisAI::DelayOpinion() const
{ if(doubting || restart || !confident) return -1;
    return Tetris::DelayOpinion(); }

ADLMIDI_PuzzleGame::TetrisAI::TetrisAI(unsigned rx) : Tetris(rx), TetrisAIengine(true), PendingAIinput(0), delay(0), intimer(0) {}

ADLMIDI_PuzzleGame::TetrisAI::~TetrisAI()
{}

