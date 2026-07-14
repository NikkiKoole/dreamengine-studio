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
    "summary": "Record what you play, chop it, and build a SONG out of the chops. Hit REC and play a synth (SAW/FM/PLUCK/EPIANO/FLUTE/REED/BRASS) or a DRUM KIT (ELECTRO/ACOUSTIC) — the ENGINE button picks the source. STOP auto-slices the recording at the moments you triggered notes. Audition the chops, +ADD the ones you like to a 16-pad bank, then ARRANGE them on a 16-step grid that loops — record more takes (a flute part, an epiano part, drums) and they all pool into one song. The source is the console's own sound, no mic.",
    "detail": "A full loop: sample → chop → arrange. REC captures the master output into a ring (record_arm) AND logs every note-on time as you play — so STOP slices EXACTLY where you triggered notes (no fragile transient-guessing; a signal detector is only the fallback for audio we didn't play in, e.g. a future loaded WAV). Synth sources play the chromatic keybed; a DRUM KIT source (drumkit.h — a shared kit of KICK/SNARE/HAT/OPEN/CLAP/TOM/CRASH voices) swaps the keybed for a PAD GRID, so you play a beat on the pads and each hit becomes a chop — the engine has no drum instrument, so this is how you sample drums. The grab is peak-normalized + silence-trimmed. Drag a marker to move it, onto a neighbour to delete, SPLIT halves the selected slice, AUTO re-slices; click a slice to select AND hear it. Two play modes: KEYS pitches the SELECTED slice across the keybed (the melodic sampler); KIT is a pad per slice at original pitch (the SP-404 break-chopper). MODE (or V) cycles NORMAL/REVERSE/LOOP/PINGPONG. ARRANGE opens the SONG: +ADD banks the selected chop into a fixed 16-pad bank; each take grabs into its OWN sample buffer (up to 8) so chops from different takes coexist; a 16-step grid loops them off the tempo clock with a sweeping playhead. A seconds-used readout shows the sample budget (shown, not yet capped — SP-404 style). A GRIT button crushes the whole song lo-fi — CLEAN/12BIT/8BIT/CRUSH (SP-1200 bit+rate reduction applied to every chop voice; the capture stays clean, so you re-grit on playback). The whole arrangement PERSISTS — the chops' PCM + grid + tempo + grit are auto-saved and restored on the next launch (a SONG button on the record screen jumps straight to your saved song).",
    "controls": "REC/STOP (or R) — record a take. ENGINE (or [ / ]) picks the source: a synth (keybed) or a DRUM KIT (pads). Drum source: A S D F.. hit the pads (or touch). EDIT: click a slice to select+hear it; drag markers to move, onto a neighbour to delete; KEYS|KIT (M), AUTO (re-slice), SPLIT, MODE (fwd/rev/loop/pingpong), +ADD (bank the selected chop), ARRANGE (open the song), RE-REC. EDIT/KEYS: A S D.. play the selected slice pitched; EDIT/KIT: the letters fire the chops. SONG: tap grid cells to program the loop; PLAY/STOP, -/+ tempo, + TAKE (record another), GRIT (CLEAN/12BIT/8BIT/CRUSH lo-fi), EDIT (back to the take), CLEAR."
  }
}
de:meta */
#include "studio.h"
#include "keybed.h"
#include "drumkit.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── SAMPLER ──────────────────────────────────────────────────────────────────
// Record the console's own output, transient-slice it into chops, and play them
// KEYS (one slice, pitched across the keybed) or KIT (a pad per slice, original
// pitch). No mic — you sample the machine. mic-and-sampling.md.

#define SYN    5          // source synth (feeds the capture ring)
#define KEYS_S 6          // chromatic sampler voice (KEYS mode)
#define KIT0   8          // KIT pads use slots KIT0..KIT0+nsl-1
#define DKIT   20         // drum-kit source (drumkit.h) uses slots DKIT..DKIT+DK_N-1
#define SONG_V0 28        // SONG pad voices = instrument slots 28..28+SONGPADS-1
#define ROOT   60         // C4 = original speed
#define MAXSLICE 12
#define MAXREC 7.5f

// SONG: arrange committed chops on a step grid. Each take grabs into its OWN sample
// buffer (slots 0..MAXBUF-1) so chops from different takes — a flute part, an epiano
// part, drums — coexist in one song. The pad count is a FIXED limit (the constraint);
// the seconds budget is shown but not yet capped (a later one-line change).
#define MAXBUF    8       // take buffers = PCM sample slots 0..7
#define SONGPADS  16      // fixed pad-count limit — SP-404-style (constraint-as-character)
#define SONGSTEPS 16

