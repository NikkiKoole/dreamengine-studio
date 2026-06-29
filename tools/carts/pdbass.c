/* de:meta
{
  "title": "pd bass",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling",
    "step-sequencer"
  ],
  "lineage": "Sibling of `upright` (the bowed-string fingerboard); swaps the modeled string for INSTR_PD (Casio CZ phase-distortion oscillator) to gain bidirectional continuous glide — the one expressive axis the bowed model could not provide.",
  "homage": "Casio CZ phase distortion",
  "description": "The upright bass's fingerboard, voiced on the Casio CZ phase-distortion engine (INSTR_PD) instead of a modeled string. The whole point of the swap: INSTR_PD is an OSCILLATOR - it sustains while held and glides BOTH ways - so where the bowed-string upright could only fret-walk downward, this synth bass does a TRUE smooth continuous slide, up and down, no re-pluck, no fret click (verified: it bends a held note down cleanly, +0.9 cents, where the waveguide stuck). Same string interactions as the upright: press a string to pluck a note (snaps to a clean semitone for in-tune starts); slide LEFT/RIGHT to glide the pitch continuously in either direction; PULL the string up or down to bend it (signed - up sharpens, down flattens - and the slide position freezes while you pull, so only the bend moves the pitch); start a drag in the open gap next to a string and sweep THROUGH it to PICK it (press ON a string grabs/frets, press in the gap picks). One pluck articulation (no arco/slap), and ONE TIMBRE slider down the right edge runs the PD distortion macro live from CLEAN (bottom) to BUZZY (top) - the signature CZ resonant edge, sweepable while a note rings. A plucky CZ bass patch on the RESO TRAP wave (fast attack, sustains while held, releases on lift; the morph macro gives the CZ strike sweep). A toggleable backing DRUM LOOP (SPACE, or the DRUMS button) - a bouncy boom-bap kick/snare/hat at 96 bpm - to play the bass over. Range E1-G3. Play with mouse or a phone tap, or the computer keyboard (GarageBand musical-typing map, A = E1; Z / X shift the octave). Dark synth panel with four glowing strings (low E drawn thickest) that deflect as you bend."
}
de:meta */
#include "studio.h"
#include <math.h>
#include <stdio.h>

// ── PD BASS ──────────────────────────────────────────────────────────────────
// The upright's fingerboard, but voiced on the Casio CZ phase-distortion engine
// (INSTR_PD) instead of a modeled string. The point of the swap: INSTR_PD is an
// OSCILLATOR — it sustains while held and glides BOTH ways — so unlike the bowed
// string (which can only bend up), this bass does a true SMOOTH continuous slide,
// up and down, no fret-walk, no re-pluck. Same string interactions as `upright`:
//   • press a string to pluck a note (snaps to a clean semitone)
//   • slide LEFT/RIGHT to glide the pitch continuously (both directions)
//   • PULL up/down to bend (the fret holds while you pull — only the bend moves it)
//   • start a drag in the gap and sweep THROUGH a string to PICK it
// One TIMBRE slider on the right runs the PD distortion macro (clean → buzzy) live.
// Computer keyboard: GarageBand map (A = E1), Z / X shift the octave.

#define SLOT     5                         // INSTR_PD — the phase-distortion bass
#define DK       6                         // drum-loop voices: kick / snare / hat
#define SN       7
#define HH       8
#define DRUM_BX  (SCREEN_W - 52)           // the DRUMS toggle button (header)
#define DRUM_BW  46

#define STRIP_H  30
#define NECK_X0  24
#define NECK_X1  (SCREEN_W - 34)           // leave the right edge for the TIMBRE slider
#define SPAN     12                        // semitones nut→far end
#define NLANE    4
#define LANE_H   ((SCREEN_H - STRIP_H) / NLANE)
#define SLIDER_X (SCREEN_W - 26)           // timbre slider track
#define SLIDER_W 14

#define BEND_K   0.05f
#define BEND_MAX 2.0f
#define GRAB_PX  13

static const int   SBASE[NLANE] = { 43, 38, 33, 28 };   // G2 D2 A1 E1
static const char *SLAB [NLANE] = { "G", "D", "A", "E" };

