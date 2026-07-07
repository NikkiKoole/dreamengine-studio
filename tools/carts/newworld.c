/* de:meta
{
  "title": "I Hear a New World",
  "status": "active",
  "created": "2026-07-07",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "gesture-loop",
    "granular-synth"
  ],
  "lineage": "Designed from docs/design/newworld-blind-brief.md (the Joe Meek 'Outer Space Music Fantasy' machine) — a lap-steel wired into a tape machine. Chassis: the event-loop recorder + CC-gesture stream from loopstation.c, with the theremin pad grown into a full-width scale-snapping glide ribbon.",
  "homage": "Joe Meek & the Blue Men — I Hear a New World (1959-60)",
  "description": {
    "summary": "A Joe Meek 'Outer Space Music Fantasy' box: slide a swooping lead across the night sky, loop it, and drown it in tape echo, spring reverb and wobble.",
    "detail": "One expressive gesture + one method. DRAG the big ribbon (the night sky) — x = pitch (snaps toward the scale so tunes stay in tune, glides between for the steel-guitar swoop), up/down = swell. Five held voices (a reedy LEAD saw, a Casio-CZ SWEEP, an FM GLASS bell, and TWO pure SINE theremins). Each voice loops on its OWN lane — the two sines let you loop a bed AND a lead both on sine: arm REC, play a bed on SINE, switch to SINE2 or LEAD and solo over it — every voice loops independently, clear any lane on its own (the Meek tape-layering, playable). The found-sound pads (bubble/bottle/zap/drain) and the fx rack (tape ECHO, spring reverb, tape WOBBLE, granular FREEZE + REVERSE, lo-fi GRIME) ARE the instrument. Live self-resampling — no sampler. Cold-opens playing itself; first touch hands over.",
    "controls": "Drag the ribbon = play (x pitch / y swell). 1-5 = voice (SINE + SINE2). S = scale, snap chip = glide amount. Z X C V = found-sound pads. Q echo · W spring · E freeze · R reverse · T grime · wobble chip = tape-dive. SPACE = arm loop · BACKSPACE = clear this voice's lane · ALL = clear everything."
  }
}
de:meta */
#include "studio.h"
#include <math.h>

// I Hear a New World — "a lap-steel wired into a tape machine" (see the blind brief,
// docs/design/newworld-blind-brief.md). The chassis is loopstation.c's event-loop
// recorder: one-shots record as discrete NOTE events, the ribbon records as a
// continuous CC gesture stream and replays verbatim. Here the theremin pad has grown
// into the whole night sky — a scale-snapping glide ribbon you slide a swooping lead
// across. The looper is the co-star (build a bed, solo over it — Meek's tape-layering),
// and the fx rack (self-oscillating tape echo, twangy spring reverb, tape wobble,
// granular freeze+reverse, lo-fi grime) IS the instrument. Live self-resampling via
// grains/varispeed — a tape/sampling aesthetic with no sampler.

#define TEMPO      84
#define LOOP_BEATS 8.0f          // 2 bars — a short hypnotic loop
#define MAXEV      1024

// instrument slots -----------------------------------------------------------
#define SL_LEAD   5              // ribbon voice 0 — reedy saw lead (clavioline-ish)
#define SL_SWEEP  6              // ribbon voice 1 — Casio-CZ / PD resonant "wowww" sweep
#define SL_GLASS  7              // ribbon voice 2 — FM bell / celeste glass
#define SL_SINE   8              // ribbon voice 3 — pure sine theremin
#define SL_SINE2  9              // ribbon voice 4 — a SECOND sine (own lane → two sine loops)
#define SL_BUBBLE 10             // pad 0 — straw-in-water bloop
#define SL_BOTTLE 11             // pad 1 — struck milk bottle
#define SL_ZAP    12             // pad 2 — shorted circuit
#define SL_DRAIN  13             // pad 3 — draining sink whoosh
#define NVOICE 5
#define NPAD   4
#define LANE_PERC NVOICE         // found-sound one-shots share one loop lane