#define PX 6
#define PY 26
#define PW (SCREEN_W - 12)
#define PH 50
#define BY 104            // bottom play-surface top

// SONG grid geometry (used by both hit-test and draw so they agree)
#define SG_X   6
#define SG_Y   28
#define SG_LBL 16         // left gutter for the pad number
#define SG_W   (SCREEN_W - 12)
#define SG_H   118

enum { ST_PLAY, ST_REC, ST_EDIT, ST_SONG };
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

// A DRUM KIT is a source too — but it plays a MAP (one sound per pad), not a chromatic
// synth, so it can't just be another Voice. Sources 0..NSRC-1 are the synths (KEYS-style
// keybed); NSRC..NSRC+NKIT-1 are the drum kits (a pad grid). drumkit.h owns the sound.
static const DrumKitDef *const DKITS[] = { &DK_ELECTRO, &DK_ACOUSTIC };
#define NKIT ((int)(sizeof DKITS / sizeof *DKITS))
#define NALL     (NSRC + NKIT)
#define IS_DRUMS(e) ((e) >= NSRC)
#define KIT_OF(e)   (DKITS[(e) - NSRC])
static const char *src_name(int e) { return IS_DRUMS(e) ? KIT_OF(e)->name : SRC[e].name; }

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

static int smode = SAMPLE_NORMAL;                         // playback mode (NORMAL/REV/LOOP/PONG)
static const char *SMODE_NM[] = { "NORMAL", "REVERSE", "LOOP", "PINGPONG" };
static const char *SMODE_SH[] = { "NORM",   "REV",     "LOOP", "PONG" };      // short (button)

// GRIT — a whole-machine lo-fi character (SP-1200 lineage): bit + sample-rate crush on every
// chop-playback voice. Capture stays clean; you crush on PLAYBACK, so a chop can be re-gritted
// after the fact (the whole reason to sample your own sound). SET-AND-HOLD — apply only on change.
static int grit = 0;
static const char  *GRIT_NM[] = { "CLEAN", "12BIT", "8BIT", "CRUSH" };
static const float  GRIT_B[]  = {  16.0f,   12.0f,   8.0f,   4.0f  };   // bit depth (16 ≈ clean)
static const float  GRIT_R[]  = {   1.0f,    2.0f,   4.0f,   8.0f  };   // sample-rate reduction (higher = darker/aliased)

static int   cur_buf = 0, take_ct = 0;    // current take's sample buffer; takes recorded (rotates the buffer)

// ── the SONG: a bank of committed chops + a step grid ──
typedef struct { int buf; float start, end, secs; } Chop;   // buf 0..MAXBUF-1 + region 0..1
static Chop  song[SONGPADS];
static int   song_n = 0;                                    // committed pads
static char  song_seq[SONGPADS][SONGSTEPS];                 // the step grid
static int   song_bpm = 100, song_step = 0, song_last16 = -1;
static int   song_play = 0;
static float song_flash[SONGPADS];
static int   song_dirty = 0;                                // a mutation happened → save on the next frame

static int nsl(void) { return nb - 1; }
static void apply_src(void) {
    if (IS_DRUMS(eng)) dk_use(KIT_OF(eng), DKIT);          // voice the kit's slots
    else { const Voice *v = &SRC[eng]; instrument(SYN, v->instr, v->a, v->d, v->s, v->r); }
}
static void point_keybed(int slot) { keybed_config(slot, 4, 14); keybed_layout(0, BY, SCREEN_W, SCREEN_H - BY); }
static void apply_mode(void) {                            // push the playback mode to every sample voice
    instrument_sample_mode(KEYS_S, smode);
    for (int i = 0; i < nsl(); i++) instrument_sample_mode(KIT0 + i, smode);
}
// Grit is the MASTER crush (SP-1200 = the whole output is lo-fi), gated by state so the CAPTURE
// stays clean: ON while auditioning/arranging (EDIT/SONG), OFF while recording the source
// (PLAY/REC) — else record_grab would bake the crush into the buffer. Master bus, not per-slot:
// there are only 7 aux buses (SOUND_FX_BUSES) — far fewer than our chop voices — so per-slot crush
// silently drops. SET-AND-HOLD: apply_grit() fires only on a grit toggle or a state change.
static void apply_grit(void) {
    int on = (state == ST_EDIT || state == ST_SONG) && grit > 0;
    crush(GRIT_B[grit], GRIT_R[grit], on ? 1.0f : 0.0f);
}

