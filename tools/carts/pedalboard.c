// pedalboard — an electric guitar you PLAY, through a row of stompboxes you STOMP. The showcase
// for stacking the engine effects on one live instrument.
//
// Two hands, like a real guitar:
//   FRETTING HAND — the ROOT row (Z X C V B N M) moves your hand up the neck (roots E F G A B C D),
//                   and the SHAPE row (A S D F G) sets the chord shape (5 / maj / min / sus4 / 7).
//                   The two combine into a chord voiced across the six strings.
//   STRUMMING HAND — the right end of the guitar (over the body) is the STRUM ZONE: drag across the
//                    strings there to strum the whole chord. On the neck side, tap a string to pick
//                    a single note.
//
// The pedalboard reads left→right as a real guitar signal chain — dirt, tone, modulation, then the
// time/space tail. Each box has its real knob row + a daft face graphic; stomp the footswitch (or
// 1..6) to toggle it, drag a knob to dial it:
//   BITCRUSH BIT·RTE·MIX · EQ LO·MID·HI · CHORUS RTE·DEP·MIX · FLANGER RTE·DEP·FB·MIX
//   TAPE WOW·FLT·SAT · REVERB SIZE·DMP·MIX
// (The engine applies master inserts in its own fixed order; the board layout is the guitar reading.)
//
// Mouse + touch both work — every contact is its own pointer. The mouse is merged in explicitly (not
// via the engine's mouse-as-touch bridge, which latches off after any stray trackpad touch).

#include "studio.h"
#include <math.h>

#define I_GTR  5
#define I_MUTE 6      // a choked, muted voice for picking the short nut-side string segment
#define NSTR   6
#define NPED   6
#define MAXK   4
#define NSHAPE 5
#define NROOT  7

// ── pedals (left→right signal order), each with its real row of knobs ──
enum { P_BIT, P_EQ, P_CHO, P_FLG, P_TAPE, P_REV };
typedef struct {
    const char *name; int body, accent;
    int nk; const char *klabel[MAXK]; float k[MAXK];
    bool on;
} Pedal;
static Pedal ped[NPED] = {
    { "BITCRUSH", CLR_DARK_BROWN,    CLR_DARK_ORANGE,  3, { "BIT","RTE","MIX" },      { 0.50f, 0.40f, 0.60f },       false },
    { "EQ",       CLR_DARKER_BLUE,   CLR_BLUE,         3, { "LO","MID","HI" },        { 0.50f, 0.50f, 0.50f },       false },  // 0.5 = 0 dB (flat)
    { "CHORUS",   CLR_DARKER_PURPLE, CLR_PINK,         3, { "RTE","DEP","MIX" },      { 0.40f, 0.55f, 0.55f },       true  },  // boots ON — lush electric
    { "FLANGER",  CLR_BLUE_GREEN,    CLR_MEDIUM_GREEN, 4, { "RT","DP","FB","MX" },    { 0.20f, 0.70f, 0.60f, 0.50f }, false },
    { "TAPE",     CLR_DARK_RED,      CLR_PEACH,        3, { "WOW","FLT","SAT" },      { 0.35f, 0.25f, 0.45f },       false },
    { "REVERB",   CLR_DARK_BLUE,     CLR_INDIGO,       3, { "SIZ","DMP","MIX" },      { 0.70f, 0.40f, 0.45f },       false },
};

// ── the fretting hand: real guitar tab ──  standard tuning, E-shape MOVEABLE chords.
// Each shape is a per-string fret pattern. −1 = NO FINGER: the string isn't pressed but still RINGS
// OPEN (its open-string pitch) — it's never silenced. The ROOT row is the barre position up the
// neck, so fingered strings climb while the −1 strings drone open. The power chord fingers only the
// low three; the rest ring open.
static const int OPEN[NSTR] = { 40, 45, 50, 55, 59, 64 };   // E A D G B E (low→high)
static const int SHAPE_F[NSHAPE][NSTR] = {
    { 0, 2, 2, -1, -1, -1 },   // 5    power — finger root/5th/octave; high strings ring open
    { 0, 2, 2,  1,  0,  0 },   // maj  E-shape major
    { 0, 2, 2,  0,  0,  0 },   // min  E-shape minor
    { 0, 2, 2,  2,  0,  0 },   // sus4 suspended fourth
    { 0, 2, 0,  1,  0,  0 },   // 7    E-shape dominant 7
};
static const char *SHAPE_NAME[NSHAPE] = { "5", "maj", "min", "sus4", "7" };
static const char  SHAPE_KEY[NSHAPE]  = { 'A', 'S', 'D', 'F', 'G' };
static const int   ROOT_FRET[NROOT]   = { 0, 1, 3, 5, 7, 8, 10 };         // barre fret for E F G A B C D
static const char *ROOT_NAME[NROOT]   = { "E", "F", "G", "A", "B", "C", "D" };
static const char  ROOT_KEY[NROOT]    = { 'Z', 'X', 'C', 'V', 'B', 'N', 'M' };
#define FRET_W 7
static int  sel_shape = 0;
static int  sel_root  = 0;
static int  str_midi[NSTR];

