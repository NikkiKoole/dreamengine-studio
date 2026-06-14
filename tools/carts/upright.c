#include "studio.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>

// ── UPRIGHT BASS ─────────────────────────────────────────────────────────────
// The jazz double bass, played the way you actually play one: the FINGERBOARD is
// the instrument. Four strings (E A D G). Press to sound a note; slide LEFT/RIGHT
// to walk the frets (each semitone re-articulates — clean up AND down); PULL the
// string off its line (either way) to bend the pitch continuously UP — displacing a
// string raises its tension, and the waveguide can only bend up anyway (down-bends
// stick — verified — so the fret walk covers descending notes); lift to damp.
// Or start a drag in the GAP next to a string and sweep THROUGH it to PICK it (press
// on a string = grab/fret; press in open space = pick). Mono, last-note-wins.
//
// The right hand is an ARTICULATION switch, and pizz + arco are the SAME string
// model (INSTR_BOWED) — instrument_mode(slot,0,1) plucks it, =0 bows it, just like the
// real instrument where the only difference is fingers vs a bow:
//   PIZZ  the jazz sound — a woody finger pluck that rings and thumps (default)
//   ARCO  drawn with the bow — slow attack, sings while held, left-hand vibrato
//   SLAP  pizz + the string slapping the fingerboard (rockabilly thump)
//
// Taps snap to a clean semitone (fretless is unforgiving); drags stay continuous
// so slides are smooth. Play the fingerboard with mouse / touch, or the computer
// keyboard (GarageBand map, A = E1; Z / X shift the octave). TONE = mic darkness,
// RING = how long a pizz note rings after you lift.

#define BASS  5                            // INSTR_BOWED, pizzicato (instrument_mode pluck)
#define ARCOS 6                            // INSTR_BOWED, bowed
#define CLICK 7                            // INSTR_NOISE — the slap / knuckle transient
#define BODY  8                            // INSTR_MEMBRANE — slapping the bass body

enum { PIZZ, ARCO, SLAP };
static const char *MNAME[3] = { "PIZZ", "ARCO", "SLAP" };

// fingerboard geometry
#define STRIP_H  34                        // control strip height
#define NECK_X0  34                        // the nut (open string)
#define NECK_X1  270                       // the bridge (wood body to its right)
#define SPAN     12                        // semitones from nut to bridge (an octave)
#define NLANE    4
#define LANE_H   ((SCREEN_H - STRIP_H) / NLANE)

// lanes top→bottom = G D A E (high string on top)
static const int   SBASE[NLANE] = { 43, 38, 33, 28 };   // G2 D2 A1 E1
static const char *SLAB [NLANE] = { "G", "D", "A", "E" };

// ── the one mono voice ──
#define NONE  (-999)
#define KBD   (-2)
#define MOUSE (-3)
static int   b_handle = -1;
static int   b_owner  = NONE;              // touch id, KBD, or NONE
static int   b_lane   = 3;
static float b_midi   = 28;                // current sounding pitch (float, for slides)
static int   b_kbsemi = -1;                // which keyboard semitone owns the voice
static int   b_mode   = PIZZ;
static float b_wob    = 0;                 // string-vibration phase (cosmetic)
static int   b_press_x, b_press_y;         // where the finger landed (snap-until-move + bend zero)
static bool  b_sliding;                    // has the finger moved enough to play live?
enum { GRAB, PICK };                       // press ON a string = grab/fret; press in OPEN space = pick
static int   b_gesture;
static float b_fret_midi;                  // the stopped pitch currently sounding (re-articulated on change)
static int   b_prevy;                      // last frame's finger y (for pick string-crossing)
static int   b_fx, b_fy;                   // finger screen position (drives the string-bend visual)
#define BEND_K   0.05f                      // semitones of pitch bend per pixel pulled
#define BEND_MAX 2.0f                       // ...clamped to ±a whole tone

// GarageBand musical-typing map (house layout) — A = the low root
static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
static char KC[18]; static int KS[18]; static int NK;
#define KB_ROOT 28                         // 'A' = E1 at octave 0
static int octv = 1;                       // Z/X, clamped 0..3

