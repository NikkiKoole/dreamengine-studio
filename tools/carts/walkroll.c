/* de:meta
{
  "slug": "walkroll",
  "title": "Walk-Roll",
  "status": "active",
  "created": "2026-07-18",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "waveguide-synth"
  ],
  "lineage": "The walkbox walking-bass sequencer re-imagined as a horizontal PIANO-ROLL: a note is a capsule (x=time, y=scale-degree) whose WIDTH is its length and which carries dedicated GRABBER handles you pick up in place — a top knob = bend, right edge = length/ring, a tip nub = slide to the next note, a bottom corner = scoop. Voice is the upright's INSTR_BOWED pizz (from walkbox). Grew out of the walkroll/walkgrab mockups.",
  "homage": "Roland TB-303 workflow x jazz double bass x a DAW piano-roll",
  "description": {
    "summary": "A walking-bass PIANO-ROLL played on the upright's real pizz voice. Tap the grid to place notes (x=time, y=pitch, scale-locked to E min pentatonic). Pick a note and its grabbers pop up: a top knob to BEND it (the expressive 'ding'), the right edge to set LENGTH (how long it rings), a tip nub to SLIDE into the next note, a bottom corner to SCOOP into the attack. PLAY loops it.",
    "detail": "The walkbox redesign: length, slide and bend all become geometry of the note itself instead of separate lanes/rows. 16 steps across (time), 10 scale-degrees up (E minor pentatonic, 2 octaves, so every row is in-key). Tap an empty cell to place a note; drag its body to move it (pitch x time); drag it below the grid to delete. The SELECTED note shows dedicated grabbers: TOP KNOB (drag up = bend depth, a hump that rises and returns), RIGHT EDGE (drag = length in cells; a long note visibly spans cells and fades as it rings), TIP NUB (tap = slide/slur into the next note on the same voice), BOTTOM CORNER (tap = scoop up into the attack). Voice = the upright INSTR_BOWED pizz; bends re-pitch the sounding voice continuously via note_pitch. Knobs: GLIDE (slur/bend time), TEMPO, TONE (pickup darkness), RING (pluck ring-out). PLAY / SPACE loops; RND rolls a line; CLR wipes.",
    "controls": "Tap empty cell = place a note (x=time, y=pitch). Tap a note = select it (grabbers appear). Drag a note's body = move it; drag it off the bottom = delete. On the selected note: drag the TOP KNOB up = bend; drag the RIGHT EDGE = length; tap the TIP nub = slide to next; tap the BOTTOM-LEFT corner = scoop. PLAY / SPACE = loop. GLIDE / TEMPO / TONE / RING knobs. RND = random line, CLR = wipe."
  }
}
de:meta */

// Walk-Roll — the walkbox walking-bass sequencer as a horizontal PIANO-ROLL.
// A note is a capsule (x=time, y=pitch); pick it and grabber handles pop up:
// top knob = bend, right edge = length, tip = slide, corner = scoop.
// Voice = the upright's INSTR_BOWED pizz (see walkbox.c). docs/design/walkbox.md.

#include "studio.h"
#include "ui.h"
#include <math.h>

#define BASS   0
#define NCOL   16
#define NDEG   10                 // E min-pent over ~2 octaves
#define BASE_MIDI 28              // E1

#define GX0    26                 // grid left (row labels in the margin)
#define GY     26                 // grid top
#define COLW   18                 // step stride (time)
#define ROWH   12                 // degree stride (pitch)
#define GW     (NCOL * COLW)
#define GH     (NDEG * ROWH)

static const int   PENT[NDEG] = { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22 };   // semitones above E
static const char *SLAB[NDEG] = { "E","G","A","B","D","E","G","A","B","D" };

// bend shapes (a pitch contour on ONE note) — v1 exposes HUMP (knob) + SCOOP (corner)
enum { BM_NONE, BM_HUMP, BM_SCOOP, BM_HOLD, BM_VIB };

// ── the pattern (one note per START step; length spans cells) ──
static int   n_on[NCOL], n_deg[NCOL], n_len[NCOL], n_slide[NCOL], n_bmode[NCOL];
static float n_bend[NCOL];        // bend amount 0..1 (audio: *2 semitones; visual: *rows)
static int   sel = -1;            // selected note's start col (-1 = none)

// ── knobs / transport ──
static float k_glide = 0.35f, k_tone = 0.55f, k_ring = 0.45f, k_tempo = 0.33f;
static int   playing = 0, cur_step = 0, last_16 = -1;