#define NONE  (-999)
#define KBD   (-2)
#define MOUSE (-3)
static int   b_handle = -1;
static int   b_owner  = NONE;
static int   b_lane   = 3;
static float b_base   = 28;                // the slide position pitch (frozen while pulling)
static float b_midi   = 28;                // sounding pitch (base + bend)
static int   b_kbsemi = -1;
static int   b_press_x, b_press_y;
static bool  b_sliding;
static int   b_fx, b_fy;
static float b_wob;
enum { GRAB, PICK };
static int   b_gesture;
static int   b_prevy;
static bool  t_drag;                       // dragging the TIMBRE slider

static const char gb_wkey[11]  = "ASDFGHJKL;'";
static const int  gb_wsemi[11] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17 };
static const char gb_bkey[7]   = { 'W', 'E', 'T', 'Y', 'U', 'O', 'P' };
static const int  gb_bsemi[7]  = { 1, 3, 6, 8, 10, 13, 15 };
static char KC[18]; static int KS[18]; static int NK;
#define KB_ROOT 28
static int octv = 1;

static float k_timbre = 0.30f;             // TIMBRE slider → PD distortion (clean..buzzy)
static const char *NOTES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

// a little backing drum loop (16 steps) — a bouncy boom-bap to play the bass over
static bool drums_on = false;
static int  last_step = -1;
static const int KICK [16] = { 1,0,0,0, 0,0,1,0, 0,0,1,0, 0,0,0,0 };
static const int SNARE[16] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 };
static const int HAT  [16] = { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 };

void init(void) {
    NK = 0;
    for (int i = 0; i < 11; i++) { KC[NK] = gb_wkey[i]; KS[NK] = gb_wsemi[i]; NK++; }
    for (int i = 0; i < 7;  i++) { KC[NK] = gb_bkey[i]; KS[NK] = gb_bsemi[i]; NK++; }
    // a plucky CZ bass: fast attack, decays to a held sustain (so slides keep ringing),
    // releases when you lift. morph = the CZ strike sweep (bright snap that settles).
    instrument(SLOT, INSTR_PD, 2, 150, 4, 220);
    instrument_harmonics(SLOT, 0.77f);     // RESO TRAP — the resonant CZ-bass wave
    instrument_timbre(SLOT, k_timbre);     // distortion — the slider drives this live
    instrument_morph(SLOT, 0.40f);         // DCW strike sweep (the CZ "wow")

    // backing drum loop voices (from drummachine.c)
    bpm(96);
    instrument(DK, INSTR_SINE,  0, 280, 0, 60);  instrument_env(DK, 1, ENV_PITCH, 0, 55, 30);  // kick
    instrument(SN, INSTR_NOISE, 0, 130, 0, 50);  instrument_filter(SN, FILTER_BAND, 1400, 3);  // snare
    instrument(HH, INSTR_NOISE, 0, 25,  0, 15);  instrument_filter(HH, FILTER_HIGH, 6500, 2);  // hat
}

static int   cy_of(int lane) { return STRIP_H + lane * LANE_H + LANE_H / 2; }
static float pos_at (int x)  { return clamp((float)(x - NECK_X0) / (NECK_X1 - NECK_X0), 0, 1) * SPAN; }

static void damp(void) {
    if (b_handle >= 0) note_off(b_handle);
    b_handle = -1; b_owner = NONE; b_kbsemi = -1;
}

// pluck a note at `pos` semitones up the string. INSTR_PD glides both ways, so the
// slide just re-targets note_pitch — no open-string trick, no re-articulation.
static void play(int owner, int lane, float pos) {
    if (b_handle >= 0) note_off(b_handle);
    b_owner = owner; b_lane = lane; b_base = SBASE[lane] + pos; b_midi = b_base; b_wob = 0;
    b_handle = note_on((int)(b_midi + 0.5f), SLOT, 6);
    note_pitch(b_handle, b_midi);
    note_glide(b_handle, 70);               // slides/bends slur smoothly
    b_fx = NECK_X0 + (int)((NECK_X1 - NECK_X0) * pos / SPAN);
    b_fy = cy_of(lane);
}

static void set_timbre_from_y(int y) {
    int top = STRIP_H + 10, bot = SCREEN_H - 10;
    k_timbre = clamp((float)(bot - y) / (bot - top), 0, 1);   // top = buzzy, bottom = clean
    instrument_timbre(SLOT, k_timbre);                        // live — sweeps the held note too
}