// body-slap impact ripples (cosmetic)
typedef struct { int x, y, life, max; } Rip;
static Rip rips[6];

static float k_tone = 0.45f;               // TONE -> lowpass cutoff
static float k_ring = 0.55f;               // RING -> pizz release (damp tail)

static int   tone_hz (void) { return 500 + (int)(k_tone * 2200); }   // 500..2700 Hz
static int   ring_ms (void) { return 90 + (int)(k_ring * 700); }      // 90..790 ms
static const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

static void setup_pizz(void) {             // RING knob re-tunes the pizz release live
    instrument(BASS, INSTR_BOWED, 4, 0, 7, ring_ms());
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);               // PIZZICATO: pluck the string instead of bowing
    instrument_filter(BASS, FILTER_LOW, tone_hz(), 3);
    instrument_harmonics(BASS, 0.30f);     // dark bow position (round, woody)
    instrument_timbre(BASS, 0.30f);        // warm string
    instrument_morph(BASS, 0.45f);
}

void init(void) {
    NK = 0;
    for (int i = 0; i < 11; i++) { KC[NK] = gb_wkey[i]; KS[NK] = gb_wsemi[i]; NK++; }
    for (int i = 0; i < 7;  i++) { KC[NK] = gb_bkey[i]; KS[NK] = gb_bsemi[i]; NK++; }

    setup_pizz();

    // ARCO — the bow: slow speak, sustains while held, gentle left-hand vibrato
    instrument(ARCOS, INSTR_BOWED, 110, 0, 7, 260);
    instrument_mode(ARCOS, MODE_BOW_PIZZ, 0.0f);              // bowed (self-oscillating, holds)
    instrument_filter(ARCOS, FILTER_LOW, tone_hz() + 400, 4);
    instrument_harmonics(ARCOS, 0.40f);
    instrument_timbre(ARCOS, 0.45f);       // a little bow grain
    instrument_morph(ARCOS, 0.50f);
    instrument_lfo(ARCOS, 0, LFO_PITCH, 5.0f, 0.12f);   // vibrato

    // SLAP — the string snapping back onto the fingerboard (also the knuckle on the body)
    instrument(CLICK, INSTR_NOISE, 0, 28, 0, 16);
    instrument_filter(CLICK, FILTER_LOW, 2000, 2);

    // BODY — slapping the wood: a struck drumhead standing in for the body cavity
    instrument(BODY, INSTR_MEMBRANE, 0, 170, 0, 60);
    instrument_harmonics(BODY, 0.45f);     // woody, a touch inharmonic
    instrument_morph(BODY, 0.0f);          // no pitch-bend gliss
    instrument_filter(BODY, FILTER_LOW, 2200, 1);

    reverb(0.30f, 0.55f);                  // a small warm room
    instrument_reverb(BASS, 0.16f);
    instrument_reverb(ARCOS, 0.18f);
}

static int   slot_for(int mode) { return mode == ARCO ? ARCOS : BASS; }
static float midi_of (int lane, float pos) { return SBASE[lane] + pos; }

static int   lane_at(int y) { int l = (y - STRIP_H) / LANE_H; return l < 0 ? 0 : (l > 3 ? 3 : l); }
static int   cy_of(int lane) { return STRIP_H + lane * LANE_H + LANE_H / 2; }   // string center line
#define GRAB_PX 13                         // press within this of a string = grab it; else a pick
static float pos_at (int x) { return clamp((float)(x - NECK_X0) / (NECK_X1 - NECK_X0), 0, 1) * SPAN; }

static void damp(void) {
    if (b_handle >= 0) note_off(b_handle);
    b_handle = -1; b_owner = NONE; b_kbsemi = -1;
}

