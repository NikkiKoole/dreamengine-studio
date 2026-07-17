/* de:meta
{
  "slug": "walkbox",
  "title": "Walking Bass",
  "status": "active",
  "created": "2026-07-17",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "waveguide-synth"
  ],
  "lineage": "The TB-303 step-sequencer workflow (tb303.c: draw a line, then per-step flag rows) driving the upright bass (INSTR_BOWED pizz) instead of a synth voice — a 303 done as a real pluck instrument, with a drawable velocity lane for dynamics.",
  "homage": "Roland TB-303 x jazz double bass",
  "description": {
    "summary": "A 303-style bass-line sequencer done as a REAL pluck: draw a 16-step walking line on chunky note-bars, sculpt the dynamics on a drawable VELOCITY lane, and flip per-step SLIDE / OCTAVE / TIE rows. SLIDE slurs one note into the next on the upright's own pizz voice.",
    "detail": "The tb303 workflow in the upright's skin. 16 chunky NOTE-BARS — bar HEIGHT = pitch, scale-locked to E minor pentatonic (2 octaves) so any line walks. Tap a bar to toggle it, drag up/down for pitch, drag sideways to draw the whole line; a bar dragged to the floor is a rest. Directly beneath is a drawable VEL lane — drag across it to sculpt per-step velocity (the dynamics that make a walking line breathe; a loud step reads as an accent, a near-silent one as a ghost). Below that, thin per-step toggle rows: SLD (glide into the next note — a same-voice slur, both directions), OCT (tap-cycle +1 / -1 / off), TIE (let this note ring on — it detaches from the mono voice and decays on its own while the next note plucks). The voice is the upright's INSTR_BOWED pizz; velocity drives the pluck's attack (note_on vol), slide is real portamento (note_glide + note_pitch). Knobs: GLIDE (slur time), TEMPO, TONE (pickup darkness), RING (how long a pluck rings). PLAY runs the loop; RND rolls a fresh line; CLR wipes it. While stopped, drawing a note auditions it.",
    "controls": "Tap a note-bar = on/off; drag up/down = pitch; drag to the floor = rest; drag sideways = draw the line. VEL lane: drag across to draw the per-step dynamics (higher = louder / accented, low = ghost). Rows under it are per-step toggles: tap SLD/TIE to flip, tap OCT to cycle +1 / -1 / off. PLAY / SPACE = run the loop. GLIDE / TEMPO / TONE / RING knobs. RND = random line, CLR = wipe."
  }
}
de:meta */

// Walking Bass — the TB-303 sequencer workflow driving the upright's INSTR_BOWED
// pizz voice. Pitch on the note-bars, dynamics on a drawable velocity lane, and
// per-step SLD/OCT/TIE rows. The slur is real portamento (note_glide + note_pitch).

#include "studio.h"
#include "ui.h"
#include <math.h>

#define BASS   0
#define NBAR   16
#define NDEG   10                 // scale degrees (E min-pent over ~2 octaves)
#define BASE_MIDI 28              // E1

#define BX0    28                 // first bar left (room for row labels at the margin)
#define BGAP   17                 // bar/cell stride
#define BW     14                 // bar/cell width
#define BY     26                 // bar top
#define BH     48                 // bar height

#define VELY   78                 // velocity lane (drawable) — right under the bars
#define VELH   15
#define ROWH   7                  // thin binary rows below
#define SLDY   97
#define OCTY   106
#define TIEY   115

static const int   PENT[NDEG] = { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22 };   // semitones above E
static const char *SLAB[NDEG] = { "E","G","A","B","D","E","G","A","B","D" };

// ── the pattern ──
static int   p_on[NBAR], p_deg[NBAR], p_sld[NBAR], p_tie[NBAR], p_oct[NBAR];
static float p_vel[NBAR];         // per-step velocity 0..1 (dynamics; high = accent, low = ghost)

// ── knobs / transport ──
static float k_glide = 0.35f, k_tone = 0.55f, k_ring = 0.45f, k_tempo = 0.33f;
static int   playing = 0, cur_step = 0, last_16 = -1;

// ── edit state ──
static int sel = 0;
static int drag_gx, drag_gy, drag_axis, drag_on0;
static int aud_cell = -1, aud_deg = -1;

