#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── TR-909 RHYTHM COMPOSER ────────────────────────────────────────────────
// Roland's TR-909 (1983) — the house and techno machine. Commercial flop
// like the 808, then "Good Life", "The Bells", "Energy Flash" and every
// warehouse since. Third sibling in the grid family (cr78 → tr808 → tr909);
// with tb303 on the shelf this completes the ReBirth RB-338 rack.
//
// The 909 is HYBRID: kick/snare/toms/rim/clap are analog circuits (same
// architecture as tr808.c, hotter values), but hats and cymbals are 6-bit
// ROM SAMPLES — a thing this engine can't play. The workaround here is
// yesterday's FM engine: INSTR_FM parked on the 3.5 ratio detent (the
// inharmonic clang — "inharmonicity, not ratio height, is what reads as
// metal") with feedback cranked, then a HIGHPASS whose cutoff starts LOW
// and RISES via a negative ENV_CUTOFF amount = the fast-closing sizzle
// burst of a sampled hat. Not a ROM playback, but it's metal, not static.
//
//   kick  — the house kick: sine, FAST +30st sweep over 35ms (the 808
//           booms, the 909 punches), plus a separate click layer on the
//           panel's famous ATTACK knob.
//   snare — two bridged-T modes (185/330Hz, same as the 808 — different
//           envelope) under brighter, longer noise. SNPY fades body↔noise.
//   toms  — sine + pitch drop + a noise CLIK at the attack.
//   rim   — short woody dual ping (the "Energy Flash" tick).
//   clap  — noise retriggered 3× ~9ms apart + a room tail.
//   hats  — FM clang through the closing highpass; closed CHOKES open,
//           which the real 909 also did (shared output stage).
//   crash — FM clang + a long noise wash (TONE knob fades clang↔wash).
//   ride  — cleaner FM ping (less feedback), the Jeff Mills 8th-note bell.
//   shuffle — PERIOD CORRECT AT LAST. The cr78/tr808 carts wear this knob
//           as an admitted anachronism; the 909 is where Roland actually
//           shipped it (after the LM-1 proved swing sells). 909-style:
//           the even 16ths drag, not the offbeat 8ths. Z/X, 50..66%.
//   accent — the TOTAL ACCENT row, red strip (orange on the 808 cart).
//   not modeled: FLAM (the panel's other trick) — a flagged step would
//           need a 3-state grid; parked until a pattern misses it.
//
//   Q W E R T Y U I O P A   play each voice by hand
//   LEFT / RIGHT  preset      UP / DOWN  tempo      SPACE  start / stop
//   Z / X  shuffle down / up
//   MOUSE  click/drag cells to edit, click the strip above for accents,
//          click a label to audition, drag the knobs (Y coarse, X fine)

// voices — named indices, never raw numbers (house rule)
enum { V_BD, V_SD, V_LT, V_MT, V_HT, V_RS, V_CP, V_CH, V_OH, V_CC, V_RC, NV };

// instrument slots (5..8 are user waves; 9..24 ours)
enum {
    SL_BD = 9, SL_BDC, SL_SDB, SL_SDN, SL_TOM, SL_TOMC,
    SL_RS, SL_CP, SL_HHC, SL_HHO, SL_CC, SL_CCN, SL_RC
};

static const char *VNAME[NV] = {
    "BASS", "SNRE", "LTOM", "MTOM", "HTOM", "RIM",
    "CLAP", "CHHH", "OPHH", "CRSH", "RIDE"
};
static const char VKEY[NV] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A'
};

#define STEPS 16

#define GX   88    // grid left edge
#define GY   46    // grid top edge
#define SX   13    // column stride
#define SY   9     // row stride

// per-voice knobs: 8×8 rotaries in the label column, same as tr808.c
#define K0X  60    // tune knob x
#define K1X  70    // decay knob x
#define K2X  80    // character knob x (voice-specific: ATTK/SNPY/CLIK/TONE)
#define KW    8    // knob size