// slap the wood. Same body voice everywhere (one resonant character), but WHERE you
// hit changes it: the belly (right) booms low + thumpy; the neck/scroll (left) is a
// drier, higher knock. Vertical position picks the pitch within that.
static void body_hit(int x, int y) {
    bool belly = x > NECK_X1;
    float yt = clamp((float)(y - STRIP_H) / (SCREEN_H - STRIP_H), 0, 1);   // 0 top .. 1 bottom
    int midi = belly ? 45 - (int)(yt * 14)     // 45→31, a deep boom
                     : 60 - (int)(yt * 12);    // 60→48, a tighter knock
    instrument_timbre(BODY, belly ? 0.22f : 0.68f);   // center thump vs edge slap
    hit(midi, BODY, 7, belly ? 130 : 70);
    hit(midi + 24, CLICK, belly ? 6 : 7, belly ? 22 : 14);   // the knuckle transient (the punch)
    for (int i = 0; i < 6; i++) if (rips[i].life <= 0) {     // an impact ripple
        rips[i] = (Rip){ x, y, belly ? 16 : 11, belly ? 16 : 11 }; break;
    }
}

// strike a note at the stopped pitch (correct attack). Mono — replaces what's sounding.
static void play(int owner, int lane, float pos) {
    if (b_handle >= 0) note_off(b_handle);          // stop the old note (last-note-wins)
    b_owner = owner; b_lane = lane; b_midi = midi_of(lane, pos); b_wob = 0;
    if (b_mode == SLAP) hit((int)(b_midi + 0.5f), CLICK, 5, 22);
    b_handle = note_on((int)(b_midi + 0.5f), slot_for(b_mode), b_mode == ARCO ? 4 : 7);
    note_pitch(b_handle, b_midi);
    note_glide(b_handle, 70);                        // up-bends/slides slur, don't step
    b_fret_midi = b_midi;
    b_fx = NECK_X0 + (int)((NECK_X1 - NECK_X0) * pos / SPAN);
    b_fy = cy_of(lane);
}

// move to a new fret (left/right): re-articulate at the new semitone. Clean in BOTH
// directions because each fret is a fresh attack — a waveguide string can only bend UP,
// never below its pluck pitch (verified), so a continuous DOWN-slide isn't possible.
static void fret_to(float fmidi) {
    if (fmidi == b_fret_midi) return;
    note_off(b_handle);
    b_handle = note_on((int)(fmidi + 0.5f), slot_for(b_mode), b_mode == ARCO ? 4 : 7);
    note_glide(b_handle, 60);
    b_fret_midi = fmidi;
}