// ── string visuals ──
static float amp[NSTR];
static float vib_ph[NSTR];
static int   pend[NSTR];

static bool  autoplay = true;
static int   apos = 0;
static int   dirty = 1;

// ── geometry ──
#define PED_Y 14
#define PED_H 72
#define PED_W 48
#define PED_X(i) (4 + (i) * 52)
#define SX0   22                         // nut (neck end)
#define SX1   302                        // bridge (body end)
#define STRUMX 196                       // strum zone starts here (right side, over the body)
#define STR_Y0 96
#define STR_DY 7
#define STR_Y(s) (STR_Y0 + (s) * STR_DY)
#define SHAPE_Y 143
#define SHAPE_W 56
#define SHAPE_X(i) (12 + (i) * 60)
#define ROOT_Y  159
#define ROOT_W  40
#define ROOT_X(i) (11 + (i) * 43)
#define ROW_H 14

static int knob_cx(int k, int j) { return PED_X(k) + 4 + (PED_W - 8) * (2 * j + 1) / (2 * ped[k].nk); }
static int knob_slot(int k)      { return (PED_W - 8) / ped[k].nk; }
#define KNOB_CY  (PED_Y + 44)
#define ILLU_CY  (PED_Y + 22)

static int gate_ms(void) { return 1800; }

// fret on string s for the current chord: 0 = open (no finger, still rings), >0 = fingered
static int str_fret(int s) { return SHAPE_F[sel_shape][s] < 0 ? 0 : ROOT_FRET[sel_root] + SHAPE_F[sel_shape][s]; }
static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) str_midi[s] = OPEN[s] + str_fret(s);   // every string sounds
}

// push every pedal's state to the engine (master bus + the guitar's reverb send). OFF = dry.
static void apply_fx(void) {
    float *b = ped[P_BIT].k, *e = ped[P_EQ].k, *c = ped[P_CHO].k, *f = ped[P_FLG].k, *t = ped[P_TAPE].k, *r = ped[P_REV].k;
    crush(16.0f - b[0] * 14.0f, 1.0f + b[1] * 15.0f, ped[P_BIT].on ? b[2] : 0.0f);
    if (ped[P_EQ].on) eq((e[0]-0.5f)*24.0f, (e[1]-0.5f)*24.0f, (e[2]-0.5f)*24.0f);
    else              eq(0.0f, 0.0f, 0.0f);
    chorus(0.1f + c[0] * 4.9f, c[1], ped[P_CHO].on ? c[2] : 0.0f);
    flanger(0.05f + f[0] * 4.95f, f[1], f[2] * 0.95f, ped[P_FLG].on ? f[3] : 0.0f);
    tape(ped[P_TAPE].on ? t[0] : 0.0f, ped[P_TAPE].on ? t[1] : 0.0f, ped[P_TAPE].on ? t[2] : 0.0f);
    reverb(0.2f + r[0] * 0.78f, r[1]);                                  // configure the room
    instrument_reverb(I_GTR, ped[P_REV].on ? r[2] : 0.0f);             // how wet the guitar is
}