static const int V_SLOT[NVOICE]   = { SL_LEAD, SL_SWEEP, SL_GLASS, SL_SINE, SL_SINE2 };
static const char *V_NAME[NVOICE] = { "LEAD", "SWEEP", "GLASS", "SINE", "SINE2" };
static const int V_COL[NVOICE]    = { CLR_ORANGE, CLR_YELLOW, CLR_MAUVE, CLR_BLUE, CLR_GREEN };

static const int  P_SLOT[NPAD] = { SL_BUBBLE, SL_BOTTLE, SL_ZAP, SL_DRAIN };
static const int  P_MIDI[NPAD] = { 84, 67, 72, 50 };
static const int  P_VOL[NPAD]  = { 5, 6, 6, 5 };
static const int  P_DUR[NPAD]  = { 60, 260, 90, 320 };
static const char *P_NAME[NPAD] = { "BUBBLE", "BOTTLE", "ZAP", "DRAIN" };
static const char  P_KEY[NPAD]  = { 'Z', 'X', 'C', 'V' };

// the ribbon (the night sky) -------------------------------------------------
#define RX 6
#define RY 16
#define RW 308
#define RH 88
#define NDEG 15                  // scale degrees spanned across the width
#define BASEOCT 3

// scales + snap
static const int  SCALES[]      = { SCALE_PENTA_MIN, SCALE_PENTA, SCALE_MAJOR, SCALE_CHROMATIC };
static const char *SCALE_NAME[] = { "MIN PENT", "MAJ PENT", "MAJOR", "CHROMA" };
static int   scale_i = 0;
static const float SNAP_AMT[]   = { 0.85f, 0.5f, 0.0f, 1.0f };
static const char *SNAP_NAME[]  = { "SOFT", "LOOSE", "FREE", "KEYED" };
static int   snap_i = 0;

// events + transport ---------------------------------------------------------
enum { EV_NOTE, EV_CC, EV_OFF };
typedef struct { float pos, pitch; int kind, slot, vol, dur, aux, lane, gx, gy; } Ev;
static Ev  ev[MAXEV];
static int nev;
static int armed;                       // loop record on

static float songb, lp, prev_lp;
static int   loop_i;

// ribbon voices: one live + one replay ghost per voice-type
static int vlive[NVOICE], vrep[NVOICE];
#define NOID (-999)
static int rib_id = NOID;               // finger claimed by the ribbon
static int rib_px, rib_py, rib_on;
static float rec_p; static int rec_v;   // CC delta filter
static int cur;                         // selected ribbon voice
static float gho_p; static int gho_v, gho_on, gho_voice; // replay draw state

// fx state -------------------------------------------------------------------
static int fx_echo = 1;                 // 0 off / 1 slap / 2 dub
static int fx_spring = 1;               // 0 off / 1 room / 2 hall
static int fx_freeze, fx_reverse, fx_grime, fx_wobble;
static float vspeed = 1.0f;
static int   fx_hash = -1;              // reconfigure only when this changes

// self-play + visuals --------------------------------------------------------
static int   playing_self = 1;
static float energy;                    // amplitude for the reactive creatures
static int   pad_flash[NPAD];

// ---------------------------------------------------------------------------
// ribbon mapping: x -> a scale-snapped float midi, y -> swell volume
static float ribbon_midi(int x) {
    float c = clamp(remap((float)x, RX, RX + RW, 0, NDEG), 0, NDEG - 0.001f);
    float nearest = roundf(c);
    c = c + (nearest - c) * SNAP_AMT[snap_i];       // pull toward the scale degree
    int lo = (int)floorf(c); if (lo > NDEG - 1) lo = NDEG - 1;
    float fr = c - lo;
    float a = (float)degree(SCALES[scale_i], BASEOCT, lo);
    float b = (float)degree(SCALES[scale_i], BASEOCT, lo + 1);
    return a + (b - a) * fr;
}
static float ribbon_vol(int y) {
    return clamp(remap((float)y, RY + 4, RY + RH - 4, 7.0f, 1.6f), 0.6f, 7.0f); // top = loud
}
static int midi_to_x(float m) {                     // inverse-ish, for drawing ghosts
    float a = (float)degree(SCALES[scale_i], BASEOCT, 0);
    float b = (float)degree(SCALES[scale_i], BASEOCT, NDEG);
    return (int)remap(m, a, b, RX, RX + RW);
}