typedef struct {
    const char *name;
    int         tempo;
    const char *row[NV];   // 16 chars, 'x' = hit; NULL = silent row
    const char *accent;    // 16 chars, 'x' = +2 volume on that step
    int         swing;     // 50 = straight .. 66 = triplet (0 = straight)
} Pat;

static const Pat PRESET[] = {
    { "GOOD LIFE", 118, {          // Inner City — the Detroit house blueprint
        [V_BD] = "x...x...x...x...",
        [V_CP] = "....x.......x...",
        [V_CH] = "x.x.x.x.x.x.x.x.",
        [V_OH] = "..x...x...x...x.",
      }, "x.......x......." },
    { "THE BELLS", 135, {          // Jeff Mills — ride-driven techno
        [V_BD] = "x...x...x...x...",
        [V_RC] = "x.x.x.x.x.x.x.x.",
        [V_SD] = "....x.......x...",
        [V_CH] = "..x...x...x...x.",
      }, "x.......x......." },
    { "ENERGY FLASH", 128, {       // Beltram — dark rolling toms + rim ticks
        [V_BD] = "x...x...x...x...",
        [V_OH] = "..x...x...x...x.",
        [V_LT] = ".......x......x.",
        [V_MT] = "..........x.....",
        [V_RS] = "..x..x....x...x.",
      }, "x.......x......." },
    { "HARDFLOOR", 130, {          // acid shuffle — the knob earns its keep
        [V_BD] = "x...x...x...x...",
        [V_CH] = "xxxxxxxxxxxxxxxx",
        [V_OH] = "..x...x...x...x.",
        [V_CP] = "....x.......x...",
        [V_RS] = "......x.......x.",
      }, "x...x...x...x...", 58 },
    { "REVOLUTION 909", 122, {     // Daft Punk — filtered french house
        [V_BD] = "x...x...x...x...",
        [V_CP] = "....x.......x...",
        [V_CH] = "x.xxx.xxx.xxx.xx",
        [V_OH] = "..x.......x.....",
        [V_RS] = ".x.....x.....x..",
      }, "x.......x......." },
    { "GABBER", 185, {             // Rotterdam — the kick IS the song
        [V_BD] = "x...x...x...x...",
        [V_SD] = "....x.......x...",
        [V_OH] = "..x...x...x...x.",
        [V_CC] = "x...............",
      }, "x...x...x...x..." },
};
#define NP ((int)(sizeof PRESET / sizeof PRESET[0]))

static int  pre      = 0;      // start on GOOD LIFE
static int  tempo    = 118;
static bool running  = true;
static int  last16   = -1;
static int  playhead = 0;
static int  flash[NV];
static bool grid[NV][STEPS];   // the live pattern — editable, loaded from preset
static bool gacc[STEPS];       // live accent row
static bool paint_val;         // what a click-drag writes (set on press)
static int  swing = 50;        // 50 = straight .. 66 = full triplet

static float ktune[NV];        // 0..1, center=0.5 → ±12 semitones
static float kdecay[NV];       // 0..1, center=0.5 → 1× scale (0→0.25×, 1→4×)
static float kcolor[NV];       // 0..1, center=0.5 → voice-specific character knob
static int   drag_v = -1, drag_k = -1;
static int   drag_y0, drag_x0;
static int   hover_v = -1, hover_k = -1;

// knob column labels — k=0,1 are the same for every voice; k=2 is per-voice
static const char *KLABEL[2] = { "TUNE", "DECAY" };
static const char *K2LABEL[NV] = {
    "ATTK",                     // BD — the famous click knob
    "SNPY",                     // SD — body↔noise fade
    "CLIK", "CLIK", "CLIK",     // toms — attack noise level
    NULL,   NULL,               // RS CP
    NULL,   NULL,               // CH OH — decay knob covers the panel's DECAY
    "TONE",                     // CC — clang↔wash fade
    NULL,                       // RC
};