void update(void) {
    int mx = mouse_x(), my = mouse_y();

    if (keyp(KEY_SPACE)) drums_on = !drums_on;       // SPACE toggles the drum loop
    if (mouse_pressed(MOUSE_LEFT) && my < STRIP_H &&  // ...or the DRUMS button in the header
        mx >= DRUM_BX && mx <= DRUM_BX + DRUM_BW && my >= 3 && my <= 16)
        drums_on = !drums_on;

    // the loop: fire each step once as the 16th-note counter ticks over
    if (drums_on) {
        int step = (beat() * 4 + (int)(beat_pos() * 4)) % 16;
        if (step != last_step) {
            last_step = step;
            if (KICK [step]) { hit(72, INSTR_NOISE, 1, 12); hit(34, DK, 4, 250); }
            if (SNARE[step]) { hit(58, SN, 3, 110); hit(53, INSTR_TRI, 2, 45); }
            if (HAT  [step]) hit(92, HH, 2, 22);
        }
    } else last_step = -1;

    if (mouse_pressed(MOUSE_LEFT) && my >= STRIP_H) {
        if (mx >= SLIDER_X - 4) {                    // the TIMBRE slider strip
            t_drag = true; set_timbre_from_y(my);
        } else if (mx >= NECK_X0 - 6 && mx <= NECK_X1 + 6) {
            int near = 0, nd = 9999;                 // nearest string
            for (int l = 0; l < NLANE; l++) { int d = abs(my - cy_of(l)); if (d < nd) { nd = d; near = l; } }
            if (nd <= GRAB_PX) {                     // ON a string → grab/fret
                play(MOUSE, near, roundf(pos_at(mx)));
                b_gesture = GRAB; b_fx = mx; b_fy = my;
            } else { b_owner = MOUSE; b_gesture = PICK; }   // OPEN gap → pick
            b_press_x = mx; b_press_y = my; b_prevy = my; b_sliding = false;
        }
    }
    else if (mouse_down(MOUSE_LEFT) && t_drag) {
        set_timbre_from_y(my);
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == GRAB) {
        if (!b_sliding && (abs(mx - b_press_x) > 4 || abs(my - b_press_y) > 4)) b_sliding = true;
        if (b_sliding) {
            int dpx = b_press_y - my;
            // while actively PULLING, freeze the slide position — only the bend moves the
            // pitch; otherwise LEFT/RIGHT glides the pitch continuously (PD bends both ways)
            if (abs(dpx) <= 6) b_base = SBASE[b_lane] + clamp(pos_at(mx), 0, SPAN);
            float ybend = clamp((float)dpx * BEND_K, -BEND_MAX, BEND_MAX);   // signed — up OR down
            b_midi = b_base + ybend;
            note_pitch(b_handle, b_midi);
            int maxpx = (int)(BEND_MAX / BEND_K);
            b_fx = NECK_X0 + (int)((NECK_X1 - NECK_X0) * (b_base - SBASE[b_lane]) / SPAN);
            b_fy = cy_of(b_lane) - (int)clamp((float)dpx, -(float)maxpx, (float)maxpx);
        }
    }
    else if (mouse_down(MOUSE_LEFT) && b_owner == MOUSE && b_gesture == PICK) {
        for (int l = 0; l < NLANE; l++) {            // sweep THROUGH a string → pluck it
            int cy = cy_of(l);
            if ((b_prevy < cy && my >= cy) || (b_prevy > cy && my <= cy))
                play(MOUSE, l, clamp(roundf(pos_at(mx)), 0, SPAN));
        }
        b_fx = mx; b_fy = my; b_prevy = my;
    }
    // Release / safety net: if the mouse button isn't down, nothing should still be
    // held by the mouse. Keying off !mouse_down (not the one-frame mouse_released event)
    // catches a release that was missed — e.g. when a big PULL drags the pointer out of
    // the play area — so a sustaining PD note can never get stuck on.
    if (!mouse_down(MOUSE_LEFT)) {
        t_drag = false;
        if (b_owner == MOUSE) damp();
    }

    // computer keyboard (GarageBand map) — discrete notes, mono
    for (int i = 0; i < NK; i++) {
        if (keyp(KC[i])) {
            int midi = KB_ROOT + octv * 12 + KS[i];
            play(KBD, 3, midi - SBASE[3]);           // play the exact pitch (no onset scoop)
            b_owner = KBD; b_kbsemi = KS[i];
        }
        if (keyr(KC[i]) && b_owner == KBD && KS[i] == b_kbsemi) damp();
    }
    if (keyp('Z') && octv > 0) octv--;
    if (keyp('X') && octv < 3) octv++;

    b_wob += 0.6f;
}