// ── voice / playback ──
static int   voice = -1;
static float ap_tone = -1, ap_ring = -1;
static int   active_col = -1, active_until = -1, active_start = -1, active_len = 1;

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int tone_hz(void) { return 500 + (int)(k_tone * 2200); }
static int ring_ms(void) { return 90  + (int)(k_ring * 700); }
static int glide_ms(void){ return 30  + (int)(k_glide * 220); }

static int   cell_x(int col) { return GX0 + col * COLW; }
static int   row_y(int deg)  { return GY + (NDEG - 1 - deg) * ROWH; }
static int   note_cy(int s)  { return row_y(n_deg[s]) + ROWH / 2; }
static float base_midi(int s){ return BASE_MIDI + PENT[n_deg[s]]; }

// which note (start col) covers cell `col`, or -1
static int note_at(int col) {
    for (int s = 0; s <= col && s < NCOL; s++)
        if (n_on[s] && col < s + n_len[s]) return s;
    return -1;
}
// longest length a note at s may have (up to the next note's start)
static int max_len(int s) {
    for (int k = s + 1; k < NCOL; k++) if (n_on[k]) return k - s;
    return NCOL - s;
}

static float bend_env(int m, float t) {   // pitch offset -1..1 along the note
    switch (m) {
        case BM_HUMP:  { float u = t * 2.0f - 1.0f; return 1.0f - u * u; }
        case BM_SCOOP: return t < 0.35f ? -(1.0f - t / 0.35f) : 0.0f;
        case BM_HOLD:  return t < 0.30f ? 0.0f : t < 0.60f ? (t - 0.30f) / 0.30f : 1.0f;
        case BM_VIB:   return 0.6f + 0.4f * sinf(t * 18.0f);
    }
    return 0.0f;
}

// ── voice (mono, glide-aware pizz) ──
static void setup_pizz(void) {
    instrument(BASS, INSTR_BOWED, 4, 0, 7, ring_ms());
    instrument_mode(BASS, MODE_BOW_PIZZ, 1.0f);
    instrument_filter(BASS, FILTER_LOW, tone_hz(), 3);
    instrument_harmonics(BASS, 0.30f);
    instrument_timbre(BASS, 0.30f);
    instrument_morph(BASS, 0.45f);
}
static void retune(void) { instrument_filter(BASS, FILTER_LOW, tone_hz(), 3); if (voice >= 0) note_cutoff(voice, tone_hz()); }
static void pluck(float midi, int vol) {
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(midi + 0.5f), BASS, vol);
    note_pitch(voice, midi);
    note_glide(voice, glide_ms());
}
static void slur(float midi) { if (voice < 0) { pluck(midi, 5); return; } note_glide(voice, glide_ms()); note_pitch(voice, midi); }
static void silence(void) { if (voice >= 0) note_off(voice); voice = -1; active_col = -1; }

static void gen_random(void) {
    for (int s = 0; s < NCOL; s++) { n_on[s]=n_deg[s]=n_len[s]=n_slide[s]=n_bmode[s]=0; n_bend[s]=0; }
    int s = 0;
    while (s < NCOL) {
        if (rnd_between(0, 100) < 20) { s++; continue; }        // a rest
        n_on[s] = 1; n_deg[s] = rnd_between(0, NDEG);
        int len = rnd_between(0, 100) < 30 ? 2 : 1; if (s + len > NCOL) len = 1;
        n_len[s] = len;
        n_slide[s] = rnd_between(0, 100) < 22;
        if (rnd_between(0, 100) < 18) { n_bmode[s] = BM_HUMP; n_bend[s] = 0.4f + rnd_between(0, 40) / 100.0f; }
        s += len;
    }
}

void init(void) {
    static const int don[NCOL] = { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 };
    static const int dde[NCOL] = { 0,0,2,0, 4,0,1,0, 0,0,2,0, 5,0,3,0 };
    static const int dln[NCOL] = { 2,0,1,0, 1,0,1,0, 2,0,1,0, 1,0,1,0 };
    static const int dsl[NCOL] = { 0,0,1,0, 0,0,0,0, 0,0,1,0, 0,0,0,0 };
    for (int s = 0; s < NCOL; s++) { n_on[s]=don[s]; n_deg[s]=dde[s]; n_len[s]=dln[s]?dln[s]:1; n_slide[s]=dsl[s]; n_bmode[s]=0; n_bend[s]=0; }
    n_bmode[4] = BM_HUMP; n_bend[4] = 0.6f;              // a demo bend on the 3rd note
    setup_pizz();
    reverb(0.30f, 0.55f);
    instrument_reverb(BASS, 0.16f);
}