// ---------------------------------------------------------------------------
static void rec_ev(int kind, int slot, float pitch, int vol, int dur, int aux, int lane, int gx, int gy) {
    if (!armed || nev >= MAXEV) return;
    ev[nev++] = (Ev){ lp, pitch, kind, slot, vol, dur, aux, lane, gx, gy };
}

static void fire_ev(Ev *e) {
    if (e->kind == EV_NOTE) {
        hit((int)e->pitch, e->slot, e->vol, e->dur);
        for (int p = 0; p < NPAD; p++) if (P_SLOT[p] == e->slot) pad_flash[p] = 5;
    } else if (e->kind == EV_CC) {
        note_pitch(vrep[e->aux], e->pitch); note_vol(vrep[e->aux], (float)e->vol);
        gho_p = e->pitch; gho_v = e->vol; gho_on = 1; gho_voice = e->aux;
    } else {  // EV_OFF
        note_vol(vrep[e->aux], 0); gho_on = 0;
    }
}

static void fire_replay(void) {
    int wrap = lp < prev_lp;
    for (int i = 0; i < nev; i++) {
        Ev *e = &ev[i];
        int now = wrap ? (e->pos > prev_lp || e->pos <= lp)
                       : (e->pos > prev_lp && e->pos <= lp);
        if (now) fire_ev(e);
    }
}

static void clear_all(void) {
    nev = 0;
    for (int v = 0; v < NVOICE; v++) note_vol(vrep[v], 0);
    gho_on = 0;
}
static void clear_lane(int L) {                 // clear one voice's loop lane
    int w = 0;
    for (int i = 0; i < nev; i++) if (ev[i].lane != L) ev[w++] = ev[i];
    nev = w;
    if (L < NVOICE) note_vol(vrep[L], 0);
    gho_on = 0;
}
static int has_lane(int L) {
    for (int i = 0; i < nev; i++) if (ev[i].lane == L) return 1;
    return 0;
}

// reconfigure the master fx section — only when a chip changed (set-and-hold).
static void apply_fx(void) {
    // echo (send bus): route the voices, set the tail
    float esend = fx_echo ? (fx_echo == 2 ? 0.55f : 0.3f) : 0.0f;
    echo((int)(60000.0f / TEMPO * 0.75f), fx_echo == 2 ? 0.72f : 0.5f, 0.45f);
    // reverb (send bus): the twangy spring, drenched
    float rsend = fx_spring ? (fx_spring == 2 ? 0.75f : 0.42f) : 0.0f;
    reverb(fx_spring == 2 ? 0.86f : 0.58f, 0.32f);
    for (int i = SL_LEAD; i <= SL_DRAIN; i++) {
        instrument_echo(i, esend);
        instrument_reverb(i, rsend);
    }
    // lo-fi grime: bitcrush + tape saturation + valve hiss/hum + pumping glue
    crush(fx_grime ? 8.0f : 16.0f, fx_grime ? 3.0f : 1.0f, fx_grime ? 0.45f : 0.0f);
    tape(fx_grime ? 0.28f : 0.0f, fx_grime ? 0.22f : 0.0f, fx_grime ? 0.5f : 0.0f);
    amp_noise(fx_grime ? 0.16f : 0.0f, fx_grime ? 0.11f : 0.0f, 50);
    glue(0, fx_grime ? 0.5f : 0.0f, 8, 150);
    // granular self-resampling — only when a FREEZE/REVERSE grab is engaged
    int gr = fx_freeze || fx_reverse;
    grains(140, 16, 0.9f, 0.3f, 0.3f, gr ? 0.55f : 0.0f);
    grains_freeze(fx_freeze);
    grains_pitch(0, 0.25f, fx_reverse);
}
static void sync_fx(void) {
    int h = fx_echo | (fx_spring << 2) | (fx_freeze << 4) | (fx_reverse << 5) | (fx_grime << 6);
    if (h != fx_hash) { apply_fx(); fx_hash = h; }
}