// ── voice ──
static int   voice = -1;
static float ap_tone = -1, ap_ring = -1;

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static int tone_hz(void) { return 500 + (int)(k_tone * 2200); }   // 500..2700 Hz
static int ring_ms(void) { return 90  + (int)(k_ring * 700); }    // 90..790 ms
static int glide_ms(void){ return 30  + (int)(k_glide * 220); }   // 30..250 ms slur
static int vel_vol(float v){ int nv = (int)(1 + v * 6 + 0.5f); return nv < 1 ? 1 : nv > 7 ? 7 : nv; }   // 0..1 → 1..7

static float midi_of(int s) { return BASE_MIDI + PENT[p_deg[s]] + p_oct[s] * 12; }

static void setup_pizz(void) {
    instrument(BASS, INSTR_BOWED, 4, 0, 7, ring_ms());
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);
    instrument_filter(BASS, FILTER_LOW, tone_hz(), 3);
    instrument_harmonics(BASS, 0.30f);      // dark bow position (round, woody)
    instrument_timbre(BASS, 0.30f);
    instrument_morph(BASS, 0.45f);
}
static void retune(void) {                  // TONE, live (cheap — no voice re-init)
    instrument_filter(BASS, FILTER_LOW, tone_hz(), 3);
    if (voice >= 0) note_cutoff(voice, tone_hz());
}

static void gen_random(void) {              // a rollable walking line
    for (int s = 0; s < NBAR; s++) {
        p_on[s]  = rnd_between(0, 100) < 82;
        p_deg[s] = rnd_between(0, NDEG);
        p_sld[s] = rnd_between(0, 100) < 24;
        p_tie[s] = rnd_between(0, 100) < 12;
        p_oct[s] = rnd_between(0, 100) < 12 ? (rnd_between(0, 2) ? 1 : -1) : 0;
        p_vel[s] = (s % 4 == 0 ? 0.85f : 0.55f) + rnd_between(0, 25) / 100.0f;   // beats louder
        if (p_vel[s] > 1) p_vel[s] = 1;
    }
}

void init(void) {
    static const int   don[NBAR] = { 1,0,1,1, 1,1,0,1, 1,1,1,1, 1,0,1,1 };
    static const int   dde[NBAR] = { 0,0,2,4, 1,1,1,3, 0,2,5,3, 7,7,4,2 };
    static const int   dsl[NBAR] = { 0,0,1,0, 0,1,0,0, 0,1,0,0, 1,0,1,0 };
    static const int   dti[NBAR] = { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 };
    static const float dvl[NBAR] = { .95f,.5f,.6f,.65f, .85f,.6f,.5f,.6f, .95f,.6f,.65f,.6f, .85f,.8f,.6f,.65f };
    for (int s = 0; s < NBAR; s++) { p_on[s]=don[s]; p_deg[s]=dde[s]; p_sld[s]=dsl[s]; p_tie[s]=dti[s]; p_oct[s]=0; p_vel[s]=dvl[s]; }
    setup_pizz();
    reverb(0.30f, 0.55f);                    // a small warm room
    instrument_reverb(BASS, 0.16f);
}

// ── voice helpers (mono, glide-aware) ──
static void pluck(float midi, float vel) {
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(midi + 0.5f), BASS, vel_vol(vel));
    note_pitch(voice, midi);
    note_glide(voice, glide_ms());
}
static void slur(float midi) {               // re-pitch the ringing voice — no re-attack
    if (voice < 0) { pluck(midi, 0.6f); return; }
    note_glide(voice, glide_ms());
    note_pitch(voice, midi);
}
static void silence(void) { if (voice >= 0) note_off(voice); voice = -1; }

static void fire_step(int s) {
    int prev = (s + NBAR - 1) % NBAR;
    if (p_on[s]) {
        float m = midi_of(s);
        if (p_on[prev] && p_sld[prev] && voice >= 0) slur(m);              // legato slur, same voice
        else pluck(m, p_vel[s]);                                           // fresh pluck at this velocity
        if (p_tie[s]) voice = -1;                                          // TIE: let it ring on untracked
    } else if (voice >= 0) { note_off(voice); voice = -1; }                // rest damps
}

void update(void) {
    bpm(60 + (int)(k_tempo * 120));          // 60..180

    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) last_16 = -1; else silence(); }

    if (playing) {
        int sx = (int)(beat() * 4 + beat_pos() * 4.0f);
        if (sx != last_16) { last_16 = sx; cur_step = sx % NBAR; fire_step(cur_step); }
    }

#ifdef DE_TRACE
    watch("step", "%d", cur_step);
    watch("playing", "%d", playing);
#endif
}

