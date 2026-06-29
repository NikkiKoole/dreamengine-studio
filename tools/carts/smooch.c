/* de:meta
{
  "title": "smooch lounge",
  "status": "active",
  "kind": [
    "game"
  ],
  "teaches": [
    "step-sequencer",
    "palette-cycling"
  ],
  "lineage": "Guitar Hero / rhythm-game genre — four-lane fall-down chart driven by the engine beat clock; the pal()-recolor crowd trick and a hand-authored chart editor are the novel parts.",
  "genre": "arcade",
  "description": "A saucy cabaret rhythm game, all cheeky tone and winks. A pixel-art chanteuse works a velvet speakeasy while hearts and lips fall down four lanes to a sultry lounge groove (walking bass, brushed swing hats, a vibrato-LFO 'voice', a bandpass sax stab) — tap the lane key as each note crosses the hit line for a SMOOCH / SMOOTH / CLUMSY rating, or a wrong-time WHIFF. The clock IS the music: every note rides the beat()/beat_pos() playhead like the drum machine. Good kisses grow a combo multiplier and fill the TEASE meter; fill it for a BIG FINISH where bonus off-beat notes appear, points double, the dancer struts and the pal()-recolored crowd cheers in confetti. 'Blow a kiss' hold notes you keep down for a bonus, lip-print stamps + heart bursts, a buzzing neon marquee, dithered velvet curtains and a swaying spotlight, then a flirty letter grade (STAR OF THE SHOW down to STAGE FRIGHT) with a saved hi-score. Tasteful innuendo, never explicit. A S K L tap the lanes (arrow keys for a gamepad), hold for a long kiss, SPACE to start / encore."
}
de:meta */
#include "studio.h"

// SMOOCH LOUNGE — a saucy cabaret rhythm game.
//
// A pixel-art chanteuse works the stage of a velvet speakeasy while hearts and
// lips fall down four lanes to a sultry lounge groove. Tap the matching lane key
// the instant a note crosses the hit line; tight timing lands a SMOOCH, looser a
// SMOOTH, late/early a CLUMSY, a wrong-time tap a WHIFF. Good kisses grow a combo
// and fill the TEASE meter — fill it for a BIG FINISH (double points, the dancer
// struts, the whole lounge goes wild). A letter grade + saved hi-score at the end.
//
// The clock IS the music: every note sits on a beat and the playhead is the song
// itself (beat()/beat_pos()), exactly like the drum machine. The crowd is the
// crowd.c pal()-recolor trick. Everything is cheeky tone, never explicit.
//
//   A S K L   tap a lane (also arrow keys for a gamepad d-pad)
//   SPACE     start / encore
//   long notes ("blow a kiss") — hold the lane key across the ribbon for a bonus

// ---- layout ----------------------------------------------------------------
#define LANES   4
#define LANEX(i) (170 + (i) * 31)     // left edge of lane i
#define LANEW   28
#define LCX(i)  (LANEX(i) + 14)        // lane center (LANEW/2)
#define HITY    158                   // the hit line
#define TOPY    22                    // notes appear here
#define LEAD    4.0f                  // beats of look-ahead
#define PPB     ((HITY - TOPY) / LEAD)// pixels a note falls per beat (~34)

// ---- timing windows (fractions of a beat) ----------------------------------
#define W_SMOOCH 0.09f
#define W_SMOOTH 0.18f
#define W_CLUMSY 0.30f
#define W_MISS   0.42f

// ---- song -------------------------------------------------------------------
#define TEMPO    96
#define SONG_END 78.0f
#define BF_LEN   8.0f                 // beats a BIG FINISH lasts

#define MAXN 360

enum { ST_TITLE, ST_PLAY, ST_END };
enum { R_NONE, R_CLUMSY, R_SMOOTH, R_SMOOCH };

typedef struct { float b; int lane; float hold; bool bonus; bool hit; bool dead; } Note;
static Note  chart[MAXN];
static int   nnotes;

typedef struct { float x, y, vx, vy, life, max, grav; int col, kind; } Part;
static Part  parts[150];