// 8×8 rotary knob: circle outline + single tick pixel showing value position.
static void draw_knob(int x, int y, float val, int col) {
    int cx = x + 3, cy = y + 3;
    circ(cx, cy, 3, CLR_MEDIUM_GREY);
    float a = (135.0f + val * 270.0f) * (3.14159265f / 180.0f);
    pset(cx + (int)(cosf(a) * 2.5f + 0.5f),
         cy + (int)(sinf(a) * 2.5f + 0.5f), col);
}

// apply tune knob: ±12 semitones offset
static int k_midi(int v, int base) {
    return base + (int)((ktune[v] - 0.5f) * 24.0f + 0.5f);
}

// apply decay knob: powf scale centred on 1.0 at val=0.5 (range 0.25× to 4×)
static int k_dur(int v, int base) {
    float s = powf(4.0f, (kdecay[v] - 0.5f) * 2.0f);
    int d = (int)(base * s + 0.5f);
    return d < 5 ? 5 : d;
}

// crossfade a vol level using kcolor[v]: lo at kcolor=0, hi at kcolor=1, clamped 0..7
static int k_cv(int v, int lo, int hi) {
    int val = (int)(lo + (hi - lo) * kcolor[v] + 0.5f);
    return val < 0 ? 0 : val > 7 ? 7 : val;
}

static void load_preset(void) {
    const Pat *p = &PRESET[pre];
    for (int v = 0; v < NV; v++)
        for (int s = 0; s < STEPS; s++)
            grid[v][s] = p->row[v] && p->row[v][s] == 'x';
    for (int s = 0; s < STEPS; s++)
        gacc[s] = p->accent && p->accent[s] == 'x';
    tempo = p->tempo;
    swing = p->swing ? p->swing : 50;
    bpm(tempo);

    // reset knobs to neutral then apply preset character
    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    switch (pre) {
    case 0: // GOOD LIFE — punchy kick, tight hats, open hat breathes
        kdecay[V_BD] = 0.42f;
        kcolor[V_BD] = 0.62f;   // some click
        kdecay[V_OH] = 0.58f;
        break;
    case 1: // THE BELLS — short dry kick under a long ringing ride
        kdecay[V_BD] = 0.36f;
        kdecay[V_RC] = 0.66f;
        kcolor[V_SD] = 0.64f;   // snappy
        break;
    case 2: // ENERGY FLASH — boomier kick, dark wide toms
        kdecay[V_BD] = 0.56f;
        ktune[V_LT]  = 0.40f;
        kdecay[V_LT] = 0.60f;
        kdecay[V_MT] = 0.60f;
        break;
    case 3: // HARDFLOOR — everything tight so the shuffle reads
        kdecay[V_BD] = 0.38f;
        kdecay[V_CH] = 0.40f;
        kcolor[V_BD] = 0.70f;
        break;
    case 4: // REVOLUTION 909 — mid kick, hats do the talking
        kdecay[V_BD] = 0.46f;
        kdecay[V_CH] = 0.46f;
        kdecay[V_OH] = 0.52f;
        break;
    case 5: // GABBER — ATTACK pinned, long overdriven boom, tuned up
        kcolor[V_BD] = 1.00f;
        kdecay[V_BD] = 0.72f;
        ktune[V_BD]  = 0.58f;
        kdecay[V_CC] = 0.62f;
        break;
    }
}

static int vv(int base, int boost) {
    int v = base + boost;
    return v < 0 ? 0 : (v > 7 ? 7 : v);
}

