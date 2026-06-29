/* de:meta
{
  "title": "Chess",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "finite-state-ai",
    "title-play-gameover-loop"
  ],
  "lineage": "Full legal chess (castling, en passant, promotion) with a depth-3 negamax CPU opponent; no chess cart existed before — the move-gen + make/unmake + alpha-beta loop is the conceptual contribution.",
  "genre": "tabletop",
  "description": "A complete, legal game of chess on one crisp 8x8 board — every piece move, captures, castling, en passant, pawn promotion, and full check / checkmate / stalemate detection, on a primitive-drawn board with readable pixel-art piece sprites (authored in code via sprite-draw.js). Play two-player hotseat or face a depth-3 negamax CPU that weighs material and central control, picking either color. Pick up a piece and its legal squares light up (dots for moves, rings for captures), the last move leaves a trail, a checked king flashes red, and checkmate lands with a fanfare and a screen shake; a tiny win tally persists between sessions. Controls: click your piece to select (legal targets highlight), click a target square to move, click the piece again to deselect; on the title screen click HOTSEAT or VS CPU (white/black); in game R starts a new game and M or ESC returns to the menu."
}
de:meta */
#include "studio.h"
#include <stddef.h>
#include <string.h>

// ── CHESS — full legal chess on an 8x8 board ─────────────────────────────────
// Two-player hotseat or vs a depth-3 negamax CPU. All piece moves, captures,
// castling, en passant, pawn promotion (auto-queen), and check / checkmate /
// stalemate detection. Pieces + board are drawn entirely from primitives.
//
// Mouse: click your piece (legal targets light up), click a target to move.
// Title: click HOTSEAT or VS CPU + pick a side.  In game: R new game, M/ESC menu.

// ── piece encoding ───────────────────────────────────────────────────────────
// board[64], index = rank*8 + file. file 0 = a (left), rank 0 = top = black's back row.
// We treat rank 0 as BLACK's home, rank 7 as WHITE's home. White moves up (toward rank 0).
enum { EMPTY=0, WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK };
#define IS_WHITE(p) ((p)>=WP && (p)<=WK)
#define IS_BLACK(p) ((p)>=BP && (p)<=BK)
#define TYPE_OF(p)  ((p)==EMPTY?0:(((p)-1)%6)+1) // 1=P 2=N 3=B 4=R 5=Q 6=K

static int grid[64];
static int sideToMove;        // 0 = white, 1 = black
static int castleRights;      // bits: 1=WK 2=WQ 4=BK 8=BQ
static int epSquare;          // en-passant target square, -1 none

// piece values for eval (index by type 1..6)
static const int PVAL[7] = { 0, 100, 320, 330, 500, 900, 20000 };

// ── moves ────────────────────────────────────────────────────────────────────
typedef struct {
    int from, to;
    int promo;     // promoted-to piece (Q) or 0
    int flag;      // 0 normal, 1 dbl-pawn, 2 ep-capture, 3 castle-K, 4 castle-Q
} Move;

// state snapshot for unmake
typedef struct { int captured, castle, ep, side; } Undo;

static Move legal[256];       // legal moves for the side to move (refreshed each turn)
static int  nLegal;

// ── game state / UI ────────────────────────────────────────────────────────────
enum { ST_TITLE, ST_PLAY, ST_OVER };
static int appState = ST_TITLE;
static int vsCPU = 0;         // 0 hotseat, 1 vs cpu
static int humanSide = 0;     // when vsCPU: which color the human plays
static int selected = -1;     // selected square, -1 none
static int result = 0;        // 0 ongoing, 1 white wins, 2 black wins, 3 stalemate
static int inCheck = 0;       // is side-to-move in check
static int lastFrom = -1, lastTo = -1;
static float msgT = 0;
static int cpuThinking = 0;
static float cpuTimer = 0;

// capture fade effect
static struct { int piece, sq; float t; } capFx;

// ── geometry ─────────────────────────────────────────────────────────────────
#define BSZ   22                   // square size in px
#define BX    ((SCREEN_W - 8*BSZ)/2 + 4)
#define BY    18
#define sq_x(s) (BX + ((s)%8)*BSZ)
#define sq_y(s) (BY + ((s)/8)*BSZ)