void init(void) {
    instrument(I_GTR, INSTR_GUITAR, 1, 0, 7, 1200);   // long release: the gate never chops a ringing string
    instrument_harmonics(I_GTR, 0.55f);
    instrument_timbre(I_GTR, 0.85f);
    instrument_morph(I_GTR, 0.15f);
    instrument_drive_mode(I_GTR, DRIVE_SOFT);
    instrument_drive(I_GTR, 0.18f);
    instrument(I_MUTE, INSTR_GUITAR, 1, 0, 2, 180);   // the choked nut-side "ghost" voice
    instrument_harmonics(I_MUTE, 0.5f);
    instrument_timbre(I_MUTE, 0.95f);                 // bright
    instrument_morph(I_MUTE, 0.92f);                  // tight mute / pizzicato stop
    build_strings();
    bpm(100);
    apply_fx();
    amp[0] = 0.8f; amp[2] = 1.0f; amp[4] = 0.6f;
}

static void pluck_str(int s, int vol) {
    if (s < 0 || s >= NSTR) return;
    hit(str_midi[s], I_GTR, vol, gate_ms());
    amp[s] = 1.0f; vib_ph[s] = 0.0f;
}

static void strum_down(void) {
    for (int s = 0; s < NSTR; s++) {
        schedule_hit(s * 28, str_midi[s], I_GTR, 5, gate_ms());
        pend[s] = 1 + (s * 28 * 60) / 1000;
    }
}

// selecting a chord just FRETS it (no sound) — you strum the strum zone (or SPACE) to play it
static void set_shape(int sh) { sel_shape = sh; build_strings(); autoplay = false; }
static void set_root(int r)   { sel_root  = r;  build_strings(); autoplay = false; }
static int  near_string(int ty) { int s = (ty - STR_Y0 + STR_DY / 2) / STR_DY; return s < 0 ? 0 : s >= NSTR ? NSTR - 1 : s; }

// the x of string s's finger dot (where the fretting hand presses); open strings sit at the nut
static int dot_x(int s) {
    int f = str_fret(s);
    if (f == 0) return SX0 + 2;
    int dx = SX0 + 6 + f * FRET_W;
    return dx > STRUMX - 16 ? STRUMX - 16 : dx;
}
// pick string s at pointer x `px`. Right of the finger → the real note. Left of the finger (the
// short nut-side segment, only on a FINGERED string) → a much higher, MUTED "ghost": the segment
// from nut to finger is short, so it rings well above the open pitch (real guitar physics).
static void pick_string(int s, int px) {
    if (s < 0 || s >= NSTR) return;
    int f = str_fret(s);
    if (f > 0 && px < dot_x(s) - 1) {
        float seg = 1.0f - powf(2.0f, -f / 12.0f);                 // nut-side fraction of the string
        int hi = (str_midi[s] - f) + (int)(12.0f * log2f(1.0f / seg) + 0.5f);   // open pitch + segment ratio
        if (hi > 103) hi = 103;
        hit(hi, I_MUTE, 4, 130);
        amp[s] = 0.7f; vib_ph[s] = 0.0f;
    } else {
        pluck_str(s, 6);
    }
}

// ── per-contact pointer pool ──
#define NPTR 10
#define NOID (-999)
enum { PTR_IDLE, PTR_KNOB, PTR_PICK };
typedef struct { int id, mode, ped, knob, prevY, x, y; } Ptr;
static Ptr ptr[NPTR];

