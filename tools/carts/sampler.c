/* de:meta
{
  "slug": "sampler",
  "title": "sampler",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "instrument",
    "probe"
  ],
  "genre": null,
  "teaches": [
    "gesture-loop"
  ],
  "lineage": "The cart on the REAL PCM sampler (engine: record_arm/record_grab capture ring + INSTR_SAMPLE + instrument_sample_region + sample_peaks/record_peaks, mic-and-sampling.md). Records the console's OWN output; slices it at the note-on times captured while playing (keybed_on_note - exact, since we played it in), editable by hand, with a signal detector as fallback for un-played audio; plays two ways — KEYS (one slice chromatically across the keybed) or KIT (each slice on its own pad at original pitch). Ported in spirit from navkit's DAW sampler.",
  "description": {
    "summary": "Record what you play, then chop it and play it back — two ways. Hit REC and play (pick a synth: SAW/FM/PLUCK/EPIANO/FLUTE/REED/BRASS). STOP auto-slices the recording on its transients into chops, drawn as markers you can drag/add/remove. Then flip between KEYS (the keybed pitches ONE selected slice) and KIT (each slice is a pad, fired at original pitch). The source is the console's own sound, no mic.",
    "detail": "The two sampler paradigms behind one toggle. REC captures the master output into a ring (record_arm) AND logs every note-on time as you play (keybed_on_note) — so STOP slices EXACTLY where you triggered notes (no fragile transient-guessing; a signal detector is only the fallback for audio we didn't play in, e.g. a future loaded WAV). The grab is peak-normalized + silence-trimmed. Drag a marker to move it, drag onto a neighbour to delete, SPLIT halves the selected slice, AUTO re-slices; click a slice to select AND hear it. KEYS mode: the keybed plays the SELECTED slice pitched (C4 = original) — the melodic sampler. KIT mode: the bottom becomes a PAD GRID, one pad per slice on the same A/S/D.. letters, fired at original pitch — the SP-404/break-chopper. One-shot forward playback (loop/reverse next).",
    "controls": "REC/STOP (or R) — record a take (auto silence-trimmed). On the waveform: drag markers to move, drag onto a neighbour to delete, click a slice to select it. Buttons: KEYS|KIT toggle (or M), AUTO (re-slice on transients), SPLIT (halve the selected slice), ENGINE (source synth), RE-REC. KEYS mode: A S D F... play the selected slice pitched. KIT mode: the SAME letters (A=pad1, S=pad2...) fire the slices at original pitch; or click a pad."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "ui.h"
#include <stdio.h>

// ── SAMPLER ──────────────────────────────────────────────────────────────────
// Record the console's own output, transient-slice it into chops, and play them
// KEYS (one slice, pitched across the keybed) or KIT (a pad per slice, original
// pitch). No mic — you sample the machine. mic-and-sampling.md.

#define SYN    5          // source synth (feeds the capture ring)
#define KEYS_S 6          // chromatic sampler voice (KEYS mode)
#define KIT0   8          // KIT pads use slots KIT0..KIT0+nsl-1
#define SND    0          // PCM sample slot
#define ROOT   60         // C4 = original speed
#define MAXSLICE 12
#define MAXREC 7.5f

#define PX 6
#define PY 26
#define PW (SCREEN_W - 12)
#define PH 50
#define BY 104            // bottom play-surface top

enum { ST_PLAY, ST_REC, ST_EDIT };
enum { MODE_KEYS, MODE_KIT };

typedef struct { int instr; const char *name; int a, d, s, r; } Voice;
static const Voice SRC[] = {
    { INSTR_SAW,    "SAW",   8, 140, 6, 220 },
    { INSTR_FM,     "FM",    4, 200, 5, 300 },
    { INSTR_PLUCK,  "PLUCK", 1,   0, 7, 700 },   // self-decaying: amp sustains, the ring dies on its own
    { INSTR_EPIANO, "EPIANO",1,   0, 7, 450 },
    { INSTR_PIPE,   "FLUTE", 40,  0, 6, 200 },   // bore/held (self-oscillating) — sustain while gated
    { INSTR_REED,   "REED",  20,  0, 6, 220 },
    { INSTR_BRASS,  "BRASS", 30,  0, 6, 220 },
};
#define NSRC ((int)(sizeof SRC / sizeof *SRC))

// pads reuse the KEYBED's own play letters (white row L->R, then a few more) so the same
// fingers that pitch a slice in KEYS mode fire the pads in KIT mode.
static const char PADKEYS[] = "ASDFGHJKLZXCV";
static char pad_key(int i) { return (i >= 0 && i < (int)sizeof(PADKEYS) - 1) ? PADKEYS[i] : 0; }

static int   eng   = 0;
static int   state = ST_PLAY;
static int   mode  = MODE_KEYS;
static float rec_t0 = 0.0f, rec_secs = 0.0f;
static int   rec_len = 0;
static float bnd[MAXSLICE + 1];   // slice boundaries (ascending, bnd[0]=0, bnd[nb-1]=1)
static int   nb = 2;              // number of boundaries; nsl = nb-1 slices
static int   sel = 0;            // selected/highlighted slice
static int   grabbed = -1;       // dragging boundary index (interior), -1 = none
static float wf_lo[PW], wf_hi[PW];
static float sc[PW];
static float on_t[MAXSLICE + 2];   // note-on times (sec since rec_t0) captured live while recording
static int   on_n = 0;             // how many — we KNOW when notes fired (we played them), so slices are exact

// keybed fires this on every fresh note-on; log the moment while recording (the honest slice boundaries)
static void rec_note_cb(int midi, int vel) { (void)midi; (void)vel; if (state == ST_REC && on_n < MAXSLICE + 1) on_t[on_n++] = now() - rec_t0; }

static int nsl(void) { return nb - 1; }
static void apply_src(void) { const Voice *v = &SRC[eng]; instrument(SYN, v->instr, v->a, v->d, v->s, v->r); }
static void point_keybed(int slot) { keybed_config(slot, 4, 14); keybed_layout(0, BY, SCREEN_W, SCREEN_H - BY); }

static void rebind(void) {   // (re)configure every slice's voice + the KEYS voice's region
    int n = nsl();
    for (int i = 0; i < n && i < MAXSLICE; i++) {
        instrument(KIT0 + i, INSTR_SAMPLE, 1, 0, 7, 150);
        instrument_sample(KIT0 + i, SND, ROOT);
        instrument_sample_region(KIT0 + i, bnd[i], bnd[i + 1]);
    }
    if (sel < 0) sel = 0; if (sel >= n) sel = n - 1;
    instrument_sample(KEYS_S, SND, ROOT);                 // BIND the KEYS voice to the buffer (not just its region — else it's silent)
    instrument_sample_region(KEYS_S, bnd[sel], bnd[sel + 1]);
}

static void detect_slices(void) {   // transient auto-slice: an attack is a sharp RISE in energy (positive
    float env[PW], mx = 0.001f;      // slope), which shows even over a ringing tail — unlike a level threshold.
    for (int x = 0; x < PW; x++) {
        float a = wf_hi[x] < 0 ? -wf_hi[x] : wf_hi[x], b = wf_lo[x] < 0 ? -wf_lo[x] : wf_lo[x];
        env[x] = a > b ? a : b; if (env[x] > mx) mx = env[x];
    }
    for (int x = 1; x < PW; x++) env[x] = env[x] * 0.6f + env[x - 1] * 0.4f;   // light causal smooth (de-noise)
    nb = 0; bnd[nb++] = 0.0f;        // slice 1 starts at 0 (buffer begins on the first attack)
    int last = 0, mingap = PW / 28, D = 5;
    float rise_thr = 0.13f * mx, floor = 0.10f * mx;
    for (int x = D; x < PW; x++) {
        float rise = env[x] - env[x - D];
        if (x > mingap && rise > rise_thr && env[x] > floor && (x - last) > mingap && nb < MAXSLICE) {
            bnd[nb++] = (float)x / PW; last = x;              // a fresh attack (energy jumped up over D columns)
        }
    }
    bnd[nb++] = 1.0f;
    if (nb < 2) { bnd[0] = 0.0f; bnd[1] = 1.0f; nb = 2; }
    sel = 0;
}

// PRIMARY slicer: use the note-on times we captured while playing — exact, no false hits. The first
// note-on ~ where the lead silence was trimmed, so boundaries are relative to it. Falls back to the
// signal detector when there are no events (a future loaded WAV we didn't play in).
static void build_slices(void) {
    if (on_n < 1) { detect_slices(); return; }
    float t0 = on_t[0];
    nb = 0; bnd[nb++] = 0.0f;
    for (int i = 1; i < on_n && nb < MAXSLICE; i++) {
        float f = rec_secs > 0.0f ? (on_t[i] - t0) / rec_secs : 0.0f;
        if (f > 0.01f && f < 0.99f && f > bnd[nb - 1] + 0.01f) bnd[nb++] = f;
    }
    bnd[nb++] = 1.0f;
    if (nb < 2) { bnd[0] = 0.0f; bnd[1] = 1.0f; nb = 2; }
    sel = 0;
}

static void stop_take(void) {
    rec_secs = now() - rec_t0;
    if (rec_secs > MAXREC) rec_secs = MAXREC;
    if (rec_secs < 0.05f)  rec_secs = 0.05f;
    rec_len = record_grab(SND, rec_secs);
    if (rec_len > 0) {
        rec_secs = rec_len / 44100.0f;                 // use the TRIMMED length for timing/display
        sample_peaks(SND, wf_lo, wf_hi, PW);
        build_slices();
        rebind();
        state = ST_EDIT;
        if (mode == MODE_KEYS) point_keybed(KEYS_S);
    } else {
        state = ST_PLAY;
    }
}

// ── editing the markers (mouse, in the waveform panel) ──
static void edit_markers(void) {
    int mx = mouse_x(), my = mouse_y();
    int inpanel = (my >= PY - 5 && my <= PY + PH + 5 && mx >= PX && mx <= PX + PW);
    if (mouse_pressed(0) && inpanel) {
        int nearest = -1, best = 8;                   // grab the nearest interior marker
        for (int i = 1; i < nb - 1; i++) {
            int bx = PX + (int)(bnd[i] * PW), d = mx - bx < 0 ? bx - mx : mx - bx;
            if (d < best) { best = d; nearest = i; }
        }
        if (nearest >= 0) grabbed = nearest;
        else {                                        // tap in a slice = select it AND audition it
            float f = (mx - PX) / (float)PW;
            for (int i = 0; i < nb - 1; i++) if (f >= bnd[i] && f < bnd[i + 1]) {
                sel = i; rebind();
                int ms = (int)((bnd[i + 1] - bnd[i]) * rec_secs * 1000.0f); if (ms < 30) ms = 30;
                hit(ROOT, KIT0 + i, 6, ms);           // hear the chop you clicked
                break;
            }
        }
    }
    if (grabbed >= 0) {
        float f = (mx - PX) / (float)PW; if (f < 0) f = 0; if (f > 1) f = 1;
        bnd[grabbed] = f;
        if (!mouse_down(0)) {                          // release: sort, drop a marker dragged onto a neighbour
            for (int i = 1; i < nb - 1; i++)           // bubble sort (tiny)
                for (int j = 1; j < nb - 1 - 1; j++)
                    if (bnd[j] > bnd[j + 1]) { float t = bnd[j]; bnd[j] = bnd[j + 1]; bnd[j + 1] = t; }
            for (int i = 1; i < nb - 1; i++)
                if (bnd[i + 1] - bnd[i] < 0.02f || bnd[i] - bnd[i - 1] < 0.02f) {   // too close → delete i
                    for (int j = i; j < nb - 1; j++) bnd[j] = bnd[j + 1];
                    nb--; i--;
                }
            grabbed = -1;
            rebind();
        }
    }
}

static void pad_rect(int i, int n, int *x, int *y, int *w, int *h) {
    int cols = n <= 8 ? (n < 1 ? 1 : n) : 8, rows = (n + cols - 1) / cols;
    int gx = 4, gy = BY, gw = SCREEN_W - 8, gh = SCREEN_H - BY - 2;
    int c = i % cols, rr = i / cols;
    *w = gw / cols - 3; *h = gh / rows - 3;
    *x = gx + c * (gw / cols); *y = gy + rr * (gh / rows);
}
static int pad_at(int x, int y) {   // which pad contains (x,y), or -1
    int n = nsl();
    for (int i = 0; i < n; i++) { int px, py, pw, ph; pad_rect(i, n, &px, &py, &pw, &ph);
        if (x >= px && x <= px + pw && y >= py && y <= py + ph) return i; }
    return -1;
}
static void fire_pad(int i) {
    if (i < 0 || i >= nsl()) return;
    int ms = (int)((bnd[i + 1] - bnd[i]) * rec_secs * 1000.0f); if (ms < 30) ms = 30;
    hit(ROOT, KIT0 + i, 6, ms); sel = i;
}

void update(void) {
    if (keyp('[') || keyp(']')) { eng = (eng + (keyp(']') ? 1 : NSRC - 1)) % NSRC; if (state != ST_EDIT) apply_src(); }

    if (state == ST_PLAY) {
        keybed_update();
        if (keyp('R')) { state = ST_REC; rec_t0 = now(); on_n = 0; }
    } else if (state == ST_REC) {
        keybed_update();
        if (keyp('R') || now() - rec_t0 >= MAXREC) stop_take();
    } else { // ST_EDIT
        if (keyp('R')) { state = ST_PLAY; apply_src(); point_keybed(SYN); return; }
        if (keyp('M')) { mode ^= 1; if (mode == MODE_KEYS) point_keybed(KEYS_S); }   // KEYS<->KIT
        edit_markers();
        if (mode == MODE_KEYS) {
            keybed_update();
        } else { // MODE_KIT — pads (MULTITOUCH: a fresh finger on a pad fires it; mouse = one finger)
            static int prev[16], pn = 0;
            int cur[16], cn = 0;
            for (int t = 0; t < touch_count() && cn < 16; t++) {
                int id = touch_id(t); cur[cn++] = id;
                int seen = 0; for (int k = 0; k < pn; k++) if (prev[k] == id) seen = 1;
                if (!seen) fire_pad(pad_at(touch_x(t), touch_y(t)));   // newly-down finger → its pad
            }
            pn = cn; for (int k = 0; k < cn; k++) prev[k] = cur[k];
            int n = nsl();                                            // + the ASDF letters (keyboard polyphony)
            for (int i = 0; i < n; i++) { char k = pad_key(i); if (k && keyp(k)) fire_pad(i); }
        }
    }
}

static void draw_panel(void) {
    rectfill(PX, PY, PW, PH, CLR_DARKER_GREY);
    int mid = PY + PH / 2;
    line(PX, mid, PX + PW - 1, mid, CLR_DARK_GREY);
    int half = PH / 2 - 1;

    if (state == ST_EDIT && rec_len > 0) {
        int n = nsl();
        for (int x = 0; x < PW; x++) {
            float f = (float)x / PW;
            int si = 0; for (int i = 0; i < n; i++) if (f >= bnd[i] && f < bnd[i + 1]) { si = i; break; }
            int c = (si == sel) ? CLR_LIME_GREEN : ((si & 1) ? CLR_MEDIUM_GREEN : CLR_BLUE_GREEN);
            line(PX + x, mid - (int)(wf_hi[x] * half), PX + x, mid - (int)(wf_lo[x] * half), c);
        }
        for (int i = 1; i < nb - 1; i++) {            // interior markers
            int bx = PX + (int)(bnd[i] * PW);
            line(bx, PY - 3, bx, PY + PH + 3, CLR_YELLOW); rectfill(bx - 2, PY - 4, 5, 4, CLR_YELLOW);
        }
        for (int i = 0; i < n; i++) {                 // slice numbers
            int cx = PX + (int)((bnd[i] + bnd[i + 1]) * 0.5f * PW);
            char t[3]; snprintf(t, sizeof t, "%d", i + 1);
            print(t, cx - text_width(t) / 2, PY + 1, i == sel ? CLR_WHITE : CLR_LIGHT_GREY);
        }
        for (int i = 0; i < n + 1; i++) {             // live playheads: each sounding slice voice (+ KEYS)
            float p = instrument_playhead(i < n ? KIT0 + i : KEYS_S);
            if (p >= 0.0f) { int hx = PX + (int)(p * PW);
                line(hx, PY, hx, PY + PH - 1, CLR_WHITE); line(hx + 1, PY, hx + 1, PY + PH - 1, CLR_WHITE); }
        }
    } else {
        float elapsed = state == ST_REC ? now() - rec_t0 : 0.0f;
        if (state == ST_REC && elapsed > MAXREC) elapsed = MAXREC;
        int cols = state == ST_REC ? (int)(elapsed / MAXREC * PW) : 0;
        if (cols > PW) cols = PW;
        static float ph = 0.08f;                       // display PEAK-HOLD: only grows during a take, so
        if (cols < 3) ph = 0.08f;                      // the scale settles instead of re-normalizing every frame (jumpy)
        if (state == ST_REC && cols >= 1 && record_peaks(elapsed, wf_lo, wf_hi, cols) > 0) {
            for (int x = 0; x < cols; x++) { float a = wf_hi[x] < 0 ? -wf_hi[x] : wf_hi[x], b = wf_lo[x] < 0 ? -wf_lo[x] : wf_lo[x]; if (a > ph) ph = a; if (b > ph) ph = b; }
            float g = 0.9f / ph;
            for (int x = 0; x < cols; x++) line(PX + x, mid - (int)(wf_hi[x] * g * half), PX + x, mid - (int)(wf_lo[x] * g * half), CLR_RED);
            line(PX + cols, PY, PX + cols, PY + PH - 1, CLR_LIGHT_GREY);
        } else {                                       // idle monitor
            scope_read(sc, PW);
            int py = mid - (int)(sc[0] * half);
            for (int x = 1; x < PW; x++) { int y = mid - (int)(sc[x] * half); line(PX + x - 1, py, PX + x, y, CLR_BLUE_GREEN); py = y; }
        }
    }
    rect(PX, PY, PW, PH, CLR_MEDIUM_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    char buf[96];

    print("SAMPLER", 4, 2, CLR_WHITE);
    const char *sn = state == ST_PLAY ? "PLAY" : state == ST_REC ? "REC" : (mode == MODE_KEYS ? "EDIT/KEYS" : "EDIT/KIT");
    print(sn, SCREEN_W - text_width(sn) - 4, 2, state == ST_REC ? CLR_RED : state == ST_EDIT ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    if (state == ST_PLAY)      snprintf(buf, sizeof buf, "%s: press REC and play", SRC[eng].name);
    else if (state == ST_REC)  snprintf(buf, sizeof buf, "recording... %.1fs  (STOP to keep)", now() - rec_t0);
    else                       snprintf(buf, sizeof buf, "%d chops  drag/split markers  %s", nsl(),
                                        mode == MODE_KEYS ? "keys pitch slice" : "A S D.. fire pads");
    print(buf, 4, 13, state == ST_REC ? CLR_RED : CLR_LIGHT_GREY);

    draw_panel();

    ui_begin();
    int y = PY + PH + 5;
    if (state == ST_EDIT) {
        if (ui_button(4,   y, 62, 18, "RE-REC")) { state = ST_PLAY; apply_src(); point_keybed(SYN); }
        if (ui_button(70,  y, 62, 18, mode == MODE_KEYS ? "KEYS" : "KIT")) {
            mode ^= 1; if (mode == MODE_KEYS) point_keybed(KEYS_S);
        }
        if (ui_button(136, y, 50, 18, "AUTO"))  { build_slices(); rebind(); }
        if (ui_button(190, y, 50, 18, "SPLIT") && nb <= MAXSLICE) {   // halve the selected slice
            float m = (bnd[sel] + bnd[sel + 1]) * 0.5f;
            for (int j = nb; j > sel + 1; j--) bnd[j] = bnd[j - 1];
            bnd[sel + 1] = m; nb++; rebind();
        }
    } else {
        if (ui_button(4, y, 90, 18, state == ST_REC ? "STOP" : "REC")) {
            if (state == ST_PLAY) { state = ST_REC; rec_t0 = now(); on_n = 0; } else stop_take();
        }
        if (ui_button(SCREEN_W - 66, y, 62, 18, SRC[eng].name)) { eng = (eng + 1) % NSRC; apply_src(); }
    }
    ui_end();

    // bottom play surface
    if (state == ST_EDIT && mode == MODE_KIT) {
        int n = nsl();
        for (int i = 0; i < n; i++) {
            int px, py, pw, ph; pad_rect(i, n, &px, &py, &pw, &ph);
            rectfill(px, py, pw, ph, i == sel ? CLR_DARK_GREEN : CLR_DARKER_GREY);
            rect(px, py, pw, ph, CLR_MEDIUM_GREY);
            char k = pad_key(i), t[2] = { k ? k : (char)('1' + i), 0 };
            print(t, px + pw / 2 - text_width(t) / 2, py + ph / 2 - 3, CLR_WHITE);
            char sn2[3]; snprintf(sn2, sizeof sn2, "%d", i + 1);   // small slice # in the corner
            print(sn2, px + 2, py + 2, CLR_MEDIUM_GREY);
        }
    } else {
        keybed_draw();
    }
}

void init(void) {
    apply_src();
    instrument(KEYS_S, INSTR_SAMPLE, 1, 0, 7, 150);
    record_arm();
    keybed_on_note(rec_note_cb);   // log note-on moments while recording → exact slice boundaries
    bnd[0] = 0.0f; bnd[1] = 1.0f; nb = 2;
    point_keybed(SYN);
}
