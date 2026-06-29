/* de:meta
{
  "title": "tabla",
  "status": "active",
  "created": "2026-06-08",
  "kind": [
    "instrument",
    "tech-demo"
  ],
  "teaches": [
    "drum-synthesis",
    "analog-voice-modeling"
  ],
  "lineage": "Showcase for the INSTR_MEMBRANE engine ported from navkit — six decaying sine modes at circular-membrane vibration ratios with a pitch-bend gliss; novel over 808/909 recipes in using real modal physics rather than a single sine + pitch envelope.",
  "description": "INSTR_MEMBRANE showcase - the seventh modeled ENGINE: a struck drumhead. Six decaying sine modes at a circular membrane's vibration ratios plus a slap contact click, buffer-free. One id covers tabla / conga / bongo / djembe / tom - hand percussion the analog 808/909 recipes structurally can't reach, because it's REAL membrane modes plus a pitch bend, not one sine plus a pitch envelope. The three engine macros: instrument_harmonics = head character (0 = tuned harmonic tabla, pitched and longer-ringing; 1 = inharmonic djembe/conga thud), instrument_timbre = strike position (0 = center thump; 1 = edge ring plus slap click - open/slap/mute in one knob, navkit's live circular-membrane weighting), instrument_morph = pitch bend (0 = flat; up = the membrane chirp / tabla bayan gliss - pitch starts raised off the strike and settles, bending all six modes together). A struck engine self-decays, so it has no ADSR (unlike fm/pd, which hold); the cart's fourth slider is instead a cart-side RING control (mute to open) - the hit() gate length, a tight palm-damped thwack to a stroke left to ring out, which is how hand percussion is actually played. The drumhead on the left shows the syahi tuning patch on the tabla side, a marker for where timbre strikes (center to edge), and ripples on every hit; the panel prints the exact instrument() call to copy into your cart. Five presets (1 tabla, 2 conga, 3 bongo, 4 djembe, 5 tom). A S D F G H J K strike a tuned low ladder, Z/X tune, 1..5 kits, drag a slider (or LEFT/RIGHT + UP/DOWN), SPACE roll, M autoplay groove on/off. Multitouch: drum with several fingers, sweep the pads for a roll."
}
de:meta */
// tabla — INSTR_MEMBRANE showcase: a struck drumhead (the seventh modeled ENGINE), six
// decaying sine modes at a circular membrane's vibration ratios. Hand percussion the analog
// 808/909 recipes structurally can't reach: REAL modes + a pitch bend, not one sine + an env.
// One id covers tabla / conga / bongo / djembe / tom. Every engine answers the same three
// 0..1 macro knobs — the API never grows:
//   harmonics = HEAD       (0 = tuned harmonic tabla, pitched + longer ring →
//                           1 = inharmonic djembe/conga thud, shorter)
//   timbre    = STRIKE POS (0 = center thump → 1 = edge ring + slap — open/slap/mute in a knob)
//   morph     = PITCH BEND (0 = flat; up = the membrane chirp / tabla bayan gliss — pitch
//                           starts raised off the strike and settles)
// A struck engine self-decays, so it has NO ADSR (unlike fm/pd, which hold). The 4th slider
// is cart-side, not an engine macro:
//   ring      = MUTE↔OPEN  (the hit() gate length — a tight palm-damped thwack ↔ a stroke left
//                           to ring out; how you actually play hand percussion, ~55ms..1.8s)
//
// The drumhead on the left shows WHERE timbre strikes (center ↔ edge) and ripples on every
// hit; the readout prints the instrument() call to copy into your cart. Dial a kit, play it.
//
// controls: A S D F G H J K  strike (tuned low→high)   ·   Z / X tuning down / up
//           1..5 presets: tabla / conga / bongo / djembe / tom
//           drag a slider (auditions as you drag), or LEFT/RIGHT pick + UP/DOWN turn it
//           SPACE roll   ·   M autoplay groove on/off
//
// MULTITOUCH: every finger is its own pointer — drum with several fingers, sweep across the
// pads for a roll, hold a slider with one finger while the groove plays. Desktop mouse = one.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_MEM 5
#define NPAD  8

static const char STRKEY[NPAD] = { 'A','S','D','F','G','H','J','K' };
// 3 engine macros + a cart-side "ring" (how long a stroke rings before it's damped — the
// open-vs-palm-muted stroke of real hand percussion; it's the hit() gate length, not ADSR).
static const char *MNAME[4] = { "harmonics", "timbre", "morph",  "ring" };
static const char *MLO[4]   = { "tabla",     "center", "flat",   "mute" };
static const char *MHI[4]   = { "djembe",    "edge",   "bend",   "open" };