void update(void) {
    // ── fingerboard: press to pluck, drag to SLIDE, release to damp ──
    // (the mouse API drives one pointer — desktop mouse, or a phone tap-as-mouse;
    //  a bass is monophonic so a single pointer is all it needs)
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && my >= STRIP_H) {
        if (mx < NECK_X0 - 6 || mx > NECK_X1 + 6) {
            body_hit(mx, my);                          // the wood → percussion
        } else {
            int near = 0, nd = 9999;                   // nearest string to the press
            for (int l = 0; l < NLANE; l++) { int d = abs(my - cy_of(l)); if (d < nd) { nd = d; near = l; } }
            if (nd <= GRAB_PX) {                       // pressed ON a string → GRAB it (fret + bend)
                play(MOUSE, near, roundf(pos_at(mx)));
                b_gesture = GRAB; b_fx = mx; b_fy = my;
            } else {                                   // pressed in OPEN space → a PICK (sweep to pluck)
                b_owner = MOUSE; b_gesture = PICK;
            }
            b_press_x = mx; b_press_y = my; b_prevy = my; b_sliding = false;
        }
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == GRAB) {
        if (!b_sliding && (abs(mx - b_press_x) > 4 || abs(my - b_press_y) > 4)) b_sliding = true;
        if (b_sliding) {
            int dpx = b_press_y - my;                            // vertical pull (+up / -down)
            // While you're actively PULLING, freeze the fret — only the up/down bend matters,
            // so a little sideways drift won't re-pluck and interrupt the bend. Near the rest
            // line (not pulling) the finger is free to walk the frets left/right.
            if (abs(dpx) <= 6) {
                float fret = clamp(roundf(pos_at(mx)), 0, SPAN);
                fret_to(SBASE[b_lane] + fret);
            }
            float vbend = clamp(fabsf((float)dpx) * BEND_K, 0, BEND_MAX);
            b_midi = b_fret_midi + vbend;                        // pull bends UP from the held fret
            note_pitch(b_handle, b_midi);
            int maxpx = (int)(BEND_MAX / BEND_K);
            b_fx = NECK_X0 + (int)((NECK_X1 - NECK_X0) * (b_fret_midi - SBASE[b_lane]) / SPAN);  // dot stays on the fret
            b_fy = cy_of(b_lane) - (int)clamp((float)dpx, -(float)maxpx, (float)maxpx);
        }
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == PICK) {
        for (int l = 0; l < NLANE; l++) {              // pluck each string the finger sweeps THROUGH
            int cy = cy_of(l);
            if ((b_prevy < cy && my >= cy) || (b_prevy > cy && my <= cy)) {
                play(MOUSE, l, clamp(roundf(pos_at(mx)), 0, SPAN));    // plucked at the fret under X
                b_gesture = PICK;                       // play() leaves gesture alone; keep it
                for (int i = 0; i < 6; i++) if (rips[i].life <= 0) { rips[i] = (Rip){ mx, cy, 9, 9 }; break; }
            }
        }
        b_fx = mx; b_fy = my; b_prevy = my;
    }
    if (mouse_released(MOUSE_LEFT) && b_owner == MOUSE) {
        if (b_gesture == GRAB) damp();                 // grabbed note: lift = damp
        else b_owner = NONE;                           // picked notes ring out on their own
    }

    // ── computer keyboard (GarageBand map) — discrete notes, mono ──
    for (int i = 0; i < NK; i++) {
        if (keyp(KC[i])) {
            int midi = KB_ROOT + octv * 12 + KS[i];
            int lane = 3; for (int l = 0; l < NLANE; l++) if (SBASE[l] <= midi) { lane = l; break; }
            play(KBD, lane, (float)(midi - SBASE[lane]));
            b_owner = KBD; b_kbsemi = KS[i];
        }
        if (keyr(KC[i]) && b_owner == KBD && KS[i] == b_kbsemi) damp();
    }
    if (keyp('Z') && octv > 0) octv--;
    if (keyp('X') && octv < 3) octv++;

    b_wob += 0.6f;
    for (int i = 0; i < 6; i++) if (rips[i].life > 0) rips[i].life--;
}

static void retune(void) {
    setup_pizz();
    instrument_filter(ARCOS, FILTER_LOW, tone_hz() + 400, 4);
    if (b_handle >= 0) note_cutoff(b_handle, b_mode == ARCO ? tone_hz() + 400 : tone_hz());
}