static int   state = ST_TITLE;
static float song0;                   // continuous-beat value when the song began
static float songpos;                 // beats into the song
static long  score;
static int   combo, maxcombo;
static float tease;                   // 0..1 flirt meter
static bool  bigfinish;
static float bf_until;
static int   c_smooch, c_smooth, c_clumsy, c_miss, c_total;
static int   holding[LANES];          // chart index being held in a lane, or -1
static float lane_flash[LANES];
static float poseT;                   // squash-into-the-pose envelope
static int   dancer_pose;             // 0 idle 1 shimmy 2 fan 3 kick
static float jt;                      // judgment callout timer
static const char *jtext = "";
static int   jcol = CLR_WHITE;
static int   last16 = -999;
static int   hiscore;
static bool  new_hi;

static const int LKEY[LANES]  = { 'A', 'S', 'K', 'L' };
static const int LARR[LANES]  = { BTN_LEFT, BTN_DOWN, BTN_UP, BTN_RIGHT };
static const int LCOL[LANES]  = { CLR_RED, CLR_PINK, CLR_ORANGE, CLR_PEACH };

// walking lounge bass — two bars of quarters, loops
static const int BASS[8] = { 33, 36, 40, 43, 41, 40, 36, 35 };

static const char *SONG_TITLE = "\"Pucker Up, Buttercup\"";
static const char *SMOOCH_LINES[] = { "OOH LA LA!", "HUBBA HUBBA!", "MWAH!", "SMOOCH!" };
static const char *SMOOTH_LINES[] = { "smooth...", "oh my!", "*wink*" };
static const char *CLUMSY_LINES[] = { "clumsy!", "oof", "rusty!" };
static const char *WHIFF_LINES[]  = { "*womp*", "missed me!", "*fans self*" };

// ---- helpers ---------------------------------------------------------------
static float fab(float v) { return v < 0 ? -v : v; }
static float cbeat(void)  { return (float)beat() + beat_pos(); }   // continuous beats

static void psp(float x, float y, float vx, float vy, float life, float grav, int col, int kind) {
    for (int i = 0; i < 150; i++)
        if (parts[i].life <= 0) {
            parts[i] = (Part){ x, y, vx, vy, life, life, grav, col, kind };
            return;
        }
}

static void tiny_heart(int x, int y, int r, int c) {
    if (r < 1) r = 1;
    circfill(x - r, y, r, c);
    circfill(x + r, y, r, c);
    trifill(x - r * 2, y, x + r * 2, y, x, y + r * 2 + 1, c);
}

// ---- chart authoring -------------------------------------------------------
static void addn(float b, int lane, float hold, bool bonus) {
    if (nnotes >= MAXN) return;
    chart[nnotes++] = (Note){ b, lane, hold, bonus, false, false };
}

static void build_chart(void) {
    nnotes = 0;
    // verse — gentle quarters that walk the lanes (beats 4..20)
    static const int pat1[16] = { 0,1,2,3, 3,2,1,0, 0,2,1,3, 2,0,3,1 };
    for (int i = 0; i < 16; i++) addn(4 + i, pat1[i], 0, false);
    addn(20, 1, 3.0f, false);                         // first "blow a kiss" hold

    // chorus — eighth notes, with off-16ths that only appear in a BIG FINISH (24..40)
    for (int i = 0; i < 32; i++) {
        float b = 24 + i * 0.5f;
        int ln = (i & 1) ? ((i / 2 + 1) % 4) : ((i / 2) % 4);
        addn(b, ln, 0, false);
        if (i % 4 == 0) addn(b + 0.25f, (ln + 2) % 4, 0, true);
    }
    addn(40, 2, 3.0f, false);                          // hold

    // bridge — syncopation (44..56)
    static const float syb[16] = { 44,44.5f,45.5f,46, 47,47.5f,48.5f,49, 50,50.5f,51.5f,52, 53,54,55,55.5f };
    static const int   syl[16] = { 0,2,1,3, 2,0,3,1, 0,3,2,1, 3,2,1,0 };
    for (int i = 0; i < 16; i++) addn(syb[i], syl[i], 0, false);

    // climax — a sweeping run; the on-beats always play, the off-beats are the
    // BIG-FINISH "double up" (only live while the meter is lit) (58..72)
    for (int i = 0; i < 56; i++) {
        float b = 58 + i * 0.25f;
        int ln = (i % 8 < 4) ? (i % 4) : (3 - (i % 4));
        addn(b, ln, 0, (i & 1));
    }
    addn(72, 0, 2.0f, false);                          // finale double-hold
    addn(72, 3, 2.0f, false);
}