// presets: macro positions + ring for the five drums navkit's membrane engine models, folded
// into our knobs. STARTING GUESSES — the acceptance tests for the engine; tune them by ear.
typedef struct { const char *name; float h, t, m, r; } Preset;
static const Preset PRESET[5] = {
    { "tabla",  0.10f, 0.45f, 0.45f, 0.85f },  // tuned + pitched, mid strike, the gliss, sings open
    { "conga",  0.55f, 0.35f, 0.15f, 0.70f },  // warm inharmonic, near-center, little bend, rings
    { "bongo",  0.72f, 0.65f, 0.10f, 0.30f },  // bright inharmonic, edge, snappy + short
    { "djembe", 0.85f, 0.55f, 0.22f, 0.45f },  // inharmonic, edge slap, medium thud
    { "tom",    0.62f, 0.18f, 0.55f, 0.75f },  // center boom, big pitch drop, long
};

#define NSLIDER 4              // 0..2 = engine macros, 3 = ring (gate length)
static int   midi_of[NPAD];
static float glow[NPAD];
static float knob[NSLIDER];    // 3 macros + ring, all 0..1
static int   sel = 0;
static bool  autoplay = true;
static int   cur_preset = 0;
static int   apos = 0;
static int   oct = 0;          // Z/X tuning shift

// expanding ripples on the drumhead — each strike spawns one at the strike radius
#define NRIP 12
typedef struct { float age, r0; int col; } Ripple;
static Ripple rip[NRIP];
static float head_pulse = 0.0f;   // whole-head flash on a strike

// per-finger pointer table — each finger drags a slider or drums/sweeps pads
enum { PTR_IDLE, PTR_DRAG, PTR_SWEEP };
typedef struct { int id, mode, k, prevX; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];       // .id == PTR_NONE → slot free

static int km(int b) { return midi_of[b] + oct * 12; }

// layout: head panel · pad row · presets · macro row (4 sliders) · footer
#define PAD_W    34
#define PAD_X(b) (10 + (b) * (PAD_W + 4))
#define PAD_Y    88
#define PAD_H    26
#define MROW_Y   150
#define MROW_W   66
#define MROW_X(k) (8 + (k) * 78)
// drumhead viz
#define HEAD_CX  52
#define HEAD_CY  50
#define HEAD_R   34

#define RING_MAX 1800   // open-stroke gate length (ms) — long, so the head's own modal decay
                        // rings out (a struck engine self-decays; the amp env stays out of its way)
#define RING_MIN 55     // muted-stroke gate length (ms) — a tight palm-damped thwack

// ring knob → hit() gate length, exponential so the low end has fine mute control and the top
// reaches the full open ring (mute ~55ms → open ~1800ms).
static int ring_ms(void) { return (int)(RING_MIN * powf((float)RING_MAX / RING_MIN, knob[3])); }

static void apply_patch(void) {
    instrument(I_MEM, INSTR_MEMBRANE, 1, 0, 7, 250);   // instant attack, full sustain, short release
    instrument_harmonics(I_MEM, knob[0]);
    instrument_timbre(I_MEM, knob[1]);
    instrument_morph(I_MEM, knob[2]);
}

static void spawn_ripple(void) {
    // the strike lands at a radius set by timbre: center (0) → edge (1)
    float rr = HEAD_R * (0.10f + 0.80f * knob[1]);
    for (int i = 0; i < NRIP; i++)
        if (rip[i].age <= 0.0f) {
            rip[i] = (Ripple){ 1.0f, rr, knob[1] > 0.6f ? CLR_LIGHT_YELLOW : CLR_PEACH };
            break;
        }
    head_pulse = 1.0f;
}

static void strike(int b, int vol) {
    hit(km(b), I_MEM, vol, ring_ms());   // gate length = the ring knob (mute → open)
    glow[b] = 1.0f;
    spawn_ripple();
}

static void set_preset(int p) {
    knob[0] = PRESET[p].h;  knob[1] = PRESET[p].t;  knob[2] = PRESET[p].m;  knob[3] = PRESET[p].r;
    cur_preset = p;
    apply_patch();
    strike(2, 6);
}

void init(void) {
    PTR_CLEAR(ptr);
    // a tuned low pentatonic ladder across the pads — tabla/tom territory
    for (int b = 0; b < NPAD; b++) midi_of[b] = degree(SCALE_PENTA_MIN, 2, b);
    set_preset(0);
    bpm(100);
}