static void draw_string(int lane) {
    int cy = cy_of(lane);
    int thick = lane;                                // E (lane 3) thickest
    bool live = b_handle >= 0 && b_lane == lane;
    int col = live ? CLR_PINK : CLR_INDIGO;
    int peakx = live ? clamp(b_fx, NECK_X0 + 1, NECK_X1 - 1) : NECK_X0;
    int px = NECK_X0, py = cy;
    for (int x = NECK_X0; x <= NECK_X1; x += 4) {
        int y = cy;
        if (live) {
            float t = x <= peakx ? (float)(x - NECK_X0) / (peakx - NECK_X0)
                                 : (float)(NECK_X1 - x) / (NECK_X1 - peakx);
            t = clamp(t, 0, 1);
            y = cy + (int)((b_fy - cy) * t + sinf(b_wob + x * 0.22f) * t * 1.8f);
        }
        for (int tk = 0; tk <= thick; tk++) line(px, py + tk, x, y + tk, col);
        px = x; py = y;
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);

    // ── playfield + faint semitone guides ──
    for (int s = 1; s < SPAN; s++) {
        int x = NECK_X0 + (NECK_X1 - NECK_X0) * s / SPAN;
        line(x, STRIP_H, x, SCREEN_H, (s == 5 || s == 7 || s == 12) ? CLR_DARK_BLUE : CLR_DARKER_PURPLE);
    }
    for (int l = 0; l < NLANE; l++) {
        draw_string(l);
        print(SLAB[l], 6, cy_of(l) - 3, CLR_INDIGO);
    }
    if (b_handle >= 0) {                              // the finger glow
        int fx = clamp(b_fx, NECK_X0, NECK_X1);
        circfill(fx, b_fy, 4, CLR_LIGHT_PEACH);
        circ(fx, b_fy, 5, CLR_PINK);
    }

    // ── TIMBRE slider (right edge) ──
    int top = STRIP_H + 10, bot = SCREEN_H - 10;
    rectfill(SLIDER_X, top, SLIDER_W, bot - top, CLR_DARK_BLUE);
    rect(SLIDER_X, top, SLIDER_W, bot - top, CLR_INDIGO);
    int hy = bot - (int)(k_timbre * (bot - top));
    rectfill(SLIDER_X, hy, SLIDER_W, bot - hy, CLR_ORANGE);   // fill from clean(bottom) up
    rectfill(SLIDER_X - 1, hy - 1, SLIDER_W + 2, 3, CLR_LIGHT_PEACH);  // the handle
    font(FONT_SMALL);
    print("BUZZY", SLIDER_X - 2, top - 8, CLR_DARK_PURPLE);
    print("CLEAN", SLIDER_X - 2, bot + 1, CLR_DARK_PURPLE);

    // ── header ──
    rectfill(0, 0, SCREEN_W, STRIP_H, CLR_DARK_BLUE);
    line(0, STRIP_H - 1, SCREEN_W, STRIP_H - 1, CLR_INDIGO);
    font(FONT_NORMAL);
    print("PD BASS", 6, 5, CLR_PINK);
    if (b_handle >= 0) {
        int m = (int)(b_midi + 0.5f);
        char nn[8]; snprintf(nn, sizeof nn, "%s%d", NOTES[m % 12], m / 12 - 1);
        print(nn, 70, 5, CLR_LIGHT_PEACH);
    }
    // DRUMS toggle button
    rectfill(DRUM_BX, 3, DRUM_BW, 13, drums_on ? CLR_PINK : CLR_DARKER_PURPLE);
    rect(DRUM_BX, 3, DRUM_BW, 13, CLR_INDIGO);
    print("DRUMS", DRUM_BX + 7, 6, drums_on ? CLR_DARKER_BLUE : CLR_LIGHT_PEACH);
    if (drums_on) {                                  // a 4-beat playhead pulse
        int b = beat() % 4;
        for (int i = 0; i < 4; i++)
            circfill(DRUM_BX - 30 + i * 7, 9, 2, i == b ? CLR_LIGHT_PEACH : CLR_INDIGO);
    }
    font(FONT_SMALL);
    print("reso trap   slide=pitch  pull=bend  sweep=pluck", 6, 19, CLR_INDIGO);
    font(FONT_NORMAL);

#ifdef DE_TRACE
    watch("midi", "%.2f", b_midi);
    watch("timbre", "%.2f", k_timbre);
    watch("on", "%d", b_handle >= 0 ? 1 : 0);
#endif
}