void update(void) {
    for (int i = 0; i < NSHAPE; i++) if (keyp(SHAPE_KEY[i])) set_shape(i);
    for (int i = 0; i < NROOT;  i++) if (keyp(ROOT_KEY[i]))  set_root(i);
    for (int i = 0; i < NPED;   i++) if (keyp('1' + i)) { ped[i].on = !ped[i].on; dirty = 1; }
    if (keyp(KEY_SPACE)) { strum_down(); autoplay = false; }

    int cid[NPTR], cxp[NPTR], cyp[NPTR], nc = 0;
    for (int i = 0; i < touch_count() && nc < NPTR; i++) { cid[nc] = touch_id(i); cxp[nc] = touch_x(i); cyp[nc] = touch_y(i); nc++; }
    if (mouse_down(MOUSE_LEFT) && nc < NPTR) {
        int mxp = mouse_x(), myp = mouse_y(), dup = 0;
        for (int i = 0; i < nc; i++) { int dx = cxp[i] - mxp, dy = cyp[i] - myp; if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) dup = 1; }
        if (!dup) { cid[nc] = -100; cxp[nc] = mxp; cyp[nc] = myp; nc++; }
    }
    if (tapp(SCREEN_W - 70, 4, 66, 10)) autoplay = !autoplay;

    for (int i = 0; i < nc; i++) {
        int id = cid[i], tx = cxp[i], ty = cyp[i];
        Ptr *p = 0, *freeP = 0;
        for (int j = 0; j < NPTR; j++) {
            if (ptr[j].id == id) { p = &ptr[j]; break; }
            if (ptr[j].id == NOID && !freeP) freeP = &ptr[j];
        }
        if (!p) {
            if (!freeP) continue;
            p = freeP; *p = (Ptr){ id, PTR_IDLE, -1, -1, ty, tx, ty };

            for (int k = 0; k < NPED && p->mode == PTR_IDLE; k++) {
                int px = PED_X(k);
                if (point_in_box(tx, ty, px + 8, PED_Y + 57, PED_W - 16, 14)) { ped[k].on = !ped[k].on; dirty = 1; }
                else if (point_in_box(tx, ty, px, KNOB_CY - 11, PED_W, 22)) {
                    int slot = knob_slot(k);
                    for (int j = 0; j < ped[k].nk; j++)
                        if (tx >= knob_cx(k, j) - slot / 2 && tx < knob_cx(k, j) + slot / 2) { p->mode = PTR_KNOB; p->ped = k; p->knob = j; }
                }
            }
            if (p->mode == PTR_IDLE)
                for (int i2 = 0; i2 < NSHAPE; i2++) if (point_in_box(tx, ty, SHAPE_X(i2), SHAPE_Y, SHAPE_W, ROW_H)) set_shape(i2);
            if (p->mode == PTR_IDLE)
                for (int i2 = 0; i2 < NROOT; i2++) if (point_in_box(tx, ty, ROOT_X(i2), ROOT_Y, ROOT_W, ROW_H)) set_root(i2);
            // the whole guitar is a pick/strum surface: drag across the strings to strum, tap one to
            // pick it. Per string, the pick X decides — over the body (right of the finger) = the real
            // chord; near the nut (left of the finger) = the high muted ghost. So strum the body for
            // the chord, strum the neck for the inverse high "ghost shape".
            if (p->mode == PTR_IDLE && ty >= STR_Y0 - 9 && ty <= STR_Y(NSTR - 1) + 9 && tx >= SX0 - 8 && tx <= SX1 + 8) {
                p->mode = PTR_PICK; autoplay = false;
                pick_string(near_string(ty), tx);
                p->prevY = ty;
            }
        } else if (p->mode == PTR_KNOB) {
            ped[p->ped].k[p->knob] = clamp(ped[p->ped].k[p->knob] + (p->prevY - ty) * 0.012f, 0.0f, 1.0f);
            dirty = 1; p->prevY = ty;
        } else if (p->mode == PTR_PICK) {
            for (int s = 0; s < NSTR; s++) {
                int ys = STR_Y(s);
                if ((p->prevY < ys && ty >= ys) || (p->prevY > ys && ty <= ys)) pick_string(s, tx);
            }
            p->prevY = ty;
        }
        p->x = tx; p->y = ty;
    }
    for (int j = 0; j < NPTR; j++) {
        if (ptr[j].id == NOID) continue;
        int present = 0;
        for (int i = 0; i < nc; i++) if (cid[i] == ptr[j].id) { present = 1; break; }
        if (!present) ptr[j].id = NOID;
    }

    if (dirty) { apply_fx(); dirty = 0; }

    if (autoplay && every(1)) {
        static const int prog[8] = { 0, 2, 6, 3, 0, 5, 3, 2 };
        if (beat() % 4 == 0) { sel_shape = 0; sel_root = prog[apos % 8]; build_strings(); strum_down(); apos++; }
    }

    for (int s = 0; s < NSTR; s++) if (pend[s] > 0 && --pend[s] == 0) { amp[s] = 1.0f; vib_ph[s] = 0.0f; }

#ifdef DE_TRACE
    watch("root", "%d", sel_root); watch("shape", "%d", sel_shape);
    watch("bit", "%d", ped[P_BIT].on); watch("rev", "%d", ped[P_REV].on); watch("tape", "%d", ped[P_TAPE].on);
#endif
}