// ---------------------------------------------------------------------------
void init(void) {
    bpm(TEMPO);
    // ribbon voice 0 — reedy saw lead, holds while dragged (the clavioline swoop)
    instrument(SL_LEAD, INSTR_SAW, 22, 220, 6, 320);
    instrument_filter(SL_LEAD, FILTER_LOW, 1500, 5);
    instrument_lfo(SL_LEAD, 0, LFO_PITCH, 5.0f, 0.16f);
    // ribbon voice 1 — Casio-CZ phase-distortion resonant "wowww" sweep (holds)
    instrument(SL_SWEEP, INSTR_PD, 26, 240, 6, 340);
    instrument_harmonics(SL_SWEEP, 0.82f);      // a resonant wavetype detent
    instrument_timbre(SL_SWEEP, 0.55f);         // resonant-peak brightness
    instrument_morph(SL_SWEEP, 0.6f);           // the CZ DCW sweep
    instrument_lfo(SL_SWEEP, 0, LFO_PITCH, 5.2f, 0.14f);
    // ribbon voice 2 — FM bell / celeste glass (bright ping, then holds glassy)
    instrument(SL_GLASS, INSTR_FM, 4, 260, 5, 360);
    instrument_harmonics(SL_GLASS, 0.42f);      // off-integer ratio = inharmonic bell
    instrument_timbre(SL_GLASS, 0.5f);          // brightness
    instrument_morph(SL_GLASS, 0.15f);          // mostly clean
    // ribbon voice 3 — pure sine theremin, wide vibrato
    instrument(SL_SINE, INSTR_SINE, 40, 180, 7, 320);
    instrument_lfo(SL_SINE, 0, LFO_PITCH, 5.6f, 0.30f);
    // ribbon voice 4 — a SECOND sine, identical (its own lane = two sine loops)
    instrument(SL_SINE2, INSTR_SINE, 40, 180, 7, 320);
    instrument_lfo(SL_SINE2, 0, LFO_PITCH, 5.6f, 0.30f);
    for (int v = 0; v < NVOICE; v++) {
        vlive[v] = note_on(60, V_SLOT[v], 0); note_glide(vlive[v], 70);
        vrep[v]  = note_on(60, V_SLOT[v], 0); note_glide(vrep[v], 70);
    }
    // found-sound pads (one-shots)
    instrument(SL_BUBBLE, INSTR_SINE, 0, 70, 0, 40);
    instrument_env(SL_BUBBLE, 0, ENV_PITCH, 0, 60, 14);          // a little "bloop" up
    instrument(SL_BOTTLE, INSTR_MALLET, 0, 320, 0, 220);
    instrument(SL_ZAP, INSTR_SAW, 0, 120, 0, 60);
    instrument_env(SL_ZAP, 0, ENV_PITCH, 0, 110, -22);           // pitch falls away
    instrument(SL_DRAIN, INSTR_NOISE, 5, 420, 0, 220);
    instrument_filter(SL_DRAIN, FILTER_BAND, 1400, 4);
    instrument_env(SL_DRAIN, 0, ENV_CUTOFF, 0, 400, -1100);      // whoosh down

    sync_fx();    // drenched from the first note (echo + spring + grime on)
}

// self-play cold-open: a slow wandering line on the SINE voice until first input
static const int SELF_MOTIF[8] = { 0, 2, 3, 2, 5, 3, 1, -1 };
static int self_step = -1;

static void self_play(void) {
    cur = 3;                                    // the sine theremin drifts
    if ((int)songb != self_step) {              // advance once per beat
        self_step = (int)songb;
    }
    float ph = songb * 0.5f;
    int idx = ((int)(ph)) % 8;
    float base = (float)degree(SCALES[scale_i], BASEOCT + 1, SELF_MOTIF[idx < 0 ? 0 : idx] + 4);
    float wobble = sinf(songb * 1.3f) * 1.4f;   // a swooping bend between steps
    note_pitch(vlive[3], base + wobble);
    float sw = 2.6f + 2.0f * (0.5f + 0.5f * sinf(songb * 0.8f));
    note_vol(vlive[3], sw);
    energy = energy * 0.9f + sw * 0.1f;
}

static void handover(void) {
    playing_self = 0;
    for (int v = 0; v < NVOICE; v++) note_vol(vlive[v], 0);
    cur = 0;
}