// ── sound ────────────────────────────────────────────────────────────────────
static void snd_move(void)    { note(48, INSTR_TRI, 4); }
static void snd_capture(void) { hit(40, INSTR_NOISE, 4, 90); note(55, INSTR_SQUARE, 3); }
static void snd_illegal(void) { hit(34, INSTR_SQUARE, 3, 70); }
static void snd_check(void)   { note(67, INSTR_SQUARE, 4); }
static void snd_mate(void)    { strum(60, CHORD_MAJ7, INSTR_TRI, 5, 70); }

// ── board setup ──────────────────────────────────────────────────────────────
static void setup(void) {
    static const int back[8] = { WR, WN, WB, WQ, WK, WB, WN, WR };
    memset(grid, 0, sizeof(grid));
    for (int f = 0; f < 8; f++) {
        grid[0*8+f] = back[f] + 6;   // black back row (offset +6 maps WR..->BR..)
        grid[1*8+f] = BP;
        grid[6*8+f] = WP;
        grid[7*8+f] = back[f];
    }
    sideToMove = 0;
    castleRights = 1|2|4|8;
    epSquare = -1;
}

// ── attack detection ─────────────────────────────────────────────────────────
// is square `sq` attacked by side `by` (0 white, 1 black)?
static int attacked(int sq, int by) {
    int r = sq/8, f = sq%8;
    // pawns: white pawns attack upward (toward smaller rank), black downward
    if (by == 0) {
        if (r+1 < 8) {
            if (f-1 >= 0 && grid[(r+1)*8+f-1] == WP) return 1;
            if (f+1 < 8 && grid[(r+1)*8+f+1] == WP) return 1;
        }
    } else {
        if (r-1 >= 0) {
            if (f-1 >= 0 && grid[(r-1)*8+f-1] == BP) return 1;
            if (f+1 < 8 && grid[(r-1)*8+f+1] == BP) return 1;
        }
    }
    // knights
    static const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    int wantN = (by==0)?WN:BN;
    for (int i = 0; i < 8; i++) {
        int rr = r+kn[i][0], ff = f+kn[i][1];
        if (rr>=0&&rr<8&&ff>=0&&ff<8 && grid[rr*8+ff]==wantN) return 1;
    }
    // king
    int wantK = (by==0)?WK:BK;
    for (int dr=-1; dr<=1; dr++) for (int df=-1; df<=1; df++) {
        if (!dr&&!df) continue;
        int rr=r+dr, ff=f+df;
        if (rr>=0&&rr<8&&ff>=0&&ff<8 && grid[rr*8+ff]==wantK) return 1;
    }
    // sliders: rook/queen orthogonal, bishop/queen diagonal
    static const int ortho[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    static const int diag[4][2]  = {{1,1},{1,-1},{-1,1},{-1,-1}};
    int wantR=(by==0)?WR:BR, wantB=(by==0)?WB:BB, wantQ=(by==0)?WQ:BQ;
    for (int i=0;i<4;i++) {
        int rr=r, ff=f;
        for (;;) {
            rr+=ortho[i][0]; ff+=ortho[i][1];
            if (rr<0||rr>=8||ff<0||ff>=8) break;
            int p = grid[rr*8+ff];
            if (p) { if (p==wantR||p==wantQ) return 1; break; }
        }
    }
    for (int i=0;i<4;i++) {
        int rr=r, ff=f;
        for (;;) {
            rr+=diag[i][0]; ff+=diag[i][1];
            if (rr<0||rr>=8||ff<0||ff>=8) break;
            int p = grid[rr*8+ff];
            if (p) { if (p==wantB||p==wantQ) return 1; break; }
        }
    }
    return 0;
}

static int king_sq(int side) {
    int want = side==0 ? WK : BK;
    for (int i=0;i<64;i++) if (grid[i]==want) return i;
    return -1;
}
static int king_in_check(int side) {
    int ks = king_sq(side);
    if (ks < 0) return 0;
    return attacked(ks, side^1);
}

// ── make / unmake ──────────────────────────────────────────────────────────────
static void make_move(Move m, Undo *u) {
    u->castle = castleRights; u->ep = epSquare; u->side = sideToMove;
    u->captured = grid[m.to];
    int p = grid[m.from];
    grid[m.to] = p;
    grid[m.from] = EMPTY;
    epSquare = -1;

    if (m.flag == 1) {            // double pawn push -> set ep square (midpoint)
        epSquare = (m.from + m.to) / 2;
    } else if (m.flag == 2) {     // en passant capture: remove the pawn beside `to`
        int capSq = (sideToMove==0) ? m.to + 8 : m.to - 8;
        u->captured = grid[capSq];
        grid[capSq] = EMPTY;
    } else if (m.flag == 3) {     // castle kingside: move rook
        int rr = (m.from/8)*8;
        grid[rr+5] = grid[rr+7]; grid[rr+7] = EMPTY;
    } else if (m.flag == 4) {     // castle queenside
        int rr = (m.from/8)*8;
        grid[rr+3] = grid[rr+0]; grid[rr+0] = EMPTY;
    }
    if (m.promo) grid[m.to] = m.promo;

    // update castle rights when king/rook move or rook captured
    if (p==WK) castleRights &= ~(1|2);
    if (p==BK) castleRights &= ~(4|8);
    if (m.from==63 || m.to==63) castleRights &= ~1;   // h1 white K-rook
    if (m.from==56 || m.to==56) castleRights &= ~2;   // a1 white Q-rook
    if (m.from==7  || m.to==7)  castleRights &= ~4;   // h8 black K-rook
    if (m.from==0  || m.to==0)  castleRights &= ~8;   // a8 black Q-rook

    sideToMove ^= 1;
}

static void unmake_move(Move m, Undo *u) {
    sideToMove = u->side;
    castleRights = u->castle;
    int dest = grid[m.to];
    // undo promotion
    int orig = m.promo ? (sideToMove==0 ? WP : BP) : dest;
    grid[m.from] = orig;
    grid[m.to] = EMPTY;

    if (m.flag == 2) {            // en passant: restore captured pawn beside `to`
        int capSq = (sideToMove==0) ? m.to + 8 : m.to - 8;
        grid[capSq] = u->captured;
    } else {
        grid[m.to] = u->captured;
    }
    if (m.flag == 3) {            // un-castle kingside rook
        int rr = (m.from/8)*8;
        grid[rr+7] = grid[rr+5]; grid[rr+5] = EMPTY;
    } else if (m.flag == 4) {
        int rr = (m.from/8)*8;
        grid[rr+0] = grid[rr+3]; grid[rr+3] = EMPTY;
    }
    epSquare = u->ep;
}

// ── pseudo-legal move generation for `side`, then filter by self-check ────────
static int gen_moves(int side, Move *out) {
    int n = 0;
    for (int s = 0; s < 64; s++) {
        int p = grid[s];
        if (p == EMPTY) continue;
        if (side==0 && !IS_WHITE(p)) continue;
        if (side==1 && !IS_BLACK(p)) continue;
        int r = s/8, f = s%8;
        int t = TYPE_OF(p);

        if (t == 1) {  // pawn
            int dir = side==0 ? -1 : 1;            // white moves up (toward rank 0)
            int startRank = side==0 ? 6 : 1;
            int promoRank = side==0 ? 0 : 7;
            int one = (r+dir)*8 + f;
            if (r+dir>=0 && r+dir<8 && grid[one]==EMPTY) {
                if (r+dir == promoRank) {
                    Move m={s,one,(side==0?WQ:BQ),0}; out[n++]=m;
                } else {
                    Move m={s,one,0,0}; out[n++]=m;
                    if (r == startRank) {
                        int two = (r+2*dir)*8 + f;
                        if (grid[two]==EMPTY) { Move m2={s,two,0,1}; out[n++]=m2; }
                    }
                }
            }
            for (int df=-1; df<=1; df+=2) {
                int cf = f+df, cr = r+dir;
                if (cf<0||cf>=8||cr<0||cr>=8) continue;
                int cs = cr*8+cf;
                int cp = grid[cs];
                int enemy = side==0 ? IS_BLACK(cp) : IS_WHITE(cp);
                if (cp && enemy) {
                    if (cr == promoRank) { Move m={s,cs,(side==0?WQ:BQ),0}; out[n++]=m; }
                    else { Move m={s,cs,0,0}; out[n++]=m; }
                } else if (cs == epSquare) {
                    Move m={s,cs,0,2}; out[n++]=m;
                }
            }
        }
        else if (t == 2) {  // knight
            static const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
            for (int i=0;i<8;i++) {
                int rr=r+kn[i][0], ff=f+kn[i][1];
                if (rr<0||rr>=8||ff<0||ff>=8) continue;
                int ts=rr*8+ff, tp=grid[ts];
                int own = side==0 ? IS_WHITE(tp) : IS_BLACK(tp);
                if (tp && own) continue;
                Move m={s,ts,0,0}; out[n++]=m;
            }
        }
        else if (t == 6) {  // king
            for (int dr=-1;dr<=1;dr++) for (int df=-1;df<=1;df++) {
                if (!dr&&!df) continue;
                int rr=r+dr, ff=f+df;
                if (rr<0||rr>=8||ff<0||ff>=8) continue;
                int ts=rr*8+ff, tp=grid[ts];
                int own = side==0 ? IS_WHITE(tp) : IS_BLACK(tp);
                if (tp && own) continue;
                Move m={s,ts,0,0}; out[n++]=m;
            }
            // castling
            int homeRank = side==0 ? 7 : 0;
            if (r==homeRank && f==4 && !king_in_check(side)) {
                int kBit = side==0 ? 1 : 4;
                int qBit = side==0 ? 2 : 8;
                int base = homeRank*8;
                if ((castleRights & kBit) && grid[base+5]==EMPTY && grid[base+6]==EMPTY
                    && !attacked(base+5, side^1) && !attacked(base+6, side^1)) {
                    Move m={s,base+6,0,3}; out[n++]=m;
                }
                if ((castleRights & qBit) && grid[base+1]==EMPTY && grid[base+2]==EMPTY
                    && grid[base+3]==EMPTY
                    && !attacked(base+3, side^1) && !attacked(base+2, side^1)) {
                    Move m={s,base+2,0,4}; out[n++]=m;
                }
            }
        }
        else {  // sliders: bishop(3), rook(4), queen(5)
            static const int ortho[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
            static const int diag[4][2]  = {{1,1},{1,-1},{-1,1},{-1,-1}};
            const int (*dirs)[2]; int ndir;
            if (t==3) { dirs=diag; ndir=4; }
            else if (t==4) { dirs=ortho; ndir=4; }
            else { dirs=ortho; ndir=8; } // queen: handled below in two passes
            // do ortho then diag for queen
            int passes = (t==5)?2:1;
            for (int pass=0; pass<passes; pass++) {
                const int (*dd)[2] = (t==5) ? (pass==0?ortho:diag) : dirs;
                int cnt = (t==5)?4:ndir;
                for (int i=0;i<cnt;i++) {
                    int rr=r, ff=f;
                    for (;;) {
                        rr+=dd[i][0]; ff+=dd[i][1];
                        if (rr<0||rr>=8||ff<0||ff>=8) break;
                        int ts=rr*8+ff, tp=grid[ts];
                        int own = side==0 ? IS_WHITE(tp) : IS_BLACK(tp);
                        if (tp && own) break;
                        Move m={s,ts,0,0}; out[n++]=m;
                        if (tp) break;
                    }
                }
            }
        }
    }
    return n;
}

// generate fully-legal moves (filter pseudo by self-check)
static int gen_legal(int side, Move *out) {
    Move pseudo[256];
    int np = gen_moves(side, pseudo);
    int n = 0;
    for (int i=0;i<np;i++) {
        Undo u;
        make_move(pseudo[i], &u);
        if (!king_in_check(side)) out[n++] = pseudo[i];
        unmake_move(pseudo[i], &u);
    }
    return n;
}

// ── refresh legal list + detect end states for the side to move ──────────────
static void refresh_state(void) {
    nLegal = gen_legal(sideToMove, legal);
    inCheck = king_in_check(sideToMove);
    if (nLegal == 0) {
        if (inCheck) result = (sideToMove==0) ? 2 : 1;  // mated side loses
        else result = 3;                                // stalemate
        appState = ST_OVER;
        msgT = 0;
        if (inCheck) snd_mate();
        shake(inCheck ? 5 : 0);
        // record tally
        if (result==1) save_int("wwins", load_int("wwins",0)+1);
        else if (result==2) save_int("bwins", load_int("bwins",0)+1);
        else save_int("draws", load_int("draws",0)+1);
    } else if (inCheck) {
        snd_check();
    }
}

// ── evaluation (from white's perspective) ─────────────────────────────────────
// small center bonus via piece-square: reward central control lightly
static int eval_board(void) {
    int score = 0;
    for (int s=0;s<64;s++) {
        int p = grid[s];
        if (!p) continue;
        int t = TYPE_OF(p);
        int v = PVAL[t];
        int r=s/8, f=s%8;
        // simple central bonus: closeness to the center files/ranks
        int cf = (f==3||f==4)?12 : (f==2||f==5)?6 : 0;
        int cr = (r==3||r==4)?12 : (r==2||r==5)?6 : 0;
        int bonus = (t<=3) ? (cf+cr) : 0;   // pawns/minors like the center
        if (IS_WHITE(p)) score += v + bonus;
        else score -= v + bonus;
    }
    return score;
}

// ── negamax + alpha-beta. returns score from `side`'s perspective. ─────────────
static int negamax(int side, int depth, int alpha, int beta) {
    if (depth == 0) {
        int e = eval_board();
        return side==0 ? e : -e;
    }
    Move moves[256];
    int n = gen_legal(side, moves);
    if (n == 0) {
        if (king_in_check(side)) return -30000 + (3 - depth); // mate (prefer slower)
        return 0; // stalemate
    }
    int best = -100000;
    for (int i=0;i<n;i++) {
        Undo u;
        make_move(moves[i], &u);
        int s = -negamax(side^1, depth-1, -beta, -alpha);
        unmake_move(moves[i], &u);
        if (s > best) best = s;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

static Move pick_cpu_move(int side) {
    Move moves[256];
    int n = gen_legal(side, moves);
    Move best = moves[0];
    int bestScore = -100000;
    int alpha = -100000, beta = 100000;
    for (int i=0;i<n;i++) {
        Undo u;
        make_move(moves[i], &u);
        int s = -negamax(side^1, 2, -beta, -alpha); // depth 3 total
        unmake_move(moves[i], &u);
        // tiny tie-break jitter so it isn't perfectly deterministic
        if (s > bestScore || (s == bestScore && rnd(3)==0)) { bestScore = s; best = moves[i]; }
        if (s > alpha) alpha = s;
    }
    return best;
}

// ── applying a move from the UI / CPU ──────────────────────────────────────────
static void apply_move(Move m) {
    int captured = grid[m.to];
    int wasCapture = (captured != EMPTY) || m.flag==2;
    if (m.flag == 2) captured = (sideToMove==0)?BP:WP;
    if (wasCapture) {
        capFx.piece = captured;
        capFx.sq = m.to;
        capFx.t = 1.0f;
    }
    Undo u;
    make_move(m, &u);
    lastFrom = m.from; lastTo = m.to;
    selected = -1;
    if (wasCapture) snd_capture(); else snd_move();
    refresh_state();
    if (vsCPU && appState==ST_PLAY && sideToMove != humanSide) {
        cpuThinking = 1;
        cpuTimer = 0;
    }
}

// ── piece drawing (pixel-art sprites from chess.cart.js) ─────────────────────────
// draw a piece of `type` (1..6) for `white` at square center. Sprite slot == type for
// white, type+8 for black. `alpha` (the capture-fade fx) is conveyed by the caller's
// blink + timer, so the sprite is the same either way.
static void draw_piece(int type, int white, int cx, int cy, int alpha) {
    (void)alpha;
    spr(white ? type : type + 8, cx - 8, cy - 8);
}

// ── input → board square (or -1) ───────────────────────────────────────────────
static int mouse_square(void) {
    int mx = mouse_x(), my = mouse_y();
    int f = (mx - BX) / BSZ;
    int r = (my - BY) / BSZ;
    if (mx < BX || my < BY || f<0||f>=8||r<0||r>=8) return -1;
    return r*8+f;
}

static int is_my_piece(int s) {
    int p = grid[s];
    if (!p) return 0;
    return sideToMove==0 ? IS_WHITE(p) : IS_BLACK(p);
}

// is `to` a legal target for currently `selected` piece? returns move index or -1
static int find_move(int from, int to) {
    for (int i=0;i<nLegal;i++)
        if (legal[i].from==from && legal[i].to==to) return i;
    return -1;
}
static int has_target(int from, int to) { return find_move(from,to) >= 0; }

// ── update ───────────────────────────────────────────────────────────────────
void init(void) {
    colorkey(0);          // index 0 = transparent, so the piece sprites sit on the squares
    setup();
    refresh_state();
}

void update(void) {
    float d = dt();
    msgT += d;
    if (capFx.t > 0) capFx.t -= d * 3.0f;

    if (appState == ST_TITLE) {
        if (mouse_pressed(MOUSE_LEFT)) {
            int mx=mouse_x(), my=mouse_y();
            // HOTSEAT button
            if (point_in_box(mx,my, SCREEN_W/2-70, 92, 60, 22)) {
                vsCPU = 0; appState = ST_PLAY; setup(); selected=-1; lastFrom=lastTo=-1;
                result=0; cpuThinking=0; refresh_state(); snd_move();
            }
            // VS CPU (play white)
            else if (point_in_box(mx,my, SCREEN_W/2+10, 92, 60, 22)) {
                vsCPU = 1; humanSide = 0; appState = ST_PLAY; setup(); selected=-1;
                lastFrom=lastTo=-1; result=0; cpuThinking=0; refresh_state(); snd_move();
            }
            // VS CPU (play black) — CPU moves first
            else if (point_in_box(mx,my, SCREEN_W/2+10, 120, 60, 22)) {
                vsCPU = 1; humanSide = 1; appState = ST_PLAY; setup(); selected=-1;
                lastFrom=lastTo=-1; result=0; refresh_state();
                cpuThinking = 1; cpuTimer = 0; snd_move();
            }
        }
        return;
    }

    if (keyp('R')) {
        setup(); selected=-1; lastFrom=lastTo=-1; result=0; cpuThinking=0;
        appState = ST_PLAY; refresh_state();
    }
    if (keyp('M') || keyp(KEY_ESCAPE)) { appState = ST_TITLE; selected=-1; return; }

    if (appState == ST_OVER) return;

    // CPU turn
    if (vsCPU && sideToMove != humanSide) {
        if (cpuThinking) {
            cpuTimer += d;
            if (cpuTimer > 0.35f) {       // brief "thinking" pause
                Move m = pick_cpu_move(sideToMove);
                cpuThinking = 0;
                apply_move(m);
            }
        }
        return;
    }

    // human turn — mouse select / move
    if (mouse_pressed(MOUSE_LEFT)) {
        int s = mouse_square();
        if (s < 0) return;
        if (selected < 0) {
            if (is_my_piece(s)) selected = s;
        } else {
            if (s == selected) { selected = -1; }    // deselect
            else {
                int mi = find_move(selected, s);
                if (mi >= 0) {
                    apply_move(legal[mi]);
                } else if (is_my_piece(s)) {
                    selected = s;                     // reselect another piece
                } else {
                    snd_illegal();
                    selected = -1;
                }
            }
        }
    }
}

// ── drawing ────────────────────────────────────────────────────────────────────
static void draw_board_and_pieces(void) {
    // squares
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) {
        int s = r*8+f;
        int light = (r+f)%2==0;
        int col = light ? CLR_LIGHT_YELLOW : CLR_BLUE_GREEN;
        rectfill(sq_x(s), sq_y(s), BSZ, BSZ, col);
        // last move trail
        if (s==lastFrom || s==lastTo)
            rectfill(sq_x(s), sq_y(s), BSZ, BSZ, light?CLR_DARK_ORANGE:CLR_BROWN);
    }
    // selected pulse
    if (selected >= 0) {
        int pulse = (int)(2 + 1.5f*sin_deg(now()*360));
        rect(sq_x(selected)+1, sq_y(selected)+1, BSZ-2, BSZ-2, CLR_GREEN);
        rect(sq_x(selected)+pulse, sq_y(selected)+pulse, BSZ-2*pulse, BSZ-2*pulse, CLR_LIME_GREEN);
    }
    // check warning on the king
    if (inCheck && appState==ST_PLAY) {
        int ks = king_sq(sideToMove);
        if (ks>=0 && blink(8))
            rect(sq_x(ks), sq_y(ks), BSZ, BSZ, CLR_RED);
    }
    // legal target highlights
    if (selected >= 0) {
        for (int i=0;i<nLegal;i++) {
            if (legal[i].from != selected) continue;
            int t = legal[i].to;
            int ccx = sq_x(t)+BSZ/2, ccy = sq_y(t)+BSZ/2;
            if (grid[t]!=EMPTY || legal[i].flag==2) {
                ring(ccx, ccy, 7, 9, 0, 360, CLR_GREEN);   // capture ring
            } else {
                circfill(ccx, ccy, 3, CLR_MEDIUM_GREEN);   // move dot
            }
        }
    }
    // pieces
    for (int s=0;s<64;s++) {
        int p = grid[s];
        if (!p) continue;
        draw_piece(TYPE_OF(p), IS_WHITE(p), sq_x(s)+BSZ/2, sq_y(s)+BSZ/2, 0);
    }
    // capture fade fx
    if (capFx.t > 0 && capFx.piece) {
        int s = capFx.sq;
        if (blink(4))
            draw_piece(TYPE_OF(capFx.piece), IS_WHITE(capFx.piece),
                       sq_x(s)+BSZ/2, sq_y(s)+BSZ/2, 1);
    }
    // board border
    rect(BX-1, BY-1, 8*BSZ+2, 8*BSZ+2, CLR_WHITE);
}

static void draw_hud(void) {
    int hy = BY + 8*BSZ + 4;
    char who[40];
    if (vsCPU) {
        int humanTurn = (sideToMove == humanSide);
        strcpy(who, humanTurn ? "YOUR MOVE" : "CPU THINKING...");
    } else {
        strcpy(who, sideToMove==0 ? "WHITE TO MOVE" : "BLACK TO MOVE");
    }
    print(who, BX, hy, sideToMove==0 ? CLR_WHITE : CLR_LIGHT_GREY);
    if (inCheck) print("CHECK!", BX + 8*BSZ - 48, hy, CLR_RED);
    print(vsCPU ? "M:MENU  R:NEW" : "M:MENU  R:NEW", BX, hy+9, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    if (appState == ST_TITLE) {
        print_centered("C H E S S", SCREEN_W/2, 30, CLR_LIGHT_YELLOW);
        print_centered("full legal chess", SCREEN_W/2, 44, CLR_LIGHT_GREY);

        // buttons
        rectfill(SCREEN_W/2-70, 92, 60, 22, CLR_BLUE_GREEN);
        rect(SCREEN_W/2-70, 92, 60, 22, CLR_WHITE);
        print("HOTSEAT", SCREEN_W/2-65, 99, CLR_WHITE);

        rectfill(SCREEN_W/2+10, 92, 60, 22, CLR_DARK_GREEN);
        rect(SCREEN_W/2+10, 92, 60, 22, CLR_WHITE);
        print("VS CPU(W)", SCREEN_W/2+14, 99, CLR_WHITE);

        rectfill(SCREEN_W/2+10, 120, 60, 22, CLR_DARK_PURPLE);
        rect(SCREEN_W/2+10, 120, 60, 22, CLR_WHITE);
        print("VS CPU(B)", SCREEN_W/2+14, 127, CLR_WHITE);

        // tally
        int ww = load_int("wwins",0), bw = load_int("bwins",0), dr = load_int("draws",0);
        print(str("W:%d  B:%d  draws:%d", ww, bw, dr), 8, SCREEN_H-12, CLR_INDIGO);
        print("click a mode to begin", SCREEN_W/2-44, SCREEN_H-12, CLR_DARK_GREY);
        return;
    }

    draw_board_and_pieces();
    draw_hud();

    if (appState == ST_OVER) {
        // dim + result banner
        rectfill(0, SCREEN_H/2-22, SCREEN_W, 44, CLR_BLACK);
        rect(0, SCREEN_H/2-22, SCREEN_W, 44, CLR_WHITE);
        const char *msg = result==1 ? "WHITE WINS — CHECKMATE"
                        : result==2 ? "BLACK WINS — CHECKMATE"
                        : "STALEMATE — DRAW";
        int col = result==3 ? CLR_LIGHT_GREY : CLR_LIGHT_YELLOW;
        print_centered(msg, SCREEN_W/2, SCREEN_H/2-12, col);
        if (blink(20))
            print_centered("R: NEW GAME    M: MENU", SCREEN_W/2, SCREEN_H/2+4, CLR_WHITE);
    }
}