static void draw_illu(int k, int cx, int col, int bg) {
    int cy = ILLU_CY;
    if (k == P_BIT) {                                   // 8-bit critter
        int ix = cx - 6, iy = cy - 5;
        rectfill(ix + 2, iy, 8, 2, col); rectfill(ix, iy + 2, 12, 4, col);
        rectfill(ix, iy + 6, 3, 3, col); rectfill(ix + 5, iy + 6, 2, 2, col); rectfill(ix + 9, iy + 6, 3, 3, col);
        rectfill(ix + 3, iy + 3, 2, 2, bg); rectfill(ix + 7, iy + 3, 2, 2, bg);
    } else if (k == P_EQ) {                             // equalizer spectrum
        static const int hh[5] = { 5, 11, 7, 13, 8 };
        for (int i = 0; i < 5; i++) rectfill(cx - 10 + i * 5, cy + 6 - hh[i], 3, hh[i], col);
    } else if (k == P_CHO) {                            // shimmer waves
        for (int o = 0; o < 2; o++) {
            int base = cy + (o ? 3 : -3), px = cx - 15, py = base;
            for (int xx = cx - 15; xx <= cx + 15; xx += 2) { int wy = base + (int)(sinf((xx - cx) * 0.42f + o * 1.6f) * 3.0f); line(px, py, xx, wy, col); px = xx; py = wy; }
        }
    } else if (k == P_FLG) {                            // a jet
        int jx = cx + 7, jy = cy;
        trifill(jx - 14, jy - 2, jx - 14, jy + 2, jx, jy, col);
        trifill(jx - 10, jy, jx - 15, jy - 6, jx - 5, jy, col);
        trifill(jx - 10, jy, jx - 15, jy + 6, jx - 5, jy, col);
        for (int i = 0; i < 3; i++) line(cx - 16, jy - 3 + i * 3, cx - 9, jy - 3 + i * 3, col);
    } else if (k == P_TAPE) {                           // two tape reels
        circ(cx - 7, cy, 4, col); circfill(cx - 7, cy, 1, col);
        circ(cx + 7, cy, 4, col); circfill(cx + 7, cy, 1, col);
        line(cx - 7, cy - 5, cx + 7, cy - 5, col); line(cx - 7, cy + 5, cx + 7, cy + 5, col);
    } else {                                            // REVERB — expanding rings (the bloom)
        for (int i = 1; i <= 3; i++) circ(cx, cy, i * 3, col);
        pset(cx, cy, col);
    }
}