// ---------------------------------------------------------------------------
void update(void) {
    songb  = beat() + beat_pos();
    lp     = fmodf(songb, LOOP_BEATS);
    loop_i = (int)(songb / LOOP_BEATS);
    fire_replay();

    // ---- any input hands over from the self-playing intro ----
    if (playing_self) {
        if (touch_count()) handover();          // mouse/touch; the keyp handlers below also hand over
        else self_play();
    }

    // ---- voice / scale / snap chips ----
    for (int v = 0; v < NVOICE; v++)
        if (keyp('1' + v) || tapp(6 + v * 32, 108, 30, 14)) { if (playing_self) handover(); cur = v; }
    if (keyp('S') || tapp(170, 108, 66, 14)) scale_i = (scale_i + 1) % 4;
    if (tapp(240, 108, 74, 14)) snap_i = (snap_i + 1) % 4;

    // ---- found-sound pads ----
    for (int p = 0; p < NPAD; p++) {
        if (keyp(P_KEY[p]) || tapp(6 + p * 77, 126, 73, 20)) {
            if (playing_self) handover();
            hit(P_MIDI[p], P_SLOT[p], P_VOL[p], P_DUR[p]);
            rec_ev(EV_NOTE, P_SLOT[p], P_MIDI[p], P_VOL[p], P_DUR[p], p, LANE_PERC, 0, 0);
            pad_flash[p] = 6;
        }
    }

    // ---- fx rack chips (set-and-hold) ----
    if (keyp('Q') || tapp(6,   150, 48, 14)) fx_echo    = (fx_echo + 1) % 3;
    if (keyp('W') || tapp(56,  150, 48, 14)) fx_spring  = (fx_spring + 1) % 3;
    if (            tapp(106, 150, 48, 14)) fx_wobble  ^= 1;                 // WOBBLE (tape dive)
    if (keyp('E') || tapp(156, 150, 48, 14)) fx_freeze  ^= 1;
    if (keyp('R') || tapp(206, 150, 48, 14)) fx_reverse ^= 1;
    if (keyp('T') || tapp(256, 150, 48, 14)) fx_grime   ^= 1;
    sync_fx();
    // varispeed is a live-swept master stage (glides on its own) — ride it every frame
    float vtarget = fx_wobble ? 0.55f : 1.0f;
    vspeed += (vtarget - vspeed) * 0.08f;
    varispeed(vspeed);

    // ---- looper transport chips ----
    if (keyp(KEY_SPACE) || tapp(6, 168, 56, 14)) armed ^= 1;
    if (keyp(KEY_BACKSPACE) || tapp(66, 168, 52, 14)) clear_lane(cur);   // clear THIS voice's lane
    if (tapp(122, 168, 44, 14)) clear_all();

    // ---- the ribbon: claim a finger, drag it. x = pitch, y = swell ----
    int found = 0;
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        if (id == rib_id) { found = 1; rib_px = tx; rib_py = ty; break; }
        if (rib_id == NOID && tx >= RX && tx < RX + RW && ty >= RY && ty < RY + RH) {
            rib_id = id; found = 1; rib_px = tx; rib_py = ty; break;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++)
        if (touch_ended_id(i) == rib_id) rib_id = NOID;
    int inrib = found && rib_px >= RX && rib_px < RX + RW && rib_py >= RY && rib_py < RY + RH;
    if (inrib) {
        float p = ribbon_midi(rib_px);
        float v = ribbon_vol(rib_py);
        note_pitch(vlive[cur], p); note_vol(vlive[cur], v);
        energy = energy * 0.85f + v * 0.15f;
        if (!rib_on || fabsf(p - rec_p) > 0.1f || (int)v != rec_v) {
            rec_ev(EV_CC, V_SLOT[cur], p, (int)v, 0, cur, cur, rib_px, rib_py);
            rec_p = p; rec_v = (int)v;
        }
        rib_on = 1;
    } else if (rib_on) {
        note_vol(vlive[cur], 0);
        rec_ev(EV_OFF, V_SLOT[cur], 0, 0, 0, cur, cur, 0, 0);
        rib_on = 0;
    }

    energy *= 0.96f;
    for (int p = 0; p < NPAD; p++) if (pad_flash[p] > 0) pad_flash[p]--;

#ifdef DE_TRACE
    watch("lp", "%.2f pass=%d ev=%d", lp, loop_i, nev);
    watch("cur", "%d self=%d speed=%.2f", cur, playing_self, vspeed);
#endif
    prev_lp = lp;
}