// ── widgets in the wood skin ──
static void wknob(unsigned seed, float *v, int cx, int cy, int r, const char *name, int lead) {
    void *wid = ui_wid_hash(seed, cx - r, cy - r, 2*r+1, 2*r+1);
    ui_reg(wid, cx - r, cy - r, 2*r+1, 2*r+1, 1);
    UiCap *c = ui_cap_for(wid);
    if (c) {
        if (!c->has_v0) { c->has_v0 = 1; c->v0 = *v; c->by = c->cy; }
        int py = c->released ? c->ry : c->cy;
        *v = clamp(c->v0 + (c->by - py) / 80.0f, 0, 1);
    }
    int lit = lead || c;
    circfill(cx, cy, r, CLR_BROWNISH_BLACK);
    circ(cx, cy, r, lit ? CLR_LIGHT_YELLOW : CLR_DARK_PEACH);
    float a = -2.356194f + *v * 4.712389f;   // 135°..405°
    line(cx, cy, cx + (int)((r-2) * sinf(a)), cy - (int)((r-2) * cosf(a)), lit ? CLR_LIGHT_YELLOW : CLR_LIGHT_PEACH);
    font(FONT_SMALL); print(name, cx - slen(name) * 2, cy + r + 3, lit ? CLR_LIGHT_YELLOW : CLR_DARK_PEACH); font(FONT_NORMAL);
}
static int wbtn(unsigned seed, int x, int y, int w, int h, const char *s, int on) {
    void *wid = ui_wid_hash(seed, x, y, w, h);
    int f = 0, p = 0, hot = 0;
    int hit = ui_button_core(wid, x, y, w, h, &f, &p, &hot);
    rectfill(x, y, w, h, on ? CLR_ORANGE : CLR_BROWNISH_BLACK);
    rect(x, y, w, h, (on || hot) ? CLR_LIGHT_YELLOW : CLR_DARK_PEACH);
    font(FONT_SMALL); print(s, x + w/2 - slen(s) * 2, y + h/2 - 2, on ? CLR_BROWNISH_BLACK : CLR_LIGHT_PEACH); font(FONT_NORMAL);
    return hit;
}

static int bar_top(int deg) { return BY + BH - BH * (deg + 1) / NDEG; }