// ---- state transitions -----------------------------------------------------
static void start_song(void) {
    for (int i = 0; i < nnotes; i++) { chart[i].hit = false; chart[i].dead = false; }
    for (int i = 0; i < LANES; i++) { holding[i] = -1; lane_flash[i] = 0; }
    score = 0; combo = 0; maxcombo = 0; tease = 0; bigfinish = false; bf_until = 0;
    c_smooch = c_smooth = c_clumsy = c_miss = c_total = 0;
    poseT = 0; dancer_pose = 0; jt = 0; jtext = ""; new_hi = false;
    song0 = cbeat();
    state = ST_PLAY;
}

static void start_bigfinish(void) {
    bigfinish = true;
    bf_until = songpos + BF_LEN;
    shake(3);
    strum(57, CHORD_DOM7, 8, 5, 22);                   // sax stab
    for (int i = 0; i < 40; i++)
        psp(rnd(SCREEN_W), rnd(40), rnd_float_between(-1.2f, 1.2f),
            rnd_float_between(0.5f, 2.2f), 1.4f, 0.04f,
            (i & 1) ? CLR_PINK : CLR_YELLOW, (i % 3) ? 1 : 2);
}

// ---- sound ------------------------------------------------------------------
static void play_audio(void) {
    int s16 = (int)(cbeat() * 4.0f);
    if (s16 == last16) return;
    last16 = s16;
    int st = ((s16 % 16) + 16) % 16;
    bool playing = (state == ST_PLAY);

    // drums — jazzy kick, backbeat snare, brushed swing hats
    if (st == 0 || st == 8 || st == 6 || st == 14) hit(36, INSTR_TRI, 4, 90);
    if (st == 4 || st == 12)                        hit(52, INSTR_NOISE, 3, 120);
    if (st & 1)                                     hit(82, INSTR_NOISE, bigfinish ? 2 : 1, bigfinish ? 38 : 26);

    // walking bass on the quarters
    if (st % 4 == 0) note(BASS[(s16 / 4) % 8], 5, 3);

    // sultry vibrato lead — sparse, busier in the big finish
    if (playing && (st == 2 || st == 7 || st == 10) && chance(bigfinish ? 75 : 38))
        note(degree(SCALE_BLUES, 4, rnd(7)), 7, bigfinish ? 4 : 2);

    // sax accents through the big finish
    if (bigfinish && (st == 0 || st == 8)) strum(57, CHORD_MIN7, 8, 3, 16);
}