// ---------------------------------------------------------------------------
static void chip(int x, int y, int w, const char *lbl, int on, int col) {
    rectfill(x, y, w, 14, on ? col : CLR_DARKER_BLUE);
    rect(x, y, w, 14, on ? CLR_WHITE : CLR_DARK_BLUE);
    font(FONT_SMALL);
    print(lbl, x + 3, y + 4, on ? CLR_BLACK : CLR_MEDIUM_GREY);
    font(FONT_NORMAL);
}

static void draw_creature(int x, int y, int r, int col) {
    circfill(x, y, r, col);
    circ(x, y, r, CLR_WHITE);
    int e = r > 6 ? 2 : 1;
    circfill(x - r/2, y - 1, e, CLR_BLACK);
    circfill(x + r/2, y - 1, e, CLR_BLACK);
}

void draw(void) {
    cls(CLR_BLACK);

    // ---- title bar ----
    print("I HEAR A NEW WORLD", RX, 4, CLR_LIGHT_PEACH);
    print_right(str("%d BPM", TEMPO), 314, 4, CLR_INDIGO);

    // ---- the ribbon = the night sky ----
    vgradient(RX, RY, RW, RH, CLR_DARKER_PURPLE, CLR_BLACK);
    // stars (deterministic, gently twinkling)
    for (int i = 0; i < 46; i++) {
        int sx = RX + (i * 61) % RW, sy = RY + 2 + (i * 37) % (RH - 20);
        int tw = (i * 7 + (int)(now() * 2)) % 5;
        pset(sx, sy, tw < 2 ? CLR_DARK_GREY : (tw == 4 ? CLR_WHITE : CLR_LIGHT_GREY));
    }
    // scale-degree tick marks along the foot of the sky
    for (int d = 0; d <= NDEG; d++) {
        int tx = RX + (int)((float)d / NDEG * RW);
        line(tx, RY + RH - 5, tx, RY + RH - 2, CLR_DARKER_GREY);
    }
    // low lunar horizon
    int hy = RY + RH - 6;
    for (int x = RX; x < RX + RW; x++) {
        int h = (int)(2.5f * sinf(x * 0.05f) + 1.5f * sinf(x * 0.13f));
        pset(x, hy - h, CLR_DARK_BLUE);
        line(x, hy - h + 1, x, RY + RH - 1, CLR_DARKER_BLUE);
    }
    rect(RX, RY, RW, RH, playing_self ? CLR_DARK_BLUE : (rib_on ? CLR_GREEN : CLR_DARK_BLUE));

    // floating creatures (bob + react to loudness) — the Globbots
    int lift = (int)(energy * 2.2f);
    draw_creature(RX + 60  + (int)(6 * sinf(now() * 0.9f)),  RY + 34 - lift + (int)(4 * sinf(now() * 1.3f)), 5 + (int)(energy * 0.5f), CLR_BLUE);
    draw_creature(RX + 180 + (int)(7 * sinf(now() * 0.7f)),  RY + 26 - lift + (int)(5 * sinf(now() * 1.1f)), 4 + (int)(energy * 0.4f), CLR_PINK);
    draw_creature(RX + 250 + (int)(5 * sinf(now() * 1.2f)),  RY + 46 - lift + (int)(4 * sinf(now() * 0.9f)), 6 + (int)(energy * 0.5f), CLR_MAUVE);

    // recorded gesture ghosts (faint comet trail)
    for (int i = 0; i < nev; i++)
        if (ev[i].kind == EV_CC) pset(ev[i].gx, ev[i].gy, CLR_DARK_GREY);
    if (gho_on) {
        int gx = midi_to_x(gho_p);
        int gy = (int)remap((float)gho_v, 7.0f, 1.6f, RY + 4, RY + RH - 4);
        circ((int)mid(RX+2, gx, RX+RW-3), (int)mid(RY+2, gy, RY+RH-3), 3, V_COL[gho_voice]);
    }
    // your hand — a bright comet crosshair
    if (rib_on) {
        line(rib_px - 5, rib_py, rib_px + 5, rib_py, CLR_WHITE);
        line(rib_px, rib_py - 5, rib_px, rib_py + 5, CLR_WHITE);
        circ(rib_px, rib_py, 4, V_COL[cur]);
    }
    if (playing_self && blink(30)) {
        font(FONT_SMALL);
        print("drag the sky", RX + RW/2 - 22, RY + RH/2, CLR_LIGHT_GREY);
        font(FONT_NORMAL);
    }

    // ---- voice / scale / snap row ----
    for (int v = 0; v < NVOICE; v++) {
        int x = 6 + v * 32;
        chip(x, 108, 30, V_NAME[v], cur == v && !playing_self, V_COL[v]);
        if (has_lane(v)) circfill(x + 26, 111, 2, blink(20) ? CLR_RED : CLR_DARK_PURPLE); // looped
    }
    chip(170, 108, 66, str("S:%s", SCALE_NAME[scale_i]), 1, CLR_DARK_GREEN);
    chip(240, 108, 74, str("GLIDE:%s", SNAP_NAME[snap_i]), 1, CLR_DARK_GREEN);

    // ---- found-sound pads ----
    for (int p = 0; p < NPAD; p++) {
        int x = 6 + p * 77;
        rectfill(x, 126, 73, 20, pad_flash[p] ? CLR_LIGHT_PEACH : CLR_DARKER_BLUE);
        rect(x, 126, 73, 20, CLR_DARK_BLUE);
        font(FONT_SMALL);
        print(P_NAME[p], x + 4, 129, pad_flash[p] ? CLR_BLACK : CLR_MAUVE);
        print(str("%c", P_KEY[p]), x + 4, 137, pad_flash[p] ? CLR_BLACK : CLR_DARK_GREY);
        font(FONT_NORMAL);
    }

    // ---- fx rack ----
    static const char *ECHO_L[3] = { "ECHO", "SLAP q", "DUB q" };
    static const char *SPR_L[3]  = { "SPRING", "ROOM w", "HALL w" };
    chip(6,   150, 48, ECHO_L[fx_echo],   fx_echo,   CLR_YELLOW);
    chip(56,  150, 48, SPR_L[fx_spring],  fx_spring, CLR_LIGHT_GREY);
    chip(106, 150, 48, "WOBBLE",          fx_wobble, CLR_INDIGO);
    chip(156, 150, 48, "FREEZE e",        fx_freeze, CLR_BLUE);
    chip(206, 150, 48, "RVRSE r",         fx_reverse,CLR_PINK);
    chip(256, 150, 48, "GRIME t",         fx_grime,  CLR_BROWN);

    // ---- looper transport ----
    chip(6,  168, 56, armed ? "REC on" : "REC spc", armed, CLR_RED);
    if (armed && blink(16)) circfill(54, 175, 2, CLR_WHITE);
    chip(66, 168, 52, str("CLR %s", V_NAME[cur]), 0, CLR_DARKER_BLUE);
    chip(122, 168, 40, "ALL", 0, CLR_DARKER_BLUE);
    // loop progress bar
    int bx = 168, bw = 146;
    rect(bx, 170, bw, 10, CLR_DARK_BLUE);
    int ph = (int)(lp / LOOP_BEATS * (bw - 2));
    rectfill(bx + 1, 171, ph, 8, nev ? CLR_DARK_GREEN : CLR_DARKER_BLUE);
    line(bx + 1 + ph, 170, bx + 1 + ph, 179, CLR_WHITE);
    font(FONT_SMALL);
    print_right(str("%d evts", nev), bx + bw, 172, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // ---- HUD ----
    font(FONT_SMALL);
    print("drag sky: x=pitch y=swell   1-5 pick voice   Z-V sounds   REC loops each voice on its lane", RX, 186, CLR_INDIGO);
    font(FONT_NORMAL);
}