void draw(void) {
    cls(CLR_DARK_BROWN);
    ui_begin();

    // ── the fingerboard body behind the bars ──
    rectfill(BX0 - 6, BY - 5, NBAR * BGAP + 4, BH + 10, CLR_BROWN);
    rectfill(BX0 - 4, BY - 5, NBAR * BGAP,     BH + 10, CLR_BROWNISH_BLACK);
    for (int s = 0; s < NBAR; s += 4)
        line(BX0 + s * BGAP - 2, BY - 4, BX0 + s * BGAP - 2, BY + BH + 4, CLR_DARK_BROWN);   // beat guides

    // ── the 16 note-bars: register + edit + draw (always note-draw mode) ──
    for (int s = 0; s < NBAR; s++) {
        int bx = BX0 + s * BGAP;
        void *w = ui_wid_hash(0xB0u + s, bx, BY, BW, BH);
        ui_reg(w, bx, BY, BW, BH, 0);
        UiCap *c = ui_cap_for(w);
        if (c) {
            if (ui_grabbed(w)) { drag_gx = c->cx; drag_gy = c->cy; drag_axis = 0; drag_on0 = p_on[s]; sel = s; aud_cell = -1; }
            int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
            int adx = px - drag_gx; if (adx < 0) adx = -adx;
            int ady = py - drag_gy; if (ady < 0) ady = -ady;
            if (!drag_axis && (adx > 4 || ady > 4)) drag_axis = 1;
            if (drag_axis) {
                int cell = (px - BX0) / BGAP; if (cell < 0) cell = 0; if (cell >= NBAR) cell = NBAR - 1;
                sel = cell;
                if (py >= BY + BH - 4) p_on[cell] = 0;        // floor band → rest
                else {
                    float frac = clamp((BY + BH - 4 - py) / (float)(BH - 5), 0, 1);
                    int d = (int)(frac * (NDEG - 1) + 0.5f);
                    p_deg[cell] = d; p_on[cell] = 1;
                    if (!playing && (cell != aud_cell || d != aud_deg)) { aud_cell = cell; aud_deg = d; pluck(midi_of(cell), p_vel[cell]); }
                }
            }
            if (ui_released(w) && !drag_axis) { p_on[s] = !drag_on0; sel = s; if (p_on[s] && !playing) pluck(midi_of(s), p_vel[s]); }
        }

        int here = (playing && s == cur_step);
        int lit  = here || (!playing && s == sel);
        rrectfill(bx, BY, BW, BH, 1, CLR_DARK_BROWN);
        if (here)                     { blend(BLEND_AVG); rrectfill(bx, BY, BW, BH, 1, CLR_LIGHT_PEACH); blend_reset(); }
        else if (!playing && s == sel){ blend(BLEND_AVG); rrectfill(bx, BY, BW, BH, 1, CLR_BROWN); blend_reset(); }
        if (p_on[s]) {
            int top = bar_top(p_deg[s]);
            rrectfill(bx, top, BW, BY + BH - top, 1, p_vel[s] > 0.8f ? CLR_ORANGE : CLR_PEACH);
            if (p_sld[s])          rectfill(bx, top, BW, 2, CLR_LIGHT_YELLOW);     // sliding cap
            else if (p_vel[s] > 0.8f) rectfill(bx, top, BW, 1, CLR_LIGHT_YELLOW);  // accent (loud) cap
            if (p_oct[s] > 0)      rectfill(bx, BY, BW, 2, CLR_TRUE_BLUE);         // +1 octave tick
            else if (p_oct[s] < 0) rectfill(bx, BY + BH - 2, BW, 2, CLR_INDIGO);   // -1 octave tick
        }
        rrect(bx, BY, BW, BH, 1, lit ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
    }

    // ── glide-lines on top (the headline) ──
    for (int s = 0; s < NBAR; s++) {
        if (!p_on[s] || !p_sld[s]) continue;
        int ns = (s + 1) % NBAR;
        if (!p_on[ns]) continue;
        int top = bar_top(p_deg[s]), ntop = bar_top(p_deg[ns]);
        int x0 = BX0 + s * BGAP + BW - 1, x1 = BX0 + ns * BGAP;
        line(x0, top,     x1, ntop,     CLR_LIGHT_YELLOW);
        line(x0, top + 1, x1, ntop + 1, CLR_LIGHT_YELLOW);
    }
    // TIE tails: a dotted line from the note top extending right — it rings on
    for (int s = 0; s < NBAR; s++) {
        if (!p_on[s] || !p_tie[s]) continue;
        int top = bar_top(p_deg[s]), x0 = BX0 + s * BGAP + BW;
        for (int tx = x0; tx < x0 + BGAP + BW/2 && tx < BX0 + NBAR * BGAP; tx += 3) { pset(tx, top, CLR_PEACH); pset(tx, top + 1, CLR_PEACH); }
    }

    // ── VELOCITY lane (drawable): drag across to sculpt per-step dynamics ──
    {
        void *w = ui_wid_hash(0xE0u, BX0, VELY, NBAR * BGAP, VELH);
        ui_reg(w, BX0, VELY, NBAR * BGAP, VELH, 0);
        UiCap *c = ui_cap_for(w);
        if (c) {
            int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
            int col = (px - BX0) / BGAP; if (col < 0) col = 0; if (col >= NBAR) col = NBAR - 1;
            p_vel[col] = clamp((VELY + VELH - py) / (float)VELH, 0.05f, 1.0f);
            sel = col;
            if (!playing && col != aud_cell) { aud_cell = col; aud_deg = -1; if (p_on[col]) pluck(midi_of(col), p_vel[col]); }
        }
        font(FONT_SMALL); print("VEL", 2, VELY + VELH/2 - 3, CLR_DARK_PEACH); font(FONT_NORMAL);
        for (int s = 0; s < NBAR; s++) {
            int x = BX0 + s * BGAP, h = (int)(p_vel[s] * VELH + 0.5f); if (h < 1) h = 1;
            rectfill(x, VELY, BW, VELH, CLR_BROWNISH_BLACK);
            int col = !p_on[s] ? CLR_DARK_BROWN : p_vel[s] > 0.8f ? CLR_ORANGE : p_vel[s] > 0.45f ? CLR_PEACH : CLR_DARK_PEACH;
            rectfill(x, VELY + VELH - h, BW, h, col);
        }
    }

    // ── thin per-step binary rows: SLD · OCT · TIE ──
    static const int   RYA[3]  = { SLDY, OCTY, TIEY };
    static const char *RLB[3]  = { "SLD", "OCT", "TIE" };
    font(FONT_SMALL);
    for (int r = 0; r < 3; r++) print(RLB[r], 2, RYA[r] + 1, CLR_DARK_PEACH);
    font(FONT_NORMAL);
    for (int r = 0; r < 3; r++) {
        void *w = ui_wid_hash(0xD0u + r, BX0, RYA[r], NBAR * BGAP, ROWH);   // one widget per row
        ui_reg(w, BX0, RYA[r], NBAR * BGAP, ROWH, 0);
        UiCap *c = ui_cap_for(w);
        if (c && ui_grabbed(w)) {                             // press toggles the cell under the finger
            int col = (c->cx - BX0) / BGAP; if (col < 0) col = 0; if (col >= NBAR) col = NBAR - 1;
            sel = col;
            if      (r == 0) p_sld[col] = !p_sld[col];
            else if (r == 1) p_oct[col] = p_oct[col] == 0 ? 1 : p_oct[col] == 1 ? -1 : 0;   // +1 → -1 → off
            else             p_tie[col] = !p_tie[col];
        }
        for (int s = 0; s < NBAR; s++) {
            int x = BX0 + s * BGAP;
            int set = r == 0 ? p_sld[s] : r == 1 ? p_oct[s] != 0 : p_tie[s];
            int col = r == 0 ? CLR_LIME_GREEN : r == 1 ? (p_oct[s] > 0 ? CLR_TRUE_BLUE : CLR_INDIGO) : CLR_LIGHT_YELLOW;
            if (set) rectfill(x, RYA[r], BW, ROWH, col);
            else     rect(x, RYA[r], BW, ROWH, s == sel ? CLR_DARK_PEACH : CLR_BROWNISH_BLACK);
        }
    }

    // ── top strip: title · transport · key · RND · CLR ──
    rectfill(0, 0, SCREEN_W, 22, CLR_DARK_BROWN);
    line(0, 21, SCREEN_W, 21, CLR_BROWNISH_BLACK);
    print("WALKING BASS", 6, 3, CLR_LIGHT_PEACH);
    font(FONT_SMALL); print("the pluck 303", 6, 13, CLR_DARK_PEACH); font(FONT_NORMAL);

    {   // PLAY / STOP
        int x = 108, y = 3, w = 16, h = 16, f = 0, p = 0, hot = 0;
        void *wid = ui_wid_hash(0x50u, x, y, w, h);
        if (ui_button_core(wid, x, y, w, h, &f, &p, &hot)) { playing = !playing; if (playing) last_16 = -1; else silence(); }
        rectfill(x, y, w, h, CLR_BROWNISH_BLACK); rect(x, y, w, h, hot ? CLR_LIGHT_YELLOW : CLR_DARK_PEACH);
        if (playing) rectfill(x + 5, y + 4, 6, 8, CLR_ORANGE);
        else         tri(x + 5, y + 4, x + 5, y + 12, x + 12, y + 8, CLR_LIME_GREEN);
    }
    font(FONT_SMALL);
    { char b[16]; int t = 60 + (int)(k_tempo * 120); b[0]='0'+t/100; b[1]='0'+(t/10)%10; b[2]='0'+t%10; b[3]=' '; b[4]='B';b[5]='P';b[6]='M';b[7]=0; print(b, 128, 3, CLR_LIGHT_PEACH); }
    print("KEY E", 128, 12, CLR_DARK_PEACH);
    font(FONT_NORMAL);

    if (wbtn(0x62u, 260, 4, 26, 14, "RND", 0)) gen_random();
    if (wbtn(0x63u, 290, 4, 26, 14, "CLR", 0))
        for (int s = 0; s < NBAR; s++) { p_on[s]=p_sld[s]=p_tie[s]=p_oct[s]=0; p_vel[s]=0.65f; }

    // ── bottom strip: knobs (GLIDE leads) + hint ──
    rectfill(0, TIEY + ROWH + 3, SCREEN_W, SCREEN_H - (TIEY + ROWH + 3), CLR_DARK_BROWN);
    int ky = 150;
    wknob(0x70u, &k_glide, 34,  ky, 9, "GLIDE", 1);
    wknob(0x71u, &k_tempo, 90,  ky, 6, "TEMPO", 0);
    wknob(0x72u, &k_tone,  144, ky, 6, "TONE",  0);
    wknob(0x73u, &k_ring,  196, ky, 6, "RING",  0);
    font(FONT_SMALL);
    print("drag bar=pitch",   232, ky - 8, CLR_DARK_PEACH);
    print("VEL lane=dynamics", 232, ky + 2, CLR_DARK_PEACH);
    font(FONT_NORMAL);

    // apply tone/ring only on change (set-and-hold)
    if (k_tone != ap_tone) { retune();     ap_tone = k_tone; }
    if (k_ring != ap_ring) { setup_pizz();  ap_ring = k_ring; }

    ui_end();
}