// fire one voice `delay` ms from now. Analog voices follow the schematic
// (shared architecture with tr808.c, 909 component values); hats/cymbals
// are the FM-clang ROM stand-ins described in the header.
static void fire(int v, int boost, int delay) {
    switch (v) {
    case V_BD:  // the house kick: fast sweep body + ATTK click layer
        schedule_hit(delay, k_midi(v, 32), SL_BD, vv(6, boost), k_dur(v, 320));
        schedule_hit(delay, 60, SL_BDC, vv(k_cv(v, 0, 6), boost), 10);
        break;
    case V_SD: {  // 185Hz + 330Hz modes; SNPY fades body↔noise
        int body = k_cv(v, 8, 1), snpy = k_cv(v, 1, 8);
        schedule_hit(delay, k_midi(v, 54), SL_SDB, vv(body, boost), k_dur(v, 95));
        schedule_hit(delay, k_midi(v, 64), SL_SDB, vv(body - 1, boost), k_dur(v, 95));
        schedule_hit(delay, k_midi(v, 60), SL_SDN, vv(snpy, boost), k_dur(v, 180));
        break;
    }
    case V_LT: case V_MT: case V_HT: {  // sine drop + CLIK attack noise
        int m = v == V_LT ? 42 : v == V_MT ? 47 : 54;
        schedule_hit(delay, k_midi(v, m),  SL_TOM,  vv(5, boost), k_dur(v, 240));
        schedule_hit(delay, 70, SL_TOMC, vv(k_cv(v, 0, 5), boost), 14);
        break;
    }
    case V_RS:  // woody dual ping, lower + shorter than the 808's
        schedule_hit(delay, k_midi(v, 76), SL_RS, vv(5, boost), k_dur(v, 30));
        schedule_hit(delay, k_midi(v, 64), SL_RS, vv(3, boost), k_dur(v, 30));
        break;
    case V_CP:  // three retriggers ~9ms apart + a soft room tail
        schedule_hit(delay,      k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 9,  k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 18, k_midi(v, 60), SL_CP, vv(5, boost), 11);
        schedule_hit(delay + 26, k_midi(v, 60), SL_CP, vv(3, boost), k_dur(v, 130));
        break;
    case V_CH:  // FM clang through the closing highpass (ROM stand-in)
        schedule_hit(delay, k_midi(v, 97), SL_HHC, vv(4, boost), k_dur(v, 40));
        break;
    case V_OH:  // same metal, long burst; choked by a closed hit
        schedule_hit(delay, k_midi(v, 97), SL_HHO, vv(4, boost), k_dur(v, 400));
        break;
    case V_CC:  // TONE fades FM clang ↔ noise wash
        schedule_hit(delay, k_midi(v, 84), SL_CC,  vv(k_cv(v, 6, 1), boost), k_dur(v, 1100));
        schedule_hit(delay, k_midi(v, 60), SL_CCN, vv(k_cv(v, 1, 6), boost), k_dur(v, 1300));
        break;
    case V_RC:  // cleaner FM ping — bell + a quieter edge a fifth up
        schedule_hit(delay, k_midi(v, 76), SL_RC, vv(4, boost), k_dur(v, 600));
        schedule_hit(delay, k_midi(v, 83), SL_RC, vv(2, boost), k_dur(v, 600));
        break;
    }
}