// ---- judging ----------------------------------------------------------------
static void judge_press(int L) {
    int best = -1; float berr = 999;
    for (int i = 0; i < nnotes; i++) {
        Note *n = &chart[i];
        if (n->lane != L || n->hit || n->dead) continue;
        if (n->bonus && !bigfinish) continue;
        float e = fab(n->b - songpos);
        if (e < berr) { berr = e; best = i; }
    }
    lane_flash[L] = 1;

    if (best < 0 || berr > W_CLUMSY) {                  // WHIFF — tapped at nothing
        combo = 0;
        if (!bigfinish) tease = clamp(tease - 0.05f, 0, 1);
        jtext = WHIFF_LINES[rnd(3)]; jcol = CLR_LIGHT_GREY; jt = 0.7f;
        note(48, 9, 3); schedule(110, 45, 9, 2);        // sad muted trumpet
        shake(1.5f);
#ifdef DE_TRACE
        watch("press", "L%d WHIFF nearest_err=%.3f (win=%.2f)", L, berr, W_CLUMSY);
#endif
        return;
    }

    Note *n = &chart[best];
    n->hit = true;
    int rating = (berr <= W_SMOOCH) ? R_SMOOCH : (berr <= W_SMOOTH) ? R_SMOOTH : R_CLUMSY;

    combo++;
    if (combo > maxcombo) maxcombo = combo;
    int mult = combo < 8 ? 1 : combo < 16 ? 2 : combo < 32 ? 3 : 4;
    int base = rating == R_SMOOCH ? 100 : rating == R_SMOOTH ? 60 : 20;
    score += (long)base * mult * (bigfinish ? 2 : 1);

    if (!n->bonus) {
        c_total++;
        if (rating == R_SMOOCH) c_smooch++; else if (rating == R_SMOOTH) c_smooth++; else c_clumsy++;
    }
    if (!bigfinish)
        tease = clamp(tease + (rating == R_SMOOCH ? 0.055f : rating == R_SMOOTH ? 0.035f : 0.012f), 0, 1);

    // callout + flavor
    if (rating == R_SMOOCH)      { jtext = SMOOCH_LINES[rnd(4)]; jcol = CLR_PINK; }
    else if (rating == R_SMOOTH) { jtext = SMOOTH_LINES[rnd(3)]; jcol = CLR_YELLOW; }
    else                         { jtext = CLUMSY_LINES[rnd(3)]; jcol = CLR_ORANGE; }
    jt = 0.7f;

    // juice
    poseT = 1; dancer_pose = (rating == R_SMOOCH) ? 2 : 1;
    int cx = LCX(L), cy = HITY;
    int bursts = rating == R_SMOOCH ? 10 : rating == R_SMOOTH ? 6 : 3;
    for (int i = 0; i < bursts; i++)
        psp(cx, cy, rnd_float_between(-1.6f, 1.6f), rnd_float_between(-2.6f, -0.4f),
            rnd_float_between(0.5f, 1.0f), 0.06f, LCOL[L], 1);
    if (rating == R_SMOOCH) psp(cx, cy - 2, 0, -0.3f, 0.8f, 0, CLR_RED, 3);   // lip print

    hit(64 + L * 4 + (rating == R_SMOOCH ? 12 : 0), 10, 4, 90);               // kiss *mwah*

    if (n->hold > 0) holding[L] = best;                  // begin the hold
    if (!bigfinish && tease >= 1.0f) start_bigfinish();
#ifdef DE_TRACE
    // the press that landed: error vs the note, the rating it earned, and whether
    // a hold armed. "arm=1 hold=3.0" means the head hit and the 3-beat ribbon began.
    watch("press", "L%d e=%.3f r=%s arm=%d hold=%.1f", L, berr,
          rating == R_SMOOCH ? "SMOOCH" : rating == R_SMOOTH ? "SMOOTH" : "CLUMSY",
          n->hold > 0 ? 1 : 0, n->hold);
#endif
}

// ---- update -----------------------------------------------------------------
void init(void) {
    colorkey(0);
    build_chart();
    bpm(TEMPO);
    instrument(5, INSTR_TRI, 8, 120, 4, 130);  instrument_filter(5, FILTER_LOW, 700, 3);   // bass
    instrument(7, INSTR_SINE, 45, 200, 5, 260); instrument_filter(7, FILTER_LOW, 1400, 5);  // sultry voice
    instrument_lfo(7, 0, LFO_PITCH, 5.5f, 0.35f);
    instrument(8, INSTR_SAW, 6, 120, 5, 180);  instrument_filter(8, FILTER_BAND, 900, 9);   // sax stab
    instrument(9, INSTR_TRI, 22, 160, 3, 220); instrument_filter(9, FILTER_LOW, 600, 4);    // muted trumpet
    instrument(10, INSTR_SINE, 4, 60, 2, 80);                                               // kiss mwah
    hiscore = load_int("smooch_hi", 0);
    for (int i = 0; i < LANES; i++) holding[i] = -1;
}

static bool start_pressed(void) {
    return keyp(KEY_SPACE) || btnp(0, BTN_A) || btnp(1, BTN_A);
}