// draw one string. The live string is PULLED toward the finger (a tent peaking at
// the finger x, deflected to the finger y) — that vertical pull IS the pitch bend.
static void draw_string(int lane) {
    int cy = STRIP_H + lane * LANE_H + LANE_H / 2;
    int thick = lane;                                // lane 3 = E (lowest) thickest, lane 0 = G thinnest
    bool live = b_handle >= 0 && b_lane == lane;
    int col = live ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY;
    int peakx = live ? clamp(b_fx, NECK_X0 + 1, NECK_X1 - 1) : NECK_X0;
    int px = NECK_X0, py = cy;
    for (int x = NECK_X0; x <= NECK_X1; x += 4) {
        int y = cy;
        if (live) {                                  // tent: 0 at the ends, full pull at the finger
            float t = x <= peakx ? (float)(x - NECK_X0) / (peakx - NECK_X0)
                                 : (float)(NECK_X1 - x) / (NECK_X1 - peakx);
            t = clamp(t, 0, 1);
            y = cy + (int)((b_fy - cy) * t + sinf(b_wob + x * 0.20f) * t * 1.8f);
        }
        for (int tk = 0; tk <= thick; tk++) line(px, py + tk, x, y + tk, col);
        px = x; py = y;
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);

    // ── the fingerboard: dark ebony over the neck wood ──
    rectfill(0, STRIP_H, SCREEN_W, SCREEN_H - STRIP_H, CLR_BROWN);
    rectfill(NECK_X0 - 6, STRIP_H, NECK_X1 - NECK_X0 + 12, SCREEN_H - STRIP_H, CLR_BROWNISH_BLACK);

    // scroll + tuning pegs (left) ; bridge + f-holes (right, the body)
    rectfill(0, STRIP_H, NECK_X0 - 6, SCREEN_H - STRIP_H, CLR_DARK_BROWN);
    for (int p = 0; p < 4; p++) circfill(14, STRIP_H + 18 + p * 38, 4, CLR_LIGHT_PEACH);
    rectfill(NECK_X1 + 6, STRIP_H, SCREEN_W - (NECK_X1 + 6), SCREEN_H - STRIP_H, CLR_DARK_BROWN);
    rectfill(NECK_X1 + 2, STRIP_H + 4, 4, SCREEN_H - STRIP_H - 8, CLR_LIGHT_PEACH);   // the bridge
    line(NECK_X1 + 16, 70, NECK_X1 + 16, 150, CLR_BROWNISH_BLACK);                    // f-holes
    line(NECK_X1 + 24, 70, NECK_X1 + 24, 150, CLR_BROWNISH_BLACK);

    // faint semitone position ticks down the neck (it's fretless — just a guide)
    for (int s = 1; s < SPAN; s++) {
        int x = NECK_X0 + (NECK_X1 - NECK_X0) * s / SPAN;
        int c = (s == 5 || s == 7 || s == 12) ? CLR_DARK_PEACH : CLR_DARK_BROWN;
        line(x, STRIP_H + 2, x, SCREEN_H - 2, c);
    }

    for (int l = 0; l < NLANE; l++) {                // strings + labels
        draw_string(l);
        print(SLAB[l], 4, STRIP_H + l * LANE_H + LANE_H / 2 - 3, CLR_LIGHT_PEACH);
    }

    // the finger: a glowing dot where you're pressing/pulling the live string
    if (b_handle >= 0) {
        int fx = clamp(b_fx, NECK_X0, NECK_X1);
        circfill(fx, b_fy, 5, CLR_YELLOW);
        circ(fx, b_fy, 5, CLR_WHITE);
    }

    // body-slap impact ripples
    for (int i = 0; i < 6; i++)
        if (rips[i].life > 0)
            circ(rips[i].x, rips[i].y, rips[i].max - rips[i].life, CLR_LIGHT_PEACH);
    print("slap", NECK_X1 + 10, SCREEN_H - 12, CLR_DARK_PEACH);   // the body is percussion

    // ── control strip ──
    rectfill(0, 0, SCREEN_W, STRIP_H, CLR_DARK_BROWN);
    line(0, STRIP_H - 1, SCREEN_W, STRIP_H - 1, CLR_BROWNISH_BLACK);
    print("UPRIGHT BASS", 6, 4, CLR_LIGHT_PEACH);

    // big note-name readout
    if (b_handle >= 0) {
        int m = (int)(b_midi + 0.5f);
        char nn[8]; snprintf(nn, sizeof nn, "%s%d", NOTES[m % 12], m / 12 - 1);
        print(nn, 150, 4, CLR_LIGHT_YELLOW);
    }

    ui_begin();
    for (int i = 0; i < 3; i++) {                    // PIZZ / ARCO / SLAP
        int bx = 6 + i * 44;
        if (ui_button(bx, 16, 42, 14, MNAME[i])) b_mode = i;
        if (b_mode == i) rectfill(bx, 31, 42, 2, CLR_LIGHT_YELLOW);
    }
    font(FONT_SMALL);
    if (ui_knob(&k_tone, 248, 16, "TONE")) retune();
    if (ui_knob(&k_ring, 286, 16, "RING")) retune();
    print("sweep=pluck   pull=bend", 144, 24, CLR_DARK_PEACH);
    font(FONT_NORMAL);
    ui_end();

#ifdef DE_TRACE
    watch("midi", "%.2f", b_midi);
    watch("mode", "%d", b_mode);
    watch("on", "%d", b_handle >= 0 ? 1 : 0);
#endif
}