void init(void) {
    // kick — sine body, lowpassed, +30st sweep over 35ms (faster + bigger
    // than the 808's 26/50: punch, not boom)
    instrument(SL_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(SL_BD, FILTER_LOW, 380, 2);
    instrument_env(SL_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    // kick click — the ATTACK knob layer: a 10ms highpassed noise tick
    instrument(SL_BDC, INSTR_NOISE, 0, 9, 0, 4);
    instrument_filter(SL_BDC, FILTER_HIGH, 2500, 2);

    // snare body (fired twice: 185Hz + 330Hz modes), quick settle sweep
    instrument(SL_SDB, INSTR_SINE, 0, 90, 0, 25);
    instrument_filter(SL_SDB, FILTER_LOW, 1400, 1);
    instrument_env(SL_SDB, 0, ENV_PITCH, 0, 15, 4.0f);
    // snare noise — brighter + longer than the 808's snappy
    instrument(SL_SDN, INSTR_NOISE, 0, 170, 0, 50);
    instrument_filter(SL_SDN, FILTER_HIGH, 1400, 2);

    // toms — sine with a pitch drop + a short noise click at the attack
    instrument(SL_TOM, INSTR_SINE, 0, 220, 0, 45);
    instrument_env(SL_TOM, 0, ENV_PITCH, 0, 60, 7.0f);
    instrument(SL_TOMC, INSTR_NOISE, 0, 12, 0, 5);
    instrument_filter(SL_TOMC, FILTER_BAND, 2000, 3);

    // rimshot — short woody dual ping through a low highpass
    instrument(SL_RS, INSTR_TRI, 0, 28, 0, 10);
    instrument_filter(SL_RS, FILTER_HIGH, 400, 3);

    // handclap — bandpassed noise; the retriggers in fire() make the clap
    instrument(SL_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(SL_CP, FILTER_BAND, 1200, 5);

    // ── the ROM-sample stand-ins ─────────────────────────────────────────
    // hats: INSTR_FM on the 3.5 detent (harmonics 0.55 → inharmonic clang),
    // feedback high (morph) = chaotic metal, then a highpass whose cutoff
    // STARTS 5kHz LOW and rises back (negative ENV_CUTOFF) — the sampled
    // hat's fast-closing sizzle. Closed chokes open, like the hardware.
    instrument(SL_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(SL_HHC, 0.55f);
    instrument_timbre(SL_HHC, 0.90f);
    instrument_morph(SL_HHC, 0.85f);
    instrument_filter(SL_HHC, FILTER_HIGH, 8500, 3);
    instrument_env(SL_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);

    instrument(SL_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(SL_HHO, 0.55f);
    instrument_timbre(SL_HHO, 0.90f);
    instrument_morph(SL_HHO, 0.85f);
    instrument_filter(SL_HHO, FILTER_HIGH, 8000, 3);
    instrument_env(SL_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_choke(SL_HHC, SL_HHO);   // closed hat chokes the open hat

    // crash — FM clang + a separate long noise wash (TONE knob crossfades)
    instrument(SL_CC, INSTR_FM, 0, 1000, 0, 280);
    instrument_harmonics(SL_CC, 0.55f);
    instrument_timbre(SL_CC, 1.00f);
    instrument_morph(SL_CC, 0.90f);
    instrument_filter(SL_CC, FILTER_HIGH, 4500, 2);
    instrument(SL_CCN, INSTR_NOISE, 0, 1200, 0, 320);
    instrument_filter(SL_CCN, FILTER_HIGH, 5200, 2);

    // ride — same metal, less feedback = a cleaner ringing ping
    instrument(SL_RC, INSTR_FM, 0, 550, 0, 160);
    instrument_harmonics(SL_RC, 0.55f);
    instrument_timbre(SL_RC, 0.72f);
    instrument_morph(SL_RC, 0.58f);
    instrument_filter(SL_RC, FILTER_HIGH, 5000, 2);

    for (int v = 0; v < NV; v++) { ktune[v] = 0.5f; kdecay[v] = 0.5f; kcolor[v] = 0.5f; }
    load_preset();
}

// hit-test the step grid: row -1 = accent strip, -2 = miss; col via *col
static int grid_row(int mx, int my, int *col) {
    int s = (mx - GX) / SX;
    if (mx < GX || s < 0 || s >= STEPS) return -2;
    if (my >= GY - 8 && my < GY - 1) { *col = s; return -1; }   // accent strip
    int v = (my - GY) / SY;
    if (my < GY || v < 0 || v >= NV) return -2;
    *col = s;
    return v;
}

void update(void) {
    for (int v = 0; v < NV; v++)
        if (keyp(VKEY[v])) { fire(v, 1, 0); flash[v] = 5; }

    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; }
    if (keyp(KEY_LEFT))  { pre = (pre + NP - 1) % NP; load_preset(); last16 = -1; }
    if (keyp(KEY_RIGHT)) { pre = (pre + 1) % NP;      load_preset(); last16 = -1; }
    if (keyp(KEY_UP))   { tempo += 4; if (tempo > 250) tempo = 250; bpm(tempo); }
    if (keyp(KEY_DOWN)) { tempo -= 4; if (tempo <  40) tempo =  40; bpm(tempo); }
    if (keyp('Z')) { swing -= 2; if (swing < 50) swing = 50; }
    if (keyp('X')) { swing += 2; if (swing > 66) swing = 66; }

    // knob hover + drag (Y=coarse, X=fine, same as modrack)
    int mx = mouse_x(), my = mouse_y();
    hover_v = -1; hover_k = -1;
    if (my >= GY && my < GY + NV * SY) {
        int v = (my - GY) / SY;
        if      (mx >= K0X && mx < K0X + KW)                          { hover_v = v; hover_k = 0; }
        else if (mx >= K1X && mx < K1X + KW)                          { hover_v = v; hover_k = 1; }
        else if (mx >= K2X && mx < K2X + KW && K2LABEL[v] != NULL)    { hover_v = v; hover_k = 2; }
    }
    if (mouse_pressed(MOUSE_LEFT) && hover_v >= 0) {
        drag_v = hover_v; drag_k = hover_k; drag_y0 = my; drag_x0 = mx;
    }
    if (drag_v >= 0 && mouse_down(MOUSE_LEFT)) {
        float dy = drag_y0 - my;          // up = positive = increase (coarse)
        float dx = mx     - drag_x0;      // right = positive = increase (fine)
        float delta = dy / 80.0f + dx / 600.0f;
        float *kp = drag_k == 0 ? &ktune[drag_v]
                  : drag_k == 1 ? &kdecay[drag_v]
                  :               &kcolor[drag_v];
        float nv = *kp + delta;
        *kp = nv < 0.0f ? 0.0f : nv > 1.0f ? 1.0f : nv;
        drag_y0 = my; drag_x0 = mx;
    }
    if (!mouse_down(MOUSE_LEFT)) { drag_v = -1; drag_k = -1; }

    // mouse: press toggles a cell (drag paints), label click auditions
    bool on_knob = hover_v >= 0 || drag_v >= 0;
    int mc, mr = grid_row(mx, my, &mc);
    if (mouse_pressed(MOUSE_LEFT) && !on_knob) {
        if (mr >= 0) {
            paint_val = !grid[mr][mc];
            grid[mr][mc] = paint_val;
        } else if (mr == -1) {
            paint_val = !gacc[mc];
            gacc[mc] = paint_val;
        } else if (mx < GX && my >= GY && my < GY + NV * SY) {
            int v = (my - GY) / SY;
            fire(v, 1, 0); flash[v] = 5;
        }
    } else if (mouse_down(MOUSE_LEFT) && !on_knob) {
        if (mr >= 0)       grid[mr][mc] = paint_val;
        else if (mr == -1) gacc[mc]     = paint_val;
    }

    if (!running) return;

    // sixteenth clock off beat(); steps scheduled one ahead, sample-accurate
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    if (s16 != last16) {
        bool first = (last16 < 0);
        last16  = s16;
        playhead = s16 & 15;

        for (int v = 0; v < NV; v++)
            if (grid[v][playhead]) flash[v] = 5;

        float f = beat_pos() * 4.0f; f -= (int)f;
        int step_ms = 15000 / tempo;
        int delay   = (int)((1.0f - f) * step_ms);
        int nx      = (s16 + 1) & 15;
        // 909 shuffle: the EVEN 16ths drag (the 808 cart swings 8ths —
        // an anachronism there; here it's the real 1983 panel knob)
        if (nx & 1) delay += (swing - 50) * 2 * step_ms / 100;
        int boost   = gacc[nx] ? 2 : 0;
        for (int v = 0; v < NV; v++)
            if (grid[v][nx]) fire(v, boost, delay);

        if (first) {   // fresh start: also sound the step we're already on
            int b0 = gacc[playhead] ? 2 : 0;
            for (int v = 0; v < NV; v++)
                if (grid[v][playhead]) fire(v, b0, 0);
        }
    }
}

void draw(void) {
    const Pat *p = &PRESET[pre];
    char buf[32];

    // cream face, orange stripe, black step section — the 909 look
    cls(CLR_DARK_GREY);
    rectfill(6, 8, 308, 188, CLR_LIGHT_GREY);
    rect(6, 8, 308, 188, CLR_WHITE);
    rectfill(6, 8, 308, 14, CLR_ORANGE);                  // the orange band
    rectfill(6, GY - 10, 308, NV * SY + 12, CLR_BROWNISH_BLACK);  // step section

    print("RHYTHM COMPOSER TR-909", 14, 11, CLR_BROWNISH_BLACK);
    sprintf(buf, "SHF%2d", swing);
    print(buf, 200, 11, swing > 50 ? CLR_DARK_RED : CLR_DARK_GREY);
    sprintf(buf, "%3d BPM", tempo);
    print(buf, 252, 11, CLR_BROWNISH_BLACK);
    sprintf(buf, "< %s >", p->name);
    print(buf, 14, 26, CLR_DARK_RED);
    print(running ? "PLAYING" : "STOPPED", 252, 26, running ? CLR_MEDIUM_GREEN : CLR_RED);

    // playhead column
    if (running)
        rectfill(GX + playhead * SX - 1, GY - 7, SX - 1, NV * SY + 7, CLR_DARKER_GREY);

    // accent strip (clickable) — TOTAL ACCENT, red on the 909
    for (int s = 0; s < STEPS; s++)
        rectfill(GX + s * SX, GY - 6, SX - 4, 3,
                 gacc[s] ? CLR_RED : CLR_DARK_GREY);

    for (int v = 0; v < NV; v++) {
        int y = GY + v * SY;
        sprintf(buf, "%c", VKEY[v]);
        print(buf, 14, y, flash[v] > 0 ? CLR_WHITE : CLR_ORANGE);
        print(VNAME[v], 26, y, flash[v] > 0 ? CLR_WHITE : CLR_LIGHT_GREY);
        int base_col = flash[v] > 0 ? CLR_WHITE : CLR_LIGHT_GREY;
        draw_knob(K0X, y, ktune[v],  (drag_v==v&&drag_k==0) ? CLR_YELLOW : base_col);
        draw_knob(K1X, y, kdecay[v], (drag_v==v&&drag_k==1) ? CLR_YELLOW : base_col);
        if (K2LABEL[v])
            draw_knob(K2X, y, kcolor[v], (drag_v==v&&drag_k==2) ? CLR_YELLOW : base_col);
        for (int s = 0; s < STEPS; s++) {
            int x = GX + s * SX;
            // 909 step buttons: uniform grey keys, orange when lit, the
            // first step of each quarter gets a white tick (beat marker)
            if (grid[v][s])
                rectfill(x, y, SX - 4, 7,
                         (flash[v] > 0 && s == playhead && running) ? CLR_WHITE : CLR_ORANGE);
            else
                rect(x, y, SX - 4, 7, (s & 3) == 0 ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        }
        if (flash[v] > 0) flash[v]--;
    }

    // hover label — fixed in the header band (never overlaps a knob row)
    int hv = (drag_v >= 0) ? drag_v : hover_v;
    int hk = (drag_v >= 0) ? drag_k : hover_k;
    if (hv >= 0 && hk >= 0) {
        const char *lbl = hk == 2 ? K2LABEL[hv] : KLABEL[hk];
        if (lbl) {
            char buf2[24];
            sprintf(buf2, "%s %s", VNAME[hv], lbl);
            font(FONT_SMALL);
            print(buf2, 168, 27, CLR_DARK_RED);
            font(FONT_NORMAL);
        }
    }

    print("<> PRESET ^v BPM Z/X SHFL DRAG KNOBS", 14, 186, CLR_DARK_GREY);
}