void update(void) {
    float k = dt() * 60.0f;
    play_audio();
#ifdef DE_TRACE
    watch("state", "%d", state);   // 0 title 1 play 2 end — always, every state
#endif

    // particles always tick (ambient even on title/end)
    for (int i = 0; i < 150; i++)
        if (parts[i].life > 0) {
            parts[i].x += parts[i].vx * k;
            parts[i].y += parts[i].vy * k;
            parts[i].vy += parts[i].grav * k;
            parts[i].life -= dt();
        }

    jt = clamp(jt - dt(), 0, 1);
    poseT = clamp(poseT - dt() * 1.8f, 0, 1);
    for (int i = 0; i < LANES; i++) lane_flash[i] = clamp(lane_flash[i] - dt() * 4.0f, 0, 1);

    if (state == ST_TITLE) {
        dancer_pose = 2;
        if (start_pressed()) start_song();
        return;
    }
    if (state == ST_END) {
        dancer_pose = 0;
        if (start_pressed()) start_song();
        return;
    }

    // ---- ST_PLAY ----
    songpos = cbeat() - song0;
    if (songpos > SONG_END) {
        if (score > hiscore) { hiscore = (int)score; save_int("smooch_hi", hiscore); new_hi = true; }
        state = ST_END;
        strum(60, CHORD_MAJ7, 8, 5, 40);
        return;
    }

    if (bigfinish) {
        if (songpos >= bf_until) { bigfinish = false; tease = 0.35f; }
        else { tease = (bf_until - songpos) / BF_LEN; if (chance(35)) shake(0.6f); }
    }

    // taps
    for (int L = 0; L < LANES; L++)
        if (keyp(LKEY[L]) || btnp(1, LARR[L])) judge_press(L);

    // holds — reward keeping the key down across the ribbon
    for (int L = 0; L < LANES; L++) {
        if (holding[L] < 0) continue;
        Note *n = &chart[holding[L]];
        float tail = n->b + n->hold;
        bool down = key(LKEY[L]) || btn(1, LARR[L]);
        if (songpos >= tail) {                          // held to the end
            score += (long)150 * (bigfinish ? 2 : 1);
            if (!bigfinish) tease = clamp(tease + 0.06f, 0, 1);
            jtext = "MWAH~"; jcol = CLR_PINK; jt = 0.8f; poseT = 1; dancer_pose = 2;
#ifdef DE_TRACE
            watch("holddone", "L%d tail=%.2f sp=%.3f +bonus", L, tail, songpos);
#endif
            for (int i = 0; i < 12; i++)
                psp(LCX(L), HITY, rnd_float_between(-2, 2), rnd_float_between(-3, -0.5f),
                    rnd_float_between(0.6f, 1.1f), 0.06f, CLR_PINK, 1);
            holding[L] = -1;
            if (!bigfinish && tease >= 1.0f) start_bigfinish();
        } else if (!down) {
            holding[L] = -1;                            // let go early — no bonus, no penalty
        } else if (frame() % 5 == 0) {
            psp(LCX(L) + rnd_between(-6, 6), HITY - rnd(4), 0, -0.4f, 0.5f, 0, CLR_LIGHT_PEACH, 0);
        }
    }

    // misses — notes that fall past the line
    for (int i = 0; i < nnotes; i++) {
        Note *n = &chart[i];
        if (n->hit || n->dead) continue;
        if (n->bonus && !bigfinish) continue;
        if (songpos > n->b + W_MISS) {
            n->dead = true;
            if (!n->bonus) {
                c_miss++; c_total++; combo = 0;
                if (!bigfinish) tease = clamp(tease - 0.06f, 0, 1);
                jtext = "*fans self*"; jcol = CLR_LIGHT_GREY; jt = 0.6f;
                note(47, 9, 3); schedule(120, 44, 9, 2);
#ifdef DE_TRACE
                watch("miss", "b%.1f L%d sp=%.3f", n->b, n->lane, songpos);
#endif
            }
        }
    }

#ifdef DE_TRACE
    // per-frame play state — fed to the --trace JSONL, read back to see exactly
    // what the engine did at any moment (combo, meter, which lane is mid-hold).
    watch("songpos", "%.3f", songpos);
    watch("combo",   "%d", combo);
    watch("tease",   "%.2f", tease);
    watch("bigfin",  "%d", bigfinish ? 1 : 0);
    watch("hold",    "%d %d %d %d", holding[0], holding[1], holding[2], holding[3]);
#endif
}