static void draw_pedal(int k) {
    Pedal *pd = &ped[k];
    int x = PED_X(k), cx = x + PED_W / 2;
    rrectfill(x, PED_Y, PED_W, PED_H, 4, pd->body);
    rrect(x, PED_Y, PED_W, PED_H, 4, pd->on ? pd->accent : CLR_DARKER_GREY);
    font(FONT_SMALL);
    print_centered(pd->name, cx, PED_Y + 3, pd->on ? CLR_WHITE : CLR_MEDIUM_GREY);
    draw_illu(k, cx, pd->on ? pd->accent : CLR_DARKER_GREY, pd->body);
    int kr = pd->nk >= 4 ? 4 : 5;
    for (int j = 0; j < pd->nk; j++) {
        int kx = knob_cx(k, j);
        circfill(kx, KNOB_CY, kr, CLR_BROWNISH_BLACK);
        circ(kx, KNOB_CY, kr, pd->on ? pd->accent : CLR_DARK_GREY);
        float a = (-135.0f + pd->k[j] * 270.0f) * 0.0174533f;
        line(kx, KNOB_CY, kx + (int)(sinf(a) * (kr - 1)), KNOB_CY - (int)(cosf(a) * (kr - 1)), pd->on ? CLR_WHITE : CLR_MEDIUM_GREY);
        font(FONT_TINY);
        print_centered(pd->klabel[j], kx, KNOB_CY + kr + 1, pd->on ? CLR_LIGHT_PEACH : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);
    circfill(x + 7, PED_Y + 63, 2, pd->on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rrectfill(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_BROWNISH_BLACK);
    rrect(x + 12, PED_Y + 58, PED_W - 20, 12, 2, CLR_DARK_GREY);
}

static void draw_btnrow(int x, int y, int w, const char *label, char key, bool on, int accent) {
    rrectfill(x, y, w, ROW_H, 3, on ? accent : CLR_DARKER_GREY);
    rrect(x, y, w, ROW_H, 3, on ? CLR_WHITE : CLR_DARK_GREY);
    print_centered(label, x + w / 2, y + 4, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    font(FONT_TINY); print(str("%c", key), x + 2, y + 1, on ? CLR_BLACK : CLR_DARK_GREY); font(FONT_NORMAL);
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("PEDALBOARD", 6, 3, CLR_PEACH);
    font(FONT_SMALL);
    print_right(autoplay ? "AUTO: on" : "AUTO: off", SCREEN_W - 6, 5, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);

    for (int k = 0; k < NPED; k++) {
        draw_pedal(k);
        if (k < NPED - 1) print(">", PED_X(k) + PED_W, KNOB_CY - 3, CLR_DARK_GREY);
    }

    // ── the electric guitar: blue-green body, cream pickguard, two pickups, 6 strings ──
    int by = STR_Y0 - 8, bh = (STR_Y(NSTR - 1) + 8) - by;
    rrectfill(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE_GREEN);
    rrect(6, by, SCREEN_W - 12, bh, 6, CLR_BLUE);
    rrectfill(SX0 - 8, by + 3, SX1 - SX0 + 28, bh - 6, 4, CLR_LIGHT_PEACH);
    // the STRUM ZONE — a tinted lane over the body where you sweep to strum
    rectfill(STRUMX, by + 3, SX1 - STRUMX + 4, bh - 6, CLR_PEACH);
    rectfill(SX0 + 60, by + 3, 5, bh - 6, CLR_DARKER_GREY);   // neck pickup
    rectfill(STRUMX - 8, by + 3, 5, bh - 6, CLR_DARKER_GREY); // bridge pickup
    rectfill(SX0 - 4, by + 2, 3, bh - 4, CLR_MEDIUM_GREY);    // nut

    for (int s = 0; s < NSTR; s++) {
        int y = STR_Y(s);
        amp[s] *= 0.93f; vib_ph[s] += 0.6f;
        int col = amp[s] > 0.5f ? CLR_LIGHT_YELLOW : amp[s] > 0.1f ? CLR_DARK_ORANGE : CLR_MEDIUM_GREY;
        int px = SX0, py = y;
        for (int xx = SX0 + 8; xx <= SX1; xx += 8) {
            float t  = (float)(xx - SX0) / (float)(SX1 - SX0);
            int   wy = y + (int)(amp[s] * 4.0f * sinf(t * 9.42f + vib_ph[s]) * sinf(t * 3.14f));
            line(px, py, xx, wy, col); px = xx; py = wy;
        }
    }
    // fretting hand — hollow ring = open (no finger, still rings), filled dot = fingered
    // (the dot slides up the neck with the root)
    for (int s = 0; s < NSTR; s++) {
        int f = str_fret(s), y = STR_Y(s);
        if (f == 0) {                                             // open — no finger
            circ(SX0 + 2, y, 2, CLR_DARK_RED);
        } else {                                                  // fingered
            int dx = SX0 + 6 + f * FRET_W; if (dx > STRUMX - 16) dx = STRUMX - 16;
            circfill(dx, y, 2, CLR_DARK_RED);
            pset(dx - 1, y - 1, CLR_PEACH);
        }
    }
    font(FONT_TINY); print_centered("STRUM", (STRUMX + SX1) / 2, by + bh - 7, CLR_DARK_BROWN); font(FONT_NORMAL);

    for (int j = 0; j < NPTR; j++)
        if (ptr[j].id != NOID && ptr[j].mode == PTR_PICK)
            trifill(ptr[j].x - 3, ptr[j].y - 4, ptr[j].x + 3, ptr[j].y - 4, ptr[j].x, ptr[j].y + 4, CLR_WHITE);

    for (int i = 0; i < NSHAPE; i++) draw_btnrow(SHAPE_X(i), SHAPE_Y, SHAPE_W, SHAPE_NAME[i], SHAPE_KEY[i], i == sel_shape, CLR_ORANGE);
    for (int i = 0; i < NROOT;  i++) draw_btnrow(ROOT_X(i),  ROOT_Y,  ROOT_W,  ROOT_NAME[i],  ROOT_KEY[i],  i == sel_root,  CLR_LIME_GREEN);

    font(FONT_TINY);
    print_centered("ZXCVBNM = neck   ASDFG = shape   strum the body = chord, near the nut = high ghosts", SCREEN_W / 2, ROOT_Y + ROW_H + 2, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