static void rebind(void) {   // (re)configure every slice's voice + the KEYS voice's region
    int n = nsl();
    for (int i = 0; i < n && i < MAXSLICE; i++) {
        instrument(KIT0 + i, INSTR_SAMPLE, 1, 0, 7, 150);
        instrument_sample(KIT0 + i, cur_buf, ROOT);
        instrument_sample_region(KIT0 + i, bnd[i], bnd[i + 1]);
    }
    if (sel < 0) sel = 0; if (sel >= n) sel = n - 1;
    instrument_sample(KEYS_S, cur_buf, ROOT);             // BIND the KEYS voice to the buffer (not just its region — else it's silent)
    instrument_sample_region(KEYS_S, bnd[sel], bnd[sel + 1]);
    apply_mode();                                         // re-slicing keeps the chosen mode
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
    cur_buf = take_ct % MAXBUF; take_ct++;              // each take → its own buffer (committed chops survive)
    rec_len = record_grab(cur_buf, rec_secs);
    if (rec_len > 0) {
        rec_secs = rec_len / 44100.0f;                 // use the TRIMMED length for timing/display
        sample_peaks(cur_buf, wf_lo, wf_hi, PW);
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
        float f = (mx - PX) / (float)PW;               // clamp between neighbours so markers never cross
        float lo = bnd[grabbed - 1] + 0.005f, hi = bnd[grabbed + 1] - 0.005f;
        if (f < lo) f = lo; if (f > hi) f = hi;
        bnd[grabbed] = f;
        // LIVE: update the two slices touching this marker (+ KEYS if it's the selected one) every frame,
        // so a sounding voice (esp. LOOP/PINGPONG) follows the drag in real time.
        int a = grabbed - 1, b = grabbed;
        if (a >= 0 && a < nsl())     instrument_sample_region(KIT0 + a, bnd[a], bnd[a + 1]);
        if (b >= 0 && b < nsl())     instrument_sample_region(KIT0 + b, bnd[b], bnd[b + 1]);
        if (sel == a || sel == b)    instrument_sample_region(KEYS_S, bnd[sel], bnd[sel + 1]);
        if (!mouse_down(0)) {                          // release: delete a marker dragged hard against a neighbour
            for (int i = 1; i < nb - 1; i++)
                if (bnd[i + 1] - bnd[i] < 0.02f || bnd[i] - bnd[i - 1] < 0.02f) {
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
static int pad_at(int x, int y, int n) {   // which of n pads contains (x,y), or -1
    for (int i = 0; i < n; i++) { int px, py, pw, ph; pad_rect(i, n, &px, &py, &pw, &ph);
        if (x >= px && x <= px + pw && y >= py && y <= py + ph) return i; }
    return -1;
}
static void fire_pad(int i) {
    if (i < 0 || i >= nsl()) return;
    int ms = (int)((bnd[i + 1] - bnd[i]) * rec_secs * 1000.0f); if (ms < 30) ms = 30;
    hit(ROOT, KIT0 + i, 6, ms); sel = i;
}

// ── the DRUM-KIT source play-surface (PLAY/REC when the source is a kit) ──
// A pad per role on the A S D.. letters + multitouch. A hit ALSO logs its moment into
// on_t[] so STOP slices the take exactly where you struck (same honest boundaries the
// keybed gives synth takes) — dk_fire feeds the capture ring like any other voice.
static float drum_flash[DK_N];
static void drum_hit(int role) {
    if (role < 0 || role >= DK_N) return;
    dk_fire(role, 0, 6);
    drum_flash[role] = 1.0f;
    if (state == ST_REC && on_n < MAXSLICE + 1) on_t[on_n++] = now() - rec_t0;
}
static void drum_pads_update(void) {
    static int prev[16], pn = 0;
    int cur[16], cn = 0;
    for (int t = 0; t < touch_count() && cn < 16; t++) {
        int id = touch_id(t); cur[cn++] = id;
        int seen = 0; for (int k = 0; k < pn; k++) if (prev[k] == id) seen = 1;
        if (!seen) drum_hit(pad_at(touch_x(t), touch_y(t), DK_N));   // newly-down finger
    }
    pn = cn; for (int k = 0; k < cn; k++) prev[k] = cur[k];
    for (int i = 0; i < DK_N; i++) { char k = pad_key(i); if (k && keyp(k)) drum_hit(i); }
}
static void drum_pads_draw(void) {
    for (int i = 0; i < DK_N; i++) {
        int px, py, pw, ph; pad_rect(i, DK_N, &px, &py, &pw, &ph);
        float f = drum_flash[i]; if (f > 0) drum_flash[i] = f - 0.12f;
        int base = (i == DK_KICK || i == DK_SNARE) ? CLR_DARK_GREEN : CLR_DARKER_GREY;
        rectfill(px, py, pw, ph, f > 0.05f ? CLR_LIME_GREEN : base);
        rect(px, py, pw, ph, CLR_MEDIUM_GREY);
        char k = pad_key(i), t[2] = { k, 0 };
        print(t, px + pw / 2 - text_width(t) / 2, py + 4, CLR_WHITE);
        font(FONT_SMALL);                                   // the role labels are wide — shrink to fit the pad
        const char *nm = dk_role_name(i);
        print(nm, px + pw / 2 - text_width(nm) / 2, py + ph - 8, f > 0.05f ? CLR_WHITE : CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }
}

// ── SONG: the arrangement (a 16-step grid over a fixed bank of committed chops) ──
static float song_secs(void) { float s = 0; for (int i = 0; i < song_n; i++) s += song[i].secs; return s; }
static void song_bind(int i) {                   // bind pad i's voice to its chop (buffer + region)
    int v = SONG_V0 + i;
    instrument(v, INSTR_SAMPLE, 1, 0, 7, 150);
    instrument_sample(v, song[i].buf, ROOT);
    instrument_sample_region(v, song[i].start, song[i].end);
}
static void commit_chop(void) {                  // add the SELECTED chop of the current take to the bank
    if (song_n >= SONGPADS || rec_len <= 0) return;
    song[song_n] = (Chop){ cur_buf, bnd[sel], bnd[sel + 1], (bnd[sel + 1] - bnd[sel]) * rec_secs };
    song_bind(song_n);
    song_n++;
    song_dirty = 1;
}

// ── persistence (mic-and-sampling.md piece 5): the arrangement survives a reload ──
// Save the referenced take BUFFERS (sample_read) + the chop table + grid + tempo + grit; restore
// them at init with sample_load — no re-recording. Auto-saved on leaving the SONG + on commit.
#define SMP_SCRATCH (8 * 44100 + 64)                   // max samples in a take buffer (≥ the 8s ring)
#define SMP_MAGIC   0x53414d33                         // 'SAM3' — bump if the layout changes
#define SMP_MAXBLOB (5 * 4 + SONGPADS * SONGSTEPS + SONGPADS * 16 + MAXBUF * (8 + SMP_SCRATCH * 4))
static float smp_scratch[SMP_SCRATCH];

static void song_save(void) {
    if (song_n <= 0) return;
    int blen[MAXBUF]; for (int b = 0; b < MAXBUF; b++) blen[b] = -1;    // -1 = not referenced
    int nbuf = 0; long pcm = 0;
    for (int i = 0; i < song_n; i++) { int b = song[i].buf;
        if (b >= 0 && b < MAXBUF && blen[b] < 0) {
            int n = sample_read(b, smp_scratch, SMP_SCRATCH);
            if (n > 0) { blen[b] = n; nbuf++; pcm += n; } else blen[b] = 0;
        } }
    long size = 5 * 4 + SONGPADS * SONGSTEPS + (long)song_n * 16 + (long)nbuf * 8 + pcm * 4;
    unsigned char *blob = (unsigned char *)malloc((size_t)size);
    if (!blob) return;
    unsigned char *p = blob;
    #define PUTI(v) do { int _v = (v); memcpy(p, &_v, 4); p += 4; } while (0)
    #define PUTF(v) do { float _v = (v); memcpy(p, &_v, 4); p += 4; } while (0)
    PUTI(SMP_MAGIC); PUTI(song_n); PUTI(song_bpm); PUTI(grit); PUTI(nbuf);
    memcpy(p, song_seq, SONGPADS * SONGSTEPS); p += SONGPADS * SONGSTEPS;
    for (int i = 0; i < song_n; i++) { PUTI(song[i].buf); PUTF(song[i].start); PUTF(song[i].end); PUTF(song[i].secs); }
    for (int b = 0; b < MAXBUF; b++) if (blen[b] > 0) { PUTI(b); PUTI(blen[b]); sample_read(b, (float *)p, blen[b]); p += (long)blen[b] * 4; }
    #undef PUTI
    #undef PUTF
    save_bytes(blob, (int)size);
    free(blob);
}

static void song_load(void) {
    unsigned char *blob = (unsigned char *)malloc(SMP_MAXBLOB);
    if (!blob) return;
    int total = load_bytes(blob, SMP_MAXBLOB);
    unsigned char *p = blob, *e = blob + total;
    #define GETI(dst) do { if (p + 4 > e) goto done; memcpy(&dst, p, 4); p += 4; } while (0)
    #define GETF(dst) do { if (p + 4 > e) goto done; memcpy(&dst, p, 4); p += 4; } while (0)
    if (total < 20) goto done;
    int magic, sn, nbuf; GETI(magic); if (magic != SMP_MAGIC) goto done;
    GETI(sn); GETI(song_bpm); GETI(grit); GETI(nbuf);
    if (sn < 0) sn = 0; if (sn > SONGPADS) sn = SONGPADS;
    if (song_bpm < 60) song_bpm = 60; if (song_bpm > 240) song_bpm = 240;
    if (grit < 0 || grit > 3) grit = 0;
    if (nbuf < 0) nbuf = 0; if (nbuf > MAXBUF) nbuf = MAXBUF;
    if (p + SONGPADS * SONGSTEPS > e) goto done;
    memcpy(song_seq, p, SONGPADS * SONGSTEPS); p += SONGPADS * SONGSTEPS;
    song_n = sn;
    for (int i = 0; i < song_n; i++) { GETI(song[i].buf); GETF(song[i].start); GETF(song[i].end); GETF(song[i].secs); }
    int maxbuf = 0;
    for (int j = 0; j < nbuf; j++) {
        int slot, len; GETI(slot); GETI(len);
        if (len < 0) len = 0; if (len > SMP_SCRATCH) len = SMP_SCRATCH;
        if (p + (long)len * 4 > e) goto done;
        if (slot >= 0 && slot < MAXBUF && len > 0) { sample_load(slot, (float *)p, len); if (slot + 1 > maxbuf) maxbuf = slot + 1; }
        p += (long)len * 4;
    }
    for (int i = 0; i < song_n; i++) if (song[i].buf >= 0 && song[i].buf < MAXBUF) song_bind(i);
    take_ct = maxbuf; cur_buf = maxbuf % MAXBUF;   // new takes land past the restored buffers
done:
    #undef GETI
    #undef GETF
    free(blob);
}
static int song_rowh(void) { int n = song_n < 1 ? 1 : song_n, h = SG_H / n; if (h > 20) h = 20; return h < 7 ? 7 : h; }
static int song_cell_at(int x, int y, int *pp, int *ps) {   // (x,y) → pad p, step s (1 = hit)
    int rh = song_rowh(), x0 = SG_X + SG_LBL, cw = (SG_W - SG_LBL) / SONGSTEPS;
    if (x < x0 || x >= x0 + cw * SONGSTEPS || y < SG_Y) return 0;
    int p = (y - SG_Y) / rh, s = (x - x0) / cw;
    if (p < 0 || p >= song_n || s < 0 || s >= SONGSTEPS) return 0;
    *pp = p; *ps = s; return 1;
}
static void song_update(void) {
    bpm(song_bpm);
    if (song_play && song_n > 0) {                       // advance the playhead off the synth clock
        int n = (int)(beat() * 4 + beat_pos() * 4.0f);
        if (n != song_last16) {
            song_last16 = n; song_step = n % SONGSTEPS;
            for (int p = 0; p < song_n; p++) if (song_seq[p][song_step]) {
                int ms = (int)(song[p].secs * 1000.0f); if (ms < 30) ms = 30;
                hit(ROOT, SONG_V0 + p, 6, ms); song_flash[p] = 1.0f;
            }
        }
    }
    static int prev[16], pn = 0;                          // tap a cell to toggle (fresh touches only)
    int cur[16], cn = 0;
    for (int t = 0; t < touch_count() && cn < 16; t++) {
        int id = touch_id(t); cur[cn++] = id;
        int seen = 0; for (int k = 0; k < pn; k++) if (prev[k] == id) seen = 1;
        int p, s; if (!seen && song_cell_at(touch_x(t), touch_y(t), &p, &s)) { song_seq[p][s] ^= 1; song_dirty = 1; }
    }
    pn = cn; for (int k = 0; k < cn; k++) prev[k] = cur[k];
}
static void song_draw(void) {
    cls(CLR_BLACK);
    char b[64];
    print("SONG", 4, 2, CLR_WHITE);
    snprintf(b, sizeof b, "%.1fs  %d/%d pads  %dbpm  %s", song_secs(), song_n, SONGPADS, song_bpm, GRIT_NM[grit]);
    print(b, SCREEN_W - text_width(b) - 4, 2, CLR_LIGHT_GREY);   // seconds budget (shown, not capped) + grit

    int rh = song_rowh(), x0 = SG_X + SG_LBL, cw = (SG_W - SG_LBL) / SONGSTEPS;
    if (song_n == 0) print("add chops in EDIT (+ADD), then arrange them here", 4, SG_Y + 20, CLR_MEDIUM_GREY);
    for (int p = 0; p < song_n; p++) {
        int ry = SG_Y + p * rh;
        float f = song_flash[p]; if (f > 0) song_flash[p] = f - 0.12f;
        char t[4]; snprintf(t, sizeof t, "%d", p + 1);
        print(t, SG_X, ry + rh / 2 - 3, f > 0.05f ? CLR_WHITE : CLR_LIGHT_GREY);
        for (int s = 0; s < SONGSTEPS; s++) {
            int cx = x0 + s * cw, on = song_seq[p][s];
            int col = on ? (f > 0.05f ? CLR_WHITE : CLR_LIME_GREEN)
                         : (s == song_step && song_play ? CLR_DARK_GREEN
                                                        : ((s / 4) % 2 ? CLR_DARKER_GREY : CLR_DARK_GREY));
            rectfill(cx + 1, ry + 1, cw - 2, rh - 2, col);
        }
    }

    ui_begin();
    int y = SCREEN_H - 20;
    if (ui_button(4,   y, 50, 18, song_play ? "STOP" : "PLAY")) song_play ^= 1;
    if (ui_button(58,  y, 22, 18, "-") && song_bpm > 60)  { song_bpm -= 5; song_dirty = 1; }
    if (ui_button(82,  y, 22, 18, "+") && song_bpm < 240) { song_bpm += 5; song_dirty = 1; }
    if (ui_button(108, y, 54, 18, "+ TAKE")) { state = ST_PLAY; apply_src(); point_keybed(SYN); }
    if (ui_button(166, y, 50, 18, GRIT_NM[grit])) { grit = (grit + 1) % 4; apply_grit(); song_dirty = 1; }   // whole-song lo-fi
    if (rec_len > 0 && ui_button(SCREEN_W - 96, y, 44, 18, "EDIT")) state = ST_EDIT;
    if (ui_button(SCREEN_W - 48, y, 44, 18, "CLEAR")) {
        for (int p = 0; p < SONGPADS; p++) for (int s = 0; s < SONGSTEPS; s++) song_seq[p][s] = 0;
        song_dirty = 1;
    }
    ui_end();
}

void update(void) {
    if (song_dirty) { song_dirty = 0; song_save(); }         // persist the arrangement after any change
    static int prev_state = -1;                  // grit follows state (crush on in EDIT/SONG, off in PLAY/REC)
    if (state != prev_state) { prev_state = state; apply_grit(); }
#ifdef DE_TRACE
    watch("state", "%d", state); watch("song_n", "%d", song_n); watch("grit", "%d", grit);
#endif
    if (state == ST_SONG) { song_update(); return; }
    if (keyp('[') || keyp(']')) { eng = (eng + (keyp(']') ? 1 : NALL - 1)) % NALL; if (state != ST_EDIT) apply_src(); }

    if (state == ST_PLAY) {
        if (IS_DRUMS(eng)) drum_pads_update(); else keybed_update();
        if (keyp('R')) { state = ST_REC; rec_t0 = now(); on_n = 0; }
    } else if (state == ST_REC) {
        if (IS_DRUMS(eng)) drum_pads_update(); else keybed_update();
        if (keyp('R') || now() - rec_t0 >= MAXREC) stop_take();
    } else { // ST_EDIT
        if (keyp('R')) { state = ST_PLAY; apply_src(); point_keybed(SYN); return; }
        if (keyp('M')) { mode ^= 1; if (mode == MODE_KEYS) point_keybed(KEYS_S); }   // KEYS<->KIT
        if (keyp('V')) { smode = (smode + 1) % 4; apply_mode(); }                    // cycle playback mode
        edit_markers();
        if (mode == MODE_KEYS) {
            keybed_update();
        } else { // MODE_KIT — pads (MULTITOUCH: a fresh finger on a pad fires it; mouse = one finger)
            static int prev[16], pn = 0;
            int cur[16], cn = 0;
            for (int t = 0; t < touch_count() && cn < 16; t++) {
                int id = touch_id(t); cur[cn++] = id;
                int seen = 0; for (int k = 0; k < pn; k++) if (prev[k] == id) seen = 1;
                if (!seen) fire_pad(pad_at(touch_x(t), touch_y(t), nsl()));   // newly-down finger → its pad
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
    if (state == ST_SONG) { song_draw(); return; }
    cls(CLR_BLACK);
    char buf[96];

    print("SAMPLER", 4, 2, CLR_WHITE);
    const char *sn = state == ST_PLAY ? "PLAY" : state == ST_REC ? "REC" : (mode == MODE_KEYS ? "EDIT/KEYS" : "EDIT/KIT");
    print(sn, SCREEN_W - text_width(sn) - 4, 2, state == ST_REC ? CLR_RED : state == ST_EDIT ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    if (state == ST_PLAY)      snprintf(buf, sizeof buf, "%s: press REC and %s", src_name(eng), IS_DRUMS(eng) ? "hit the pads" : "play");
    else if (state == ST_REC)  snprintf(buf, sizeof buf, "recording... %.1fs  (STOP to keep)", now() - rec_t0);
    else                       snprintf(buf, sizeof buf, "%d chops · %s · %d song · %s", nsl(), SMODE_NM[smode],
                                        song_n, GRIT_NM[grit]);
    print(buf, 4, 13, state == ST_REC ? CLR_RED : CLR_LIGHT_GREY);

    draw_panel();

    ui_begin();
    int y = PY + PH + 5;
    if (state == ST_EDIT) {   // chop editing + commit to the SONG (tight 7-button row)
        if (ui_button(4,   y, 48, 18, "RE-REC")) { state = ST_PLAY; apply_src(); point_keybed(SYN); }
        if (ui_button(54,  y, 34, 18, mode == MODE_KEYS ? "KEYS" : "KIT")) {
            mode ^= 1; if (mode == MODE_KEYS) point_keybed(KEYS_S);
        }
        if (ui_button(90,  y, 34, 18, "AUTO"))  { build_slices(); rebind(); }
        if (ui_button(126, y, 40, 18, "SPLIT") && nb <= MAXSLICE) {   // halve the selected slice
            float m = (bnd[sel] + bnd[sel + 1]) * 0.5f;
            for (int j = nb; j > sel + 1; j--) bnd[j] = bnd[j - 1];
            bnd[sel + 1] = m; nb++; rebind();
        }
        if (ui_button(168, y, 40, 18, SMODE_SH[smode])) { smode = (smode + 1) % 4; apply_mode(); }   // playback mode
        if (ui_button(210, y, 40, 18, "+ADD")) commit_chop();          // banks the SELECTED chop
        if (ui_button(252, y, 64, 18, "ARRANGE")) state = ST_SONG;
    } else {
        if (ui_button(4, y, 90, 18, state == ST_REC ? "STOP" : "REC")) {
            if (state == ST_PLAY) { state = ST_REC; rec_t0 = now(); on_n = 0; } else stop_take();
        }
        if (state == ST_PLAY && song_n > 0 && ui_button(100, y, 62, 18, "SONG")) state = ST_SONG;   // jump to a (restored) song
        if (ui_button(SCREEN_W - 66, y, 62, 18, src_name(eng))) { eng = (eng + 1) % NALL; apply_src(); }
    }
    ui_end();

    // bottom play surface: chop pads (EDIT/KIT), the drum kit's own pads (PLAY/REC on a
    // drum source), or the keybed (everything else).
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
    } else if (state != ST_EDIT && IS_DRUMS(eng)) {
        drum_pads_draw();
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
    song_load();                   // restore a saved arrangement (chops + grid + tempo + grit)
}