void update(void) {
    for (int b = 0; b < NPAD; b++)
        if (keyp(STRKEY[b])) strike(b, 6);
    for (int p = 0; p < 5; p++)
        if (keyp('1' + p)) set_preset(p);

    if (keyp(KEY_LEFT))  sel = (sel + NSLIDER - 1) % NSLIDER;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % NSLIDER;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        cur_preset = -1;
        apply_patch();
        if (frame() % 12 == 0) strike(2, 5);
    }

    if (keyp(KEY_SPACE)) { int d = ring_ms(); for (int k = 0; k < 4; k++) schedule_hit(k * 70, km(2 + k % 3), I_MEM, 5 - k, d); spawn_ripple(); }
    if (keyp('M')) autoplay = !autoplay;
    if ((keyp('Z') || tapp(154, 2, 14, 13)) && oct > -2) { oct--; strike(2, 5); }
    if ((keyp('X') || tapp(212, 2, 14, 13)) && oct <  3) { oct++; strike(2, 5); }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                          // pool full (>PTR_MAX fingers)
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, tx };
            if (point_in_box(tx, ty, SCREEN_W - 92, 2, 88, 12)) { autoplay = !autoplay; continue; }
            if (ty >= PAD_Y + PAD_H + 2 && ty < PAD_Y + PAD_H + 16) {
                for (int q = 0; q < 5; q++)
                    if (tx >= 12 + q * 58 && tx < 12 + q * 58 + 56) set_preset(q);
                continue;
            }
            for (int k = 0; k < NSLIDER; k++)
                if (point_in_box(tx, ty, MROW_X(k) - 2, MROW_Y - 6, MROW_W + 4, 18))
                    { p->mode = PTR_DRAG; p->k = sel = k; }
            if (p->mode == PTR_IDLE && ty >= PAD_Y - 4 && ty < PAD_Y + PAD_H + 2) {
                for (int b = 0; b < NPAD; b++)
                    if (point_in_box(tx, ty, PAD_X(b), PAD_Y, PAD_W, PAD_H)) strike(b, 6);
                p->mode = PTR_SWEEP; p->prevX = tx;
            }
        } else if (p->mode == PTR_DRAG) {
            knob[p->k] = clamp((float)(tx - MROW_X(p->k)) / (float)MROW_W, 0.0f, 1.0f);
            cur_preset = -1;
            apply_patch();
            if (frame() % 12 == 0) strike(2, 5);
        } else if (p->mode == PTR_SWEEP) {
            for (int b = 0; b < NPAD; b++) {
                int cx = PAD_X(b) + PAD_W / 2;
                if ((p->prevX < cx && tx >= cx) || (p->prevX > cx && tx <= cx)) strike(b, 5);
            }
            p->prevX = tx;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // autoplay: a hand-percussion groove — accents on the down/up, ghost strokes between,
    // an occasional flam for life. pads index the tuned ladder.
    if (autoplay && every(1)) {
        static const int seq[16] = { 0,4,2,4, 5,4,2,0, 3,4,2,4, 5,2,1,4 };
        static const int vel[16] = { 6,3,5,3, 6,3,4,3, 6,3,5,3, 6,4,3,4 };
        int s = apos % 16;
        strike(seq[s] % NPAD, vel[s]);
        if (chance(22)) schedule_hit(120, km((seq[s] + 2) % NPAD), I_MEM, 3, ring_ms());  // ghost flam
        apos++;
    }

    // tick the ripples + head flash
    for (int i = 0; i < NRIP; i++) if (rip[i].age > 0.0f) { rip[i].age -= 0.06f; if (rip[i].age < 0.0f) rip[i].age = 0.0f; }
    head_pulse *= 0.85f;

#ifdef DE_TRACE
    watch("harm", "%.2f", knob[0]);
    watch("timb", "%.2f", knob[1]);
    watch("mor",  "%.2f", knob[2]);
    watch("ring", "%.2f", knob[3]);
    watch("ring_ms", "%d", ring_ms());
    watch("preset", "%d", cur_preset);
    watch("oct", "%d", oct);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("TABLA", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("membrane drum engine", 56, 8, CLR_MEDIUM_GREY);
    print_right(autoplay ? "M groove: on" : "M groove: off", SCREEN_W - 10, 8, autoplay ? CLR_LIME_GREEN : CLR_DARK_GREY);
    rect(154, 2, 14, 13, CLR_DARK_GREY);  print("-", 159, 6, CLR_LIGHT_GREY);
    print(str("tune %+d", oct), 172, 8, oct ? CLR_LIGHT_YELLOW : CLR_DARK_GREY);
    rect(212, 2, 14, 13, CLR_DARK_GREY);  print("+", 217, 6, CLR_LIGHT_GREY);
    font(FONT_NORMAL);

    // ── the drumhead: a head that flashes + ripples on strike; a marker shows strike position
    int hp = (int)(head_pulse * 4.0f);
    rectfill(8, 22, 92, 58, CLR_DARK_BROWN);
    circfill(HEAD_CX, HEAD_CY, HEAD_R + 3, CLR_DARK_RED);                 // rim/shell
    circfill(HEAD_CX, HEAD_CY, HEAD_R, head_pulse > 0.3f ? CLR_LIGHT_PEACH : CLR_PEACH);   // skin
    circ(HEAD_CX, HEAD_CY, HEAD_R, CLR_BROWN);
    // the syahi (tabla's tuning patch) when on the harmonic/tabla side; fades toward djembe
    if (knob[0] < 0.5f) circfill(HEAD_CX, HEAD_CY, (int)(HEAD_R * 0.45f), CLR_BROWNISH_BLACK);
    // ripples
    for (int i = 0; i < NRIP; i++) if (rip[i].age > 0.0f) {
        int r = (int)(rip[i].r0 + (1.0f - rip[i].age) * HEAD_R);
        if (r < HEAD_R) circ(HEAD_CX, HEAD_CY, r, rip[i].age > 0.5f ? rip[i].col : CLR_BROWN);
    }
    // strike-position marker: a dot at the radius timbre selects (center → edge)
    {
        int mr = (int)(HEAD_R * (0.10f + 0.80f * knob[1]));
        int mx = HEAD_CX + mr, my = HEAD_CY - 6;
        circfill(mx, my, 2 + hp, CLR_WHITE);
    }
    font(FONT_TINY);
    print(knob[1] < 0.33f ? "center" : knob[1] < 0.66f ? "open" : "edge/slap", 12, 70, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);

    // readout: the copy-paste instrument() call + the current preset name
    font(FONT_SMALL);
    const char *pname = (cur_preset >= 0) ? PRESET[cur_preset].name : "(custom)";
    print(str("kit: %s", pname), 108, 26, CLR_LIGHT_YELLOW);
    print(knob[0] < 0.5f ? "tuned / pitched head" : "inharmonic / thud head", 108, 40, CLR_MEDIUM_GREY);
    print(str("%s   ring %dms", knob[2] < 0.05f ? "flat" : "pitch bend (chirp)", ring_ms()),
          108, 52, knob[2] < 0.05f ? CLR_MEDIUM_GREY : CLR_PEACH);
    font(FONT_TINY);
    print(str("instrument(I, INSTR_MEMBRANE, 1,0,7,250)  h %.2f t %.2f m %.2f",
              knob[0], knob[1], knob[2]), 108, 70, CLR_BLUE_GREEN);
    font(FONT_NORMAL);

    // ── the pads (tuned low→high)
    for (int b = 0; b < NPAD; b++) {
        int x = PAD_X(b);
        glow[b] *= 0.88f;
        int col = glow[b] > 0.5f ? CLR_LIGHT_PEACH : glow[b] > 0.1f ? CLR_DARK_PEACH : CLR_DARK_BROWN;
        rectfill(x, PAD_Y, PAD_W, PAD_H, col);
        rect(x, PAD_Y, PAD_W, PAD_H, glow[b] > 0.1f ? CLR_WHITE : CLR_DARK_RED);
        print(str("%c", STRKEY[b]), x + PAD_W / 2 - 3, PAD_Y + PAD_H - 11,
              glow[b] > 0.5f ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREY);
    }

    // ── preset row
    font(FONT_SMALL);
    for (int p = 0; p < 5; p++) {
        int x = 14 + p * 58;
        bool on = (p == cur_preset);
        print(str("%d %s", p + 1, PRESET[p].name), x, PAD_Y + PAD_H + 6, on ? CLR_YELLOW : CLR_DARK_GREY);
    }
    font(FONT_NORMAL);

    // ── macro row
    for (int k = 0; k < NSLIDER; k++) {
        int x = MROW_X(k), y = MROW_Y;
        bool on = (k == sel);
        font(FONT_SMALL);
        print(MNAME[k], x, y - 8, on ? CLR_YELLOW : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        bar(x, y, MROW_W, 7, knob[k], on ? CLR_ORANGE : CLR_DARK_PEACH, CLR_DARK_BROWN);
        font(FONT_TINY);
        print(MLO[k], x, y + 9, CLR_DARK_GREY);
        print_right(MHI[k], x + MROW_W, y + 9, CLR_DARK_GREY);
        font(FONT_NORMAL);
        if (on) print(">", x - 9, y, CLR_YELLOW);
    }

    font(FONT_TINY);
    int rx = print("A..K strike   Z/X tune   1..5 kits   ", 10, SCREEN_H - 8, CLR_DARK_GREY);
    int ax = print("SPACE roll", rx, SCREEN_H - 8, CLR_MEDIUM_GREY);
    print("   sliders: drag or arrows", ax, SCREEN_H - 8, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