// ── playback ──
static void fire(int cur, int raw) {
    float m = base_midi(cur);
    if (active_col >= 0 && n_slide[active_col] && voice >= 0) slur(m);   // slur from the ringing note
    else pluck(m, 6);
    active_col = cur; active_start = raw; active_len = n_len[cur]; active_until = raw + n_len[cur];
}

void update(void) {
    bpm(60 + (int)(k_tempo * 120));

    if (keyp(KEY_SPACE)) { playing = !playing; if (playing) last_16 = -1; else silence(); }

    if (playing) {
        float p4 = beat_pos() * 4.0f;
        int   raw = beat() * 4 + (int)p4;
        float frac = p4 - (int)p4;
        if (raw != last_16) {
            last_16 = raw; cur_step = raw % NCOL;
            if (n_on[cur_step]) fire(cur_step, raw);
            else if (voice >= 0 && raw >= active_until) { note_off(voice); voice = -1; active_col = -1; }
        }
        if (voice >= 0 && active_col >= 0) {            // continuous bend on the sounding note
            float t = (raw + frac - active_start) / (float)active_len; t = clamp(t, 0, 1);
            float off = bend_env(n_bmode[active_col], t) * n_bend[active_col] * 2.0f;
            note_pitch(voice, base_midi(active_col) + off);
        }
    }

#ifdef DE_TRACE
    watch("step", "%d", cur_step);
    watch("sel", "%d", sel);
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
    float a = -2.356194f + *v * 4.712389f;
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

// a capsule that fades from a bright pluck-head to a dark ring-tail as it lingers
static void capsule(int s, int selected) {
    int y = row_y(n_deg[s]) + 1, h = ROWH - 2, len = n_len[s];
    for (int i = 0; i < len; i++) {
        int bx = cell_x(s + i) + 1, bw = COLW - (i == len - 1 ? 2 : 0);
        int c = i == 0 ? CLR_PEACH : i == 1 ? CLR_DARK_PEACH : CLR_BROWN;
        rectfill(bx, y, bw, h, c);
    }
    int x = cell_x(s) + 1, w = len * COLW - 2;
    rrect(x, y, w, h, 1, selected ? CLR_LIGHT_YELLOW : CLR_BROWNISH_BLACK);
    rectfill(x, y, 2, h, CLR_LIGHT_PEACH);
}
static void draw_contour(int s) {
    if (n_bmode[s] == BM_NONE || n_bend[s] <= 0.02f) return;
    int x0 = cell_x(s) + 2, x1 = cell_x(s + n_len[s]) - 2, base = note_cy(s);
    float amp = n_bend[s] * ROWH * 1.5f;
    int px = x0, py = base - (int)(bend_env(n_bmode[s], 0) * amp);
    for (int x = x0 + 1; x <= x1; x++) {
        float t = (float)(x - x0) / (x1 - x0);
        int y = base - (int)(bend_env(n_bmode[s], t) * amp + 0.5f); if (y < GY) y = GY;
        line(px, py, x, y, CLR_ORANGE);
        px = x; py = y;
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);
    ui_begin();

    // ── grid: rows (pitch) + beat columns (time) ──
    rectfill(GX0 - 2, GY - 2, GW + 4, GH + 4, CLR_BROWNISH_BLACK);
    font(FONT_SMALL);
    for (int d = 0; d < NDEG; d++) {
        int y = row_y(d);
        rectfill(GX0, y, GW, ROWH, (d & 1) ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK);
        print(SLAB[d], 10, y + 3, CLR_DARK_PEACH);
    }
    font(FONT_NORMAL);
    for (int c = 0; c <= NCOL; c += 4) line(cell_x(c), GY, cell_x(c), GY + GH, CLR_DARK_BROWN);

    // ── the grid capture widget (placement / select / move / delete) ──
    // registered FIRST so the selected note's grabbers (drawn later) win the press.
    {
        void *w = ui_wid_hash(0x6710u, GX0, GY, GW, GH);
        ui_reg(w, GX0, GY, GW, GH, 0);
        UiCap *c = ui_cap_for(w);
        if (c) {
            int px = c->released ? c->rx : c->cx, py = c->released ? c->ry : c->cy;
            int col = (px - GX0) / COLW; col = col < 0 ? 0 : col > NCOL - 1 ? NCOL - 1 : col;
            float frac = clamp((GY + GH - py) / (float)GH, 0, 1);
            int row = (int)(frac * (NDEG - 1) + 0.5f);
            if (ui_grabbed(w)) {                          // press: select existing, else place
                int at = note_at(col);
                if (at < 0) { n_on[col]=1; n_deg[col]=row; n_len[col]=1; n_slide[col]=0; n_bmode[col]=0; n_bend[col]=0; sel=col; if (!playing) pluck(base_midi(col), 6); }
                else { sel = at; }
            } else if (sel >= 0) {                         // drag body: move (pitch always; col if free)
                if (py > GY + GH + 4) { n_on[sel]=0; sel=-1; }   // dragged off the bottom → delete
                else {
                    if (n_deg[sel] != row) { n_deg[sel]=row; if (!playing && !c->released) pluck(base_midi(sel), 6); }
                    if (col != sel && note_at(col) < 0 && col + n_len[sel] <= NCOL) {   // relocate to a free col
                        n_on[col]=1; n_deg[col]=n_deg[sel]; n_len[col]=n_len[sel]; n_slide[col]=n_slide[sel]; n_bmode[col]=n_bmode[sel]; n_bend[col]=n_bend[sel];
                        n_on[sel]=0; sel=col;
                    }
                }
            }
        }
    }

    // ── notes: capsule + contour, slide diagonal on top ──
    for (int s = 0; s < NCOL; s++) if (n_on[s]) capsule(s, s == sel);
    for (int s = 0; s < NCOL; s++) if (n_on[s]) draw_contour(s);
    for (int s = 0; s < NCOL; s++) {                       // slide: a ramp into the next note
        if (!n_on[s] || !n_slide[s]) continue;
        int ns = -1; for (int k = s + n_len[s]; k < NCOL; k++) if (n_on[k]) { ns = k; break; }
        if (ns < 0) continue;
        int x0 = cell_x(s + n_len[s]) - 1, y0 = note_cy(s), x1 = cell_x(ns) + 1, y1 = note_cy(ns);
        line(x0, y0, x1, y1, CLR_LIGHT_YELLOW); line(x0, y0 - 1, x1, y1 - 1, CLR_LIGHT_YELLOW);
    }

    // ── the SELECTED note's grabbers (registered LAST → they win overlapping presses) ──
    if (sel >= 0 && n_on[sel]) {
        int ncx = cell_x(sel) + (n_len[sel] * COLW) / 2;
        int nrx = cell_x(sel + n_len[sel]);
        int ncy = note_cy(sel), ntop = row_y(n_deg[sel]);

        // (1) BEND knob on a stem — drag up = depth (hump)
        int stem = 6 + (int)(n_bend[sel] * 26);
        int bkx = ncx, bky = ntop - stem;
        {
            void *w = ui_wid_hash(0x6720u, bkx - 5, bky - 5, 11, 11);
            ui_reg(w, bkx - 5, bky - 5, 11, 11, 1);
            UiCap *c = ui_cap_for(w);
            if (c) {
                if (!c->has_v0) { c->has_v0 = 1; c->v0 = n_bend[sel]; c->by = c->cy; }
                int py = c->released ? c->ry : c->cy;
                n_bend[sel] = clamp(c->v0 + (c->by - py) / 60.0f, 0, 1);
                if (n_bend[sel] > 0.03f && (n_bmode[sel] == BM_NONE)) n_bmode[sel] = BM_HUMP;
                if (n_bend[sel] <= 0.03f) n_bmode[sel] = BM_NONE;
                stem = 6 + (int)(n_bend[sel] * 26); bky = ntop - stem;
            }
            line(bkx, ntop, bkx, bky + 3, CLR_LIGHT_YELLOW);
            circfill(bkx, bky, 3, CLR_BROWNISH_BLACK); circfill(bkx, bky, 2, CLR_LIGHT_YELLOW); pset(bkx-1, bky-1, CLR_WHITE);
        }

        // (2) LENGTH tab — right edge, drag sideways
        {
            int tx = nrx - 4, ty = ntop;
            void *w = ui_wid_hash(0x6730u, tx, ty - 2, 10, ROWH + 4);
            ui_reg(w, tx, ty - 2, 10, ROWH + 4, 1);
            UiCap *c = ui_cap_for(w);
            if (c) {
                int px = c->released ? c->rx : c->cx;
                int nl = (px - cell_x(sel)) / COLW + 1;
                int mx = max_len(sel); nl = nl < 1 ? 1 : nl > mx ? mx : nl;
                n_len[sel] = nl; nrx = cell_x(sel + nl);
            }
            rectfill(nrx - 3, ty, 3, ROWH - 2, CLR_WHITE);
        }

        // (3) SLIDE nub — tip, tap to toggle
        {
            int sx = nrx + 4;
            void *w = ui_wid_hash(0x6740u, sx - 4, ncy - 4, 9, 9);
            ui_reg(w, sx - 4, ncy - 4, 9, 9, 1);
            if (ui_grabbed(w)) n_slide[sel] = !n_slide[sel];
            circfill(sx, ncy, 3, CLR_BROWNISH_BLACK); circfill(sx, ncy, 2, n_slide[sel] ? CLR_LIME_GREEN : CLR_ORANGE);
        }

        // (4) SCOOP corner — bottom-left, tap to toggle a scoop-in
        {
            int cxp = cell_x(sel), cyp = row_y(n_deg[sel]) + ROWH;
            void *w = ui_wid_hash(0x6750u, cxp - 4, cyp - 4, 9, 9);
            ui_reg(w, cxp - 4, cyp - 4, 9, 9, 1);
            if (ui_grabbed(w)) {
                if (n_bmode[sel] == BM_SCOOP) { n_bmode[sel] = BM_NONE; n_bend[sel] = 0; }
                else { n_bmode[sel] = BM_SCOOP; if (n_bend[sel] < 0.3f) n_bend[sel] = 0.6f; }
            }
            circfill(cxp, cyp, 3, CLR_BROWNISH_BLACK); circfill(cxp, cyp, 2, n_bmode[sel] == BM_SCOOP ? CLR_LIME_GREEN : CLR_LIGHT_PEACH);
        }
    }

    // ── playhead ──
    if (playing) { int ph = cell_x(cur_step); line(ph, GY, ph, GY + GH, CLR_LIGHT_YELLOW); }

    // ── top strip: title · transport · key · RND · CLR ──
    rectfill(0, 0, SCREEN_W, 22, CLR_DARK_BROWN);
    line(0, 21, SCREEN_W, 21, CLR_BROWNISH_BLACK);
    print("WALK-ROLL", 6, 3, CLR_LIGHT_PEACH);
    font(FONT_SMALL); print("bass piano-roll", 6, 13, CLR_DARK_PEACH); font(FONT_NORMAL);
    {   // PLAY / STOP
        int x = 100, y = 3, w = 16, h = 16, f = 0, p = 0, hot = 0;
        void *wid = ui_wid_hash(0x50u, x, y, w, h);
        if (ui_button_core(wid, x, y, w, h, &f, &p, &hot)) { playing = !playing; if (playing) last_16 = -1; else silence(); }
        rectfill(x, y, w, h, CLR_BROWNISH_BLACK); rect(x, y, w, h, hot ? CLR_LIGHT_YELLOW : CLR_DARK_PEACH);
        if (playing) rectfill(x + 5, y + 4, 6, 8, CLR_ORANGE);
        else         tri(x + 5, y + 4, x + 5, y + 12, x + 12, y + 8, CLR_LIME_GREEN);
    }
    font(FONT_SMALL);
    { char b[16]; int t = 60 + (int)(k_tempo * 120); b[0]='0'+t/100; b[1]='0'+(t/10)%10; b[2]='0'+t%10; b[3]=' '; b[4]='B';b[5]='P';b[6]='M';b[7]=0; print(b, 120, 4, CLR_LIGHT_PEACH); }
    print("KEY E", 120, 13, CLR_DARK_PEACH);
    font(FONT_NORMAL);
    if (wbtn(0x62u, 260, 4, 26, 14, "RND", 0)) { gen_random(); sel = -1; }
    if (wbtn(0x63u, 290, 4, 26, 14, "CLR", 0)) { for (int s=0;s<NCOL;s++){n_on[s]=n_slide[s]=n_bmode[s]=0;n_bend[s]=0;n_len[s]=1;} sel=-1; }

    // ── bottom strip: knobs + hint ──
    int ky = 176;
    wknob(0x70u, &k_glide, 30,  ky, 7, "GLIDE", 0);
    wknob(0x71u, &k_tempo, 74,  ky, 6, "TEMPO", 0);
    wknob(0x72u, &k_tone,  114, ky, 6, "TONE",  0);
    wknob(0x73u, &k_ring,  152, ky, 6, "RING",  0);
    font(FONT_SMALL);
    print("tap=place  drag=move", 184, ky - 7, CLR_DARK_PEACH);
    print("off-bottom = delete", 184, ky + 1, CLR_DARK_PEACH);
    print("picked: knob edge tip corner", 184, ky + 9, CLR_DARK_PEACH);
    font(FONT_NORMAL);

    if (k_tone != ap_tone) { retune();    ap_tone = k_tone; }
    if (k_ring != ap_ring) { setup_pizz(); ap_ring = k_ring; }

    ui_end();
}