// ---- drawing ----------------------------------------------------------------
static void draw_backdrop(float sway) {
    cls(CLR_DARKER_PURPLE);

    // velvet side curtains + top valance (dithered for sheen)
    fillp(FILL_VLINES, CLR_DARKER_PURPLE);
    rectfill(0, 0, SCREEN_W, 16, CLR_DARK_RED);
    rectfill(0, 0, 28, SCREEN_H, CLR_DARK_RED);
    rectfill(SCREEN_W - 22, 0, 22, SCREEN_H, CLR_DARK_RED);
    fillp_reset();
    for (int x = 4; x < SCREEN_W; x += 16) circfill(x, 16, 7, CLR_DARK_RED);   // valance scallops
    for (int x = 5; x < 28; x += 7) line(x, 16, x, SCREEN_H, CLR_DARKER_PURPLE);
    for (int x = SCREEN_W - 19; x < SCREEN_W; x += 7) line(x, 16, x, SCREEN_H, CLR_DARKER_PURPLE);

    // neon "SMOOCH LOUNGE" marquee — buzzes, flickers, races bulbs
    int f = frame();
    bool buzz = !(blink(2) && (f % 37 < 2));            // brief flicker
    int neon = (f / 6) % 3 == 0 ? CLR_PINK : (f / 6) % 3 == 1 ? CLR_PEACH : CLR_RED;
    if (bigfinish) neon = (f & 2) ? CLR_YELLOW : CLR_PINK;
    int sgx = 108;
    rectfill(sgx - 6, 2, 116, 13, CLR_DARKER_PURPLE);
    rect(sgx - 6, 2, 116, 13, buzz ? neon : CLR_DARKER_GREY);
    print("SMOOCH LOUNGE", sgx, 5, buzz ? neon : CLR_DARK_RED);
    for (int i = 0; i < 13; i++) {                      // chasing bulbs
        int on = ((f / 4 + i) % 4 == 0) || bigfinish;
        pset(sgx - 3 + i * 9, 2, on ? CLR_YELLOW : CLR_BROWN);
        pset(sgx - 3 + i * 9, 14, on ? CLR_YELLOW : CLR_BROWN);
    }

    // spotlight cone onto the dancer (translucent dither)
    int apex = 92 + (int)sway;
    fillp(FILL_CHECKER, -1);
    trifill(apex, 16, 60, 152, 124, 152, CLR_LIGHT_YELLOW);
    fillp_reset();

    // stage floor + footlights along the front
    rectfill(28, 150, 132, 50, CLR_DARK_BROWN);
    line(28, 150, 160, 150, CLR_BROWN);
    float bp = beat_pos();
    int glow = (state == ST_PLAY && bp < 0.3f) || bigfinish ? CLR_YELLOW : CLR_ORANGE;
    for (int x = 36; x < 158; x += 14) {
        circfill(x, 152, 2, glow);
        if (glow == CLR_YELLOW) circfill(x, 152, 3, CLR_DARK_ORANGE);
    }
}

static void draw_dancer(void) {
    float bp = beat_pos();
    float db = (state == ST_PLAY && bp < 0.25f) ? (1 - bp * 4) : 0;
    float ys = 1 - 0.14f * poseT - 0.07f * db;
    float xs = 1 + 0.12f * poseT + 0.05f * db;
    float fx = 92, fy = 150;
    float scl = 2.0f;
    float dw = 16 * scl * xs, dh = 32 * scl * ys;
    float swy = sin_deg(now() * 36) * (bigfinish ? 6 : 2.2f);
    if (dancer_pose == 1) swy += (frame() % 2 ? 4 : -4);

    int pose = dancer_pose;
    if (state == ST_PLAY && pose == 0 && bp < 0.5f) pose = 1;     // idle bob on the beat
    if (bigfinish && poseT < 0.3f) pose = 3;                       // strut

    ovalfill((int)fx, (int)fy + 2, 16, 4, CLR_DARKER_PURPLE);      // shadow

    // feather fan flourish on the big poses (primitives, recolored)
    if (pose == 2 || pose == 3) {
        int hx = (int)fx + 22, hy = (int)(fy - dh + 18);
        for (int i = -2; i <= 2; i++) {
            int fc = (frame() / 4 + i + bigfinish * 2) % 3 == 0 ? CLR_PINK : CLR_PEACH;
            int ex = hx + (int)dx(16, 60 + i * 22), ey = hy + (int)dy(16, 60 + i * 22);
            ovalfill((hx + ex) / 2, (hy + ey) / 2, 4, 2, fc);
            line(hx, hy, ex, ey, CLR_WHITE);
        }
    }

    sspr_ex(pose * 16, 0, 16, 32, (int)(fx - dw / 2), (int)(fy - dh),
            (int)dw, (int)dh, swy, (int)fx, (int)fy);
}

static void draw_crowd(void) {
    static const int SHADE[6] = { CLR_DARKER_BLUE, CLR_DARKER_PURPLE, CLR_DARK_PURPLE,
                                  CLR_DARKER_GREY, CLR_BLUE_GREEN, CLR_DARKER_BLUE };
    float excite = clamp(tease + combo * 0.01f, 0, 1.4f);
    for (int i = 0; i < 15; i++) {
        int x = 22 + i * 19;
        float ph = i * 1.3f;
        float amp = 1.5f + excite * 4.0f;
        int y = 188 - (int)(fab(sin_deg(now() * 150 + ph * 57)) * amp);
        int shade = bigfinish ? ((i + frame() / 3) & 1 ? CLR_PINK : CLR_YELLOW) : SHADE[i % 6];
        pal(28, shade);
        spr(7, x - 8, y);
        pal_reset();
        // a thrown heart now and then when the room is hot
        if ((bigfinish || excite > 0.6f) && chance(2))
            psp(x, y, rnd_float_between(-0.4f, 0.4f), rnd_float_between(-1.4f, -0.7f),
                1.2f, 0.02f, CLR_PINK, 1);
    }
}

static void draw_lanes(void) {
    for (int L = 0; L < LANES; L++) {
        int x = LANEX(L);
        rectfill(x, TOPY, LANEW, HITY - TOPY + 14, CLR_DARKER_BLUE);
        if (L) line(x - 1, TOPY, x - 1, HITY + 14, CLR_DARKER_GREY);
        // receptor at the hit line
        int rc = lane_flash[L] > 0.05f ? CLR_WHITE : LCOL[L];
        circ(LCX(L), HITY, 8, rc);
        if (lane_flash[L] > 0.05f) circfill(LCX(L), HITY, (int)(8 * lane_flash[L]), LCOL[L]);
        print(str("%c", LKEY[L]), LCX(L) - 3, HITY + 6, CLR_LIGHT_GREY);
    }
    line(LANEX(0), HITY, LANEX(3) + LANEW, HITY, CLR_WHITE);

    // hold ribbons (under the heads)
    for (int i = 0; i < nnotes; i++) {
        Note *n = &chart[i];
        if (n->dead || n->hold <= 0) continue;
        if (n->bonus && !bigfinish) continue;
        float heady = HITY - (n->b - songpos) * PPB;
        float taily = HITY - (n->b + n->hold - songpos) * PPB;
        if (heady < TOPY - 20 || taily > HITY + 20) continue;
        fillp(FILL_CHECKER, -1);
        rectfill(LCX(n->lane) - 4, (int)taily, 8, (int)(heady - taily), LCOL[n->lane]);
        fillp_reset();
    }

    // note heads
    for (int i = 0; i < nnotes; i++) {
        Note *n = &chart[i];
        if (n->hit || n->dead) continue;
        if (n->bonus && !bigfinish) continue;
        float ny = HITY - (n->b - songpos) * PPB;
        if (ny < TOPY - 18 || ny > HITY + 22) continue;
        int sx = LANEX(n->lane) + 6, sy = (int)ny - 8;
        pal(8, LCOL[n->lane]);
        spr((n->lane & 1) ? 5 : 4, sx, sy);            // lips on odd lanes, hearts on even
        pal_reset();
    }
}

static void draw_particles(void) {
    for (int i = 0; i < 150; i++) {
        Part *p = &parts[i];
        if (p->life <= 0) continue;
        float t = p->life / p->max;
        switch (p->kind) {
            case 0: pset((int)p->x, (int)p->y, p->col); break;
            case 1: tiny_heart((int)p->x, (int)p->y, 1 + (int)(t * 1.5f), p->col); break;
            case 2: rectfill((int)p->x, (int)p->y, 2, 3, p->col); break;
            case 3: if (t > 0.15f) spr(6, (int)p->x - 8, (int)p->y - 8); break;
        }
    }
}

static void draw_hud(void) {
    print(str("SCORE %ld", score), 4, 4, CLR_WHITE);
    print_right(str("HI %d", hiscore), SCREEN_W - 4, 4, CLR_LIGHT_YELLOW);

    // tease meter
    if (bigfinish) {
        print_centered("BIG FINISH!", SCREEN_W/2, 17, (frame() & 2) ? CLR_YELLOW : CLR_PINK);
        bar(110, 26, 100, 5, tease, CLR_YELLOW, CLR_DARKER_PURPLE);
    } else {
        print("TEASE", 84, 26, CLR_PINK);
        bar(118, 26, 92, 5, tease, tease > 0.75f ? CLR_PINK : CLR_RED, CLR_DARKER_PURPLE);
    }
    rect(118, 26, 92, 5, CLR_DARKER_GREY);

    // combo over the lanes
    if (combo >= 4) {
        int mult = combo < 8 ? 1 : combo < 16 ? 2 : combo < 32 ? 3 : 4;
        int bx = (LANEX(0) + LANEX(3) + LANEW) / 2;
        int pop = poseT > 0.3f ? 1 : 0;
        print_scaled(str("x%d", mult), bx - 8, 36 - pop, bigfinish ? CLR_YELLOW : CLR_PINK, 2);
        print_centered(str("%d combo", combo), SCREEN_W/2, 52, CLR_LIGHT_GREY);
    }

    // judgment callout near the hit line
    if (jt > 0.05f) {
        int bx = (LANEX(0) + LANEX(3) + LANEW) / 2;
        int yy = 118 - (int)(jt * 8);
        print(jtext, bx - text_width(jtext) / 2, yy, jcol);
    }
}

static void draw_title(void) {
    fillp(FILL_CHECKER, -1);
    rectfill(36, 70, 248, 70, CLR_BLACK);
    fillp_reset();
    print_scaled("SMOOCH", 64, 64, CLR_PINK, 4);
    print_scaled("LOUNGE", 164, 96, CLR_RED, 4);
    print_centered(str("tonight's number: %s", SONG_TITLE), SCREEN_W/2, 132, CLR_LIGHT_YELLOW);
    if (blink(20)) print_centered("press SPACE to take the stage", SCREEN_W/2, 156, CLR_WHITE);
    print_centered("A S K L  =  kiss the beat   (hold for a long kiss)", SCREEN_W/2, 172, CLR_LIGHT_GREY);
}

static void draw_end(void) {
    float perf = c_total ? (c_smooch + 0.5f * c_smooth) / c_total : 0;
    const char *grade, *rank; int gc;
    if (perf >= 0.92f)      { grade = "S"; rank = "STAR OF THE SHOW"; gc = CLR_YELLOW; }
    else if (perf >= 0.80f) { grade = "A"; rank = "HEARTBREAKER";     gc = CLR_PINK; }
    else if (perf >= 0.65f) { grade = "B"; rank = "CHARMER";          gc = CLR_ORANGE; }
    else if (perf >= 0.50f) { grade = "C"; rank = "WALLFLOWER";       gc = CLR_LIGHT_GREY; }
    else                    { grade = "D"; rank = "STAGE FRIGHT";     gc = CLR_DARK_RED; }

    fillp(FILL_CHECKER, -1);
    rectfill(54, 40, 212, 124, CLR_BLACK);
    fillp_reset();
    rect(54, 40, 212, 124, CLR_DARK_RED);
    print_centered("THE CURTAIN FALLS", SCREEN_W/2, 48, CLR_LIGHT_YELLOW);
    print_scaled(grade, 150, 60, gc, 4);
    print_centered(rank, SCREEN_W/2, 96, gc);
    print_centered(str("score %ld", score), SCREEN_W/2, 110, CLR_WHITE);
    print_centered(str("SMOOCH %d   SMOOTH %d   max combo %d", c_smooch, c_smooth, maxcombo), SCREEN_W/2, 122, CLR_LIGHT_GREY);
    if (new_hi && blink(15)) print_centered("* NEW HIGH! *", SCREEN_W/2, 134, CLR_PINK);
    if (blink(20)) print_centered("press SPACE for an encore", SCREEN_W/2, 150, CLR_WHITE);
}

void draw(void) {
    float sway = sin_deg(now() * 36) * (bigfinish ? 3 : 1.2f);
    camera((int)sway, 0);
    draw_backdrop(sway);
    draw_crowd();
    draw_dancer();
    camera(0, 0);

    draw_lanes();
    draw_particles();
    draw_hud();

    if (state == ST_TITLE) draw_title();
    if (state == ST_END)   draw_end();
}
