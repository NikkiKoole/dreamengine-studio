/* de:meta
{
  "slug": "breakchop",
  "title": "breakchop",
  "status": "active",
  "created": "2026-07-14",
  "kind": ["instrument", "probe"],
  "genre": null,
  "teaches": ["gesture-loop", "audio-input"],
  "lineage": "The MPC/SP-404 break-chopper: load a drum LOOP and slice it across a pad grid to re-play and rearrange. Sibling of the `sampler` cart (shares INSTR_SAMPLE + chop playback + master crush), but the source is an EXTERNAL loop loaded at runtime (data-tools/breaks/breaks.js → sample_load), not the console's own output — and the slicing is TEMPO-LOCKED (even divisions), not transient/note-on. The whole point is 'throw in your own loop and it's cut correctly'. Falls back to a console-synthesised break so it's self-contained.",
  "description": {
    "summary": "Chop a drum loop across a pad grid, tempo-locked — the MPC move. Hit REC to BEATBOX a loop with your mouth: it records a few seconds of mic and auto-chops your take at the hits (energy onsets) straight onto the pads. Or it loads a loop (the amen, via data-tools/breaks) and slices it into even divisions (8/16/32); each slice lands on a pad you can play by hand or auto-run in order at the loop's own tempo (which reconstructs the break — the proof the cut is right). It plays MONO — a new hit silences the ringing chop (the MPC mute-group feel), so stutters stay clean. Give any chop its own REVERSE, SPEED (.25–2x, varispeed so pitch couples), or TONE (±12 semis, GRANULAR so it repitches WITHOUT changing the slice length — 'tuned, not sped up', the decoupled twin of SPEED) — tap a pad to select it, set its property, then perform: you design the kit chop-by-chop and live-play it. Add GRIT for the SP-1200 crunch, then ride the punch-in FX live: the DJ FILTER (CUT + RES knobs, LP/HP), an ECHO throw (dub delay whose tail rings out), and STUTTER (roll the chops). With no loop file it synthesises a break so there's always something to chop.",
    "detail": "Bring-your-own-loop auto-chopper. At startup it loads a mono PCM loop via de_data_path() (--data / $DE_DATA) or the default data-tools/breaks/cache/amen.f32; if none is found it synthesises a short kick/snare/hat break so the cart is never empty. The loop is sliced into N EVEN divisions (DIV cycles 8/16/32) — tempo-locked by construction: a clean N-beat loop cuts exactly on the grid, and the tempo is DERIVED from the loop length (60·N/seconds), no metadata needed. The break plays MONOPHONICALLY through one SELF-CHOKED voice: firing any pad (or the next slice in PLAY) instantly silences the chop that's still ringing — the MPC mute-group feel — so stutters/rolls stay clean and the reconstruction never smears. Each slice sets that voice's [i/N, (i+1)/N] region and lands on a pad. TAP a pad (touch/mouse) or press its key to fire that slice — the last one tapped is SELECTED. Each pad carries its OWN reverse + speed + tone: hit REV, SPD or TONE to set the selected chop's property, tap again to hear it. SPEED = varispeed .25–2x, pitch AND time couple (.5x = octave down + half tempo). TONE = a granular pitch shift ±12 semitones that keeps the slice's ORIGINAL duration (pitch WITHOUT speed — the time-stretch move; grainy by nature, like an SP-404 pitch mode). SPEED and TONE stack, and together they teach the exact repitch-vs-timestretch distinction. So you DESIGN the kit chop-by-chop (reverse this one, drop that one an octave) then PERFORM — auto-play and hand-play both use the assigned properties. This is the set-then-play split: per-pad properties are assigned (you can't tune a pad while playing it), while the GLOBAL effects are ridden live — the SP-404 punch-in half: GRIT (CLEAN/12BIT/8BIT/CRUSH), the DJ FILTER (CUT + RES knobs + LP/HP, drag it down for the breakdown thump / open for the drop), an ECHO throw (a tempo-synced dotted-8th dub delay — toggle it on for a moment and the tail rings out after), and STUTTER (pads fire as a looped roll — a bounce-roll if the pad is reversed). Best ridden while auto-play runs and your hands are free. PLAY steps through the slices in order at the derived tempo, looping. GRIT crushes the whole output CLEAN/12BIT/8BIT/CRUSH (SP-1200). The waveform shows the slice markers + a live playhead.",
    "controls": "SPACE (or PLAY) — auto-run the slices in order at tempo (loops). Pads: tap/click a pad or press its key (1 2 3 4 / Q W E R / A S D F / Z X C V) to fire that slice — the last tapped is SELECTED. REV / SPD / TONE — set the SELECTED pad's reverse / speed (.25/.5/1/1.5/2x, varispeed — pitch+time) / tone (-12/-7/-5/+5/+7/+12 semis, granular — pitch only, length held), then tap it to hear it: design each chop, then perform. DIV — cycle 8/16/32 slices. GRIT — CLEAN/12BIT/8BIT/CRUSH lo-fi (global). FILTER — drag the CUT + RES knobs, cycle OFF/LP/HP: the DJ punch-in sweep. ECHO — throw a dub delay (dotted-8th; tail rings after). STUT — stutter: pads loop their slice as a roll. All global, ridden live. Drop your own loop with --data <loop.f32> (mono float32) or $DE_DATA; make one with data-tools/breaks/breaks.js. REC — beatbox into the mic for ~4s (allow the permission prompt); it captures your take and auto-chops it at the hits onto the pads (mic recording; capture-then-freeze)."
  },
  "todo": [
    "RELEASE GATE: the amen fixture is a copyrighted dev placeholder — before publishing, ship no bundled audio (loops are user-supplied) or swap to a CC0 loop. See data-tools/breaks/README.md.",
    "onset-chop now exists for MIC takes (energy-onset boundaries → uneven slices); extend it to loaded loop FILES too (they're still pure even division).",
    "punch-in FX done (filter/echo/stutter); a true hold-to-roll (finger-down length) would need note_on/off held-pad tracking.",
    "runtime user import on device (file picker → sample_load) — the eventual product surface.",
    "free slice→pad assignment + reorder; persist the designed kit (per-pad rev/speed/tone) via save_bytes, like the sampler.",
    "TONE (granular repitch) softens the FIRST hit after silence ~80ms (granular reads recent past → cold buffer at chop start; consecutive rolls/auto-play hits stay punchy). A pre-rendered per-pad resample would be transient-tight, but needs an offline free-transpose primitive (sample_autotune only snaps to a scale). Revisit if the softening bothers on real breaks."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// ── BREAKCHOP ────────────────────────────────────────────────────────────────
// Load a drum LOOP, slice it into an even tempo grid across a pad grid, play/perform
// the slices. External loop (data-tools/breaks) or a synthesised fallback. See de:meta.

#define LOOP_SLOT 0                 // the PCM sample buffer
#define VOICE     8                 // ONE mono playback voice — SELF-CHOKED so a new hit silences
                                    //   the ringing chop (MPC mute-group feel; safe: choke runs before
                                    //   the fresh voice is allocated). The break plays back monophonically.
#define ROOT      60                // C4 = original speed
#define MAXSLICE  32
#define DEFAULT_LOOP "../data-tools/breaks/cache/amen.f32"   // cwd is build/ → the gitignored cache

static int   loaded    = 0;
static int   is_synth  = 0;         // true = the fallback break (no file found)
static int   loop_len  = 0;         // samples
static float loop_secs = 0.0f;
static int   ndiv      = 16;        // slice count
static int   sel       = 0;
static int   playing   = 0;
static int   last_beat = -1, cur_step = 0;
static int   grit      = 0;
static float pad_flash[MAXSLICE];
static float wf_lo[240], wf_hi[240];
static float slice_bound[MAXSLICE + 1];   // slice boundaries as fractions 0..1 (ndiv+1 entries) —
                                          //   even divisions OR onset positions from a beatboxed take
static int   rec_state = 0;               // 0 idle · 1 arming (waiting for the mic) · 2 recording
static int   is_mic    = 0;               // true = the loop was beatboxed in through the mic
#define REC_SECONDS 4.0f

static const int   DIVS[]   = { 8, 16, 32 };
static const char *GRIT_NM[] = { "CLEAN", "12BIT", "8BIT", "CRUSH" };
static const float GRIT_B[]  = { 16, 12, 8, 4 };
static const float GRIT_R[]  = { 1, 2, 4, 8 };
static const char  PADKEYS[] = "1234QWERASDFZXCV";   // 16 keys (keyp wants UPPERCASE letters); >16 = click/touch

// per-pad PROPERTIES (set on the selected pad, then perform — the MPC way). speed is varispeed:
// pitch couples (no time-stretch), so .5x = an octave down. SEMI = round(12·log2(speed)).
static const float SPEEDS[]   = { 0.25f, 0.5f, 1.0f, 1.5f, 2.0f };
static const int   SEMI[]     = { -24, -12, 0, 7, 12 };
static const char *SPEED_NM[] = { ".25x", ".5x", "1x", "1.5x", "2x" };
#define NSPEED 5
#define SPD1X  2                          // index of 1.0x (the default)
static char pad_rev[MAXSLICE];            // per-pad reverse
static char pad_speed[MAXSLICE];          // per-pad speed index into SPEEDS (init to SPD1X)

// per-pad TONE — the DECOUPLED twin of SPEED. SPEED is varispeed (pitch+time couple: .5x = octave
// down AND half tempo). TONE repitches the slice via a granular cloud while the read pointer keeps
// the slice's ORIGINAL duration — pitch WITHOUT speed (the time-stretch move: "tuned, not sped up").
// It's grainy by nature (like an SP-404 pitch mode) — that's the sound. Stacks on top of SPEED.
static const int TONES[]   = { -12, -7, -5, 0, 5, 7, 12 };   // semitones
#define NTONE 7
#define TONE0 3                             // index of 0 (no transpose = clean, granular bypassed)
static char pad_tone[MAXSLICE];             // per-pad tone index into TONES (init to TONE0)

// global DJ FILTER — the one ride-live punch-in FX (filter() is cheap to sweep every frame).
static float f_cut = 1.0f;                // knob 0..1 → 20Hz..18kHz (1 = wide open)
static float f_res = 0.0f;                // knob 0..1 → resonance
static int   f_mode = 0;                  // 0=OFF 1=LP 2=HP
// per-VOICE character filters (creamier than the master SVF filter()): the Moog ladder for the LP
// sweep (loses bass + self-oscillates as RES climbs — the juicy DJ-filter squelch), Steiner HP for HP.
static const int   FMODE[]    = { FILTER_OFF, FILTER_LADDER, FILTER_STEINER_HP };
static const char *FMODE_NM[] = { "OFF", "LP", "HP" };

static int echo_on = 0;                   // echo THROW: VOICE feeds the shared echo bus (tail rings after)
static int stut_on = 0;                   // STUTTER: pads fire as a looped roll
#define STUT_REPEATS 4                     // how many times a stuttered slice loops

#define PX 6
#define PY 24
#define PW (SCREEN_W - 12)
#define PH 46
#define GY 124                      // pad grid top (leaves a filter-knob row under the buttons)

static float derived_bpm(void) { return loop_secs > 0.0f ? 60.0f * ndiv / loop_secs : 120.0f; }

static void rebuild_even(void) {              // fill slice_bound with even divisions (the default grid)
    if (ndiv < 1) ndiv = 1; if (ndiv > MAXSLICE) ndiv = MAXSLICE;
    for (int i = 0; i <= ndiv; i++) slice_bound[i] = (float)i / ndiv;
    is_mic = 0;
}
static int slice_of(float frac) {             // which slice a 0..1 position falls in (bounds may be uneven)
    int i = 0; while (i < ndiv - 1 && frac >= slice_bound[i + 1]) i++;
    return i;
}

static void setup_voice(void) {     // one mono INSTR_SAMPLE voice, self-choked (region set per hit)
    instrument(VOICE, INSTR_SAMPLE, 3, 0, 7, 150);   // 3ms attack = a declick ramp (a chop starts mid-waveform → an instant onset clicks); still punchy
    instrument_sample(VOICE, LOOP_SLOT, ROOT);
    instrument_choke(VOICE, VOICE);                       // a new hit cuts the ringing chop → mono
    instrument_grains(VOICE, 30, 90, 1.0f, 0.0f, 0.0f, 0.0f);   // allocate the granular tank up front (mix 0 = bypass);
                                                                //   fire_slice reuses the SAME grain/density so it only flips
                                                                //   mix+pitch (no tank re-alloc). NB granular reads recent past,
                                                                //   so the FIRST tone hit after silence softens ~80ms; rolls /
                                                                //   auto-play keep the buffer primed and stay punchy.
    if (loaded) sample_peaks(LOOP_SLOT, wf_lo, wf_hi, PW < 240 ? PW : 240);
}

static void apply_grit(void) { crush(GRIT_B[grit], GRIT_R[grit], grit ? 1.0f : 0.0f); }
static void apply_filter(void) {                          // per-VOICE ladder/steiner (change-guarded, like acid303)
    static int l_mode = -2, l_hz = -1, l_res = -1;        // instrument_filter is set-and-hold — only re-apply on change
    int  mode = FMODE[f_mode];
    int  hz   = (int)(20.0f * powf(900.0f, f_cut));       // 20 Hz .. ~18 kHz, log sweep
    int  res  = (int)(f_res * 15.0f + 0.5f);              // knob 0..1 → 0..15 Q (the ladder's whistly peak)
    if (mode == l_mode && hz == l_hz && res == l_res) return;
    l_mode = mode; l_hz = hz; l_res = res;
    instrument_filter(VOICE, mode, hz, mode == FILTER_OFF ? 0 : res);   // filters the one chop voice = the whole sound
}

static void fire_slice(int i, int select) {
    if (i < 0 || i >= ndiv) return;
    float sp = SPEEDS[(int)pad_speed[i]];
    float frac = slice_bound[i + 1] - slice_bound[i];     // this slice's share of the buffer (uneven OK)
    float slice_ms = loop_secs * frac / sp * 1000.0f;     // slower pad → longer slice
    int mode = pad_rev[i] ? SAMPLE_REVERSE : SAMPLE_NORMAL;
    int ms = (int)slice_ms;
    if (stut_on) {                                        // STUTTER: loop the slice as a roll (bounce if reversed)
        mode = pad_rev[i] ? SAMPLE_PINGPONG : SAMPLE_LOOP;
        ms = (int)(slice_ms * STUT_REPEATS);
    }
    if (ms < 30) ms = 30;
    instrument_sample_region(VOICE, slice_bound[i], slice_bound[i + 1]);        // carve this slice
    instrument_sample_mode(VOICE, mode);                 // reverse / loop-roll
    // TONE: granular pitch shift (pitch WITHOUT changing the slice's duration — the twin of SPEED's
    // varispeed). A dense, fully-wet live-edge cloud = a real-time pitch shifter; tone 0 → mix 0 =
    // clean bypass. Configured per hit (fire is event-driven, not per-frame, so this is allowed).
    int tone = TONES[(int)pad_tone[i]];
    instrument_grains(VOICE, 30, 90, 1.0f, 0.0f, 0.0f, tone != 0 ? 1.0f : 0.0f);   // same cloud as setup — only mix flips
    if (tone != 0) instrument_grains_pitch(VOICE, (float)tone, 0.0f, 0);
    hit(ROOT + SEMI[(int)pad_speed[i]], VOICE, 6, ms);   // per-pad speed = transpose; self-choke cuts the ringing chop
    pad_flash[i] = 1.0f;
    if (select) sel = i;
}

// synthesise a short kick/snare/hat break so the cart is self-contained when no loop file
// exists (console-generated → on-doctrine). Deterministic: a fixed LCG for the noise.
static void synth_fallback(void) {
    int sr = 44100; float secs = 2.0f; int n = (int)(sr * secs);
    float *buf = (float *)calloc((size_t)n, sizeof(float));
    if (!buf) return;
    unsigned int rng = 0x1234567u;
    int beats = 8;
    for (int b = 0; b < beats; b++) {
        int at = (int)((float)b / beats * n);
        if (b % 2 == 0)                             // kick on 1 & 3 (of each 4) — low sine thump
            for (int i = 0; at + i < n && i < sr * 0.16f; i++) {
                float t = (float)i / sr; buf[at + i] += sinf(2.0f * 3.14159f * 62.0f * t) * expf(-t * 18.0f) * 0.9f;
            }
        if (b % 4 == 2)                             // snare on 2 & 4 — noise burst
            for (int i = 0; at + i < n && i < sr * 0.13f; i++) {
                rng = rng * 1664525u + 1013904223u; float nz = (float)(rng >> 9) / 8388608.0f - 1.0f;
                float t = (float)i / sr; buf[at + i] += nz * expf(-t * 28.0f) * 0.6f;
            }
        for (int i = 0; at + i < n && i < sr * 0.03f; i++) {   // hat every beat — short bright noise
            rng = rng * 1664525u + 1013904223u; float nz = (float)(rng >> 9) / 8388608.0f - 1.0f;
            float t = (float)i / sr; buf[at + i] += nz * expf(-t * 120.0f) * 0.3f;
        }
    }
    float pk = 0.0f; for (int i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > pk) pk = a; }
    if (pk > 0.001f) { float g = 0.95f / pk; for (int i = 0; i < n; i++) buf[i] *= g; }
    sample_load(LOOP_SLOT, buf, n);
    free(buf);
    loop_len = n; loop_secs = secs; loaded = 1; is_synth = 1; ndiv = 8; rebuild_even();
}

static void load_loop(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { synth_fallback(); setup_voice(); return; }
    fseek(f, 0, SEEK_END); long bytes = ftell(f); fseek(f, 0, SEEK_SET);
    long n = bytes / 4;
    if (n < 64) { fclose(f); synth_fallback(); setup_voice(); return; }
    float *buf = (float *)malloc((size_t)n * sizeof(float));
    if (!buf) { fclose(f); synth_fallback(); setup_voice(); return; }
    size_t got = fread(buf, sizeof(float), (size_t)n, f);
    fclose(f);
    if ((long)got < 64) { free(buf); synth_fallback(); setup_voice(); return; }
    sample_load(LOOP_SLOT, buf, (int)got);
    free(buf);
    loop_len = (int)got; loop_secs = loop_len / 44100.0f; loaded = 1; is_synth = 0;
    ndiv = 16; rebuild_even();
    setup_voice();
}

// AUTO-CHOP a beatboxed take: place slice boundaries at the ENERGY ONSETS (the mouth-drum hits),
// not on an even grid. Simple, robust: an RMS envelope per 10ms hop, then a boundary wherever the
// energy jumps up past a floor with a minimum gap between hits (so one hit ≠ several slices).
static void detect_onsets(const float *buf, int n) {
    int hop = 441;                                  // 10 ms @ 44.1k
    int nh = n / hop; if (nh > 4096) nh = 4096;
    if (nh < 4) { ndiv = 8; rebuild_even(); return; }
    static float env[4096];
    float emax = 1e-9f;
    for (int k = 0; k < nh; k++) {
        double s = 0; const float *p = buf + (size_t)k * hop;
        for (int j = 0; j < hop; j++) s += (double)p[j] * p[j];
        env[k] = (float)sqrt(s / hop);
        if (env[k] > emax) emax = env[k];
    }
    float floor_e = emax * 0.16f;                   // ignore hits quieter than this
    float rise_e  = emax * 0.06f;                   // required energy jump to count as an onset
    int   mingap  = 6;                              // ≥60 ms between onsets (one hit → one slice)
    int   cnt = 0, last = -mingap;
    slice_bound[cnt++] = 0.0f;                      // the take always starts on a boundary
    for (int k = 1; k < nh && cnt < MAXSLICE; k++) {
        if (env[k] > floor_e && env[k] - env[k - 1] > rise_e && (k - last) >= mingap) {
            float frac = (float)((size_t)k * hop) / (float)n;
            if (frac > 0.02f && frac < 0.98f) { slice_bound[cnt++] = frac; last = k; }
        }
    }
    slice_bound[cnt] = 1.0f;
    ndiv = cnt;                                     // cnt boundaries before the trailing 1.0 ⇒ cnt slices
    if (ndiv < 2) { ndiv = 8; rebuild_even(); }     // no clear hits → fall back to an even grid
    else is_mic = 1;
}

// finish a mic take: grab the captured PCM, normalize, load it as the loop, and auto-chop it.
static void finish_rec(void) {
    int max = 44100 * 8;
    float *buf = (float *)malloc((size_t)max * sizeof(float));
    if (!buf) { rec_state = 0; return; }
    int n = mic_record_read(buf, max);
    if (n < 2048) { free(buf); rec_state = 0; return; }   // too short — keep whatever was loaded
    float pk = 0.0f; for (int i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > pk) pk = a; }
    if (pk > 0.001f) { float g = 0.95f / pk; for (int i = 0; i < n; i++) buf[i] *= g; }
    sample_load(LOOP_SLOT, buf, n);
    loop_len = n; loop_secs = n / 44100.0f; loaded = 1; is_synth = 0;   // playback is at the engine rate
    detect_onsets(buf, n);
    free(buf);
    if (sel >= ndiv) sel = ndiv - 1;
    for (int i = 0; i < MAXSLICE; i++) { pad_rev[i] = 0; pad_speed[i] = SPD1X; pad_tone[i] = TONE0; }  // fresh kit
    setup_voice();
    mic_stop();                                     // capture-then-freeze: release the mic after the take
}

// pad grid: cols 4 (≤16 slices) or 8 (32); fills GY..bottom
static void pad_rect(int i, int *x, int *y, int *w, int *h) {
    int cols = ndiv <= 16 ? 4 : 8, rows = (ndiv + cols - 1) / cols;
    int gx = 4, gw = SCREEN_W - 8, gh = SCREEN_H - GY - 2;
    int c = i % cols, r = i / cols;
    *w = gw / cols - 3; *h = gh / rows - 3;
    *x = gx + c * (gw / cols); *y = GY + r * (gh / rows);
}
static int pad_at(int px, int py) {
    for (int i = 0; i < ndiv; i++) { int x, y, w, h; pad_rect(i, &x, &y, &w, &h);
        if (px >= x && px <= x + w && py >= y && py <= y + h) return i; }
    return -1;
}

void update(void) {
    bpm((int)(derived_bpm() + 0.5f));

    // mic RECORD (beatbox → auto-chop): arm once the mic is live, then finish when the take completes
    if (rec_state == 1 && mic_active())   { mic_record(REC_SECONDS); rec_state = 2; }
    if (rec_state == 2 && !mic_recording()) { finish_rec(); rec_state = 0; }   // grab + chop, then LEAVE the REC screen
    if (rec_state != 0) return;                               // no pads / auto-play while arming or recording

    if (playing && loaded) {                    // auto-run: one slice per beat, looping → reconstructs the loop
        int b = beat();
        if (b != last_beat) { last_beat = b; cur_step = ((b % ndiv) + ndiv) % ndiv; fire_slice(cur_step, 0); }
    }

    if (keyp(' ')) { playing = !playing; last_beat = -1; }

    // pads — multitouch: a fresh finger on a pad fires its slice
    static int prev[16], pn = 0; int cur[16], cn = 0;
    for (int t = 0; t < touch_count() && cn < 16; t++) {
        int id = touch_id(t); cur[cn++] = id;
        int seen = 0; for (int k = 0; k < pn; k++) if (prev[k] == id) seen = 1;
        if (!seen) { int p = pad_at(touch_x(t), touch_y(t)); if (p >= 0) fire_slice(p, 1); }
    }
    pn = cn; for (int k = 0; k < cn; k++) prev[k] = cur[k];
    for (int i = 0; i < ndiv && i < (int)sizeof(PADKEYS) - 1; i++)
        if (keyp(PADKEYS[i])) fire_slice(i, 1);
}

void draw(void) {
    cls(CLR_BLACK);
    char buf[96];
    print("BREAKCHOP", 4, 2, CLR_WHITE);
    if (!loaded) { print("no loop loaded", 4, 14, CLR_RED); return; }
    char tinfo[8] = "";
    if (pad_tone[sel] != TONE0) snprintf(tinfo, sizeof tinfo, " %+dt", TONES[(int)pad_tone[sel]]);
    snprintf(buf, sizeof buf, "%d slices  %.0f bpm  %s%s   pad%d: %s%s%s", ndiv, derived_bpm(), GRIT_NM[grit],
             is_mic ? " MIC" : is_synth ? " SYNTH" : "", sel + 1, SPEED_NM[(int)pad_speed[sel]], pad_rev[sel] ? " REV" : "", tinfo);
    print(buf, 4, 13, CLR_LIGHT_GREY);

    // waveform + slice markers + playhead
    int cols = PW < 240 ? PW : 240;
    rectfill(PX, PY, PW, PH, CLR_DARKER_GREY);
    int mid = PY + PH / 2, half = PH / 2 - 1;
    for (int x = 0; x < cols; x++) {
        int sx = PX + x * PW / cols;
        int si = slice_of((float)x / cols);          // which slice this column falls in (uneven OK)
        int c = (si == sel) ? CLR_LIME_GREEN : ((si & 1) ? CLR_MEDIUM_GREEN : CLR_BLUE_GREEN);
        line(sx, mid - (int)(wf_hi[x] * half), sx, mid - (int)(wf_lo[x] * half), c);
    }
    for (int i = 1; i < ndiv; i++) {                 // slice boundaries (even OR onset positions)
        int bx = PX + (int)(slice_bound[i] * PW);
        line(bx, PY, bx, PY + PH - 1, CLR_YELLOW);
    }
    { float p = instrument_playhead(VOICE);          // one mono voice → one playhead (0..1 of the WHOLE buffer)
      if (p >= 0.0f) { int hx = PX + (int)(p * PW); line(hx, PY, hx, PY + PH - 1, CLR_WHITE); } }
    rect(PX, PY, PW, PH, CLR_MEDIUM_GREY);

    // mic RECORD overlay — draw over the waveform while arming / capturing a beatbox take
    if (rec_state != 0) {
        rectfill(PX + 1, PY + 1, PW - 2, PH - 2, CLR_DARKER_GREY);
        if (rec_state == 1) {
            print("ARMING MIC... allow the prompt", PX + 8, PY + PH / 2 - 3, CLR_YELLOW);
        } else {
            float p = mic_record_progress();
            print("REC", PX + 6, PY + 4, CLR_RED);
            char c[28]; snprintf(c, sizeof c, "beatbox!  %.1fs", REC_SECONDS * (1.0f - p));
            print(c, PX + PW / 2 - 40, PY + 6, CLR_WHITE);
            int lv = (int)(mic_level() * (PW - 12) * 3.0f); if (lv > PW - 12) lv = PW - 12;
            rectfill(PX + 6, PY + PH / 2 - 4, lv, 7, CLR_GREEN);                 // live input level
            rect(PX + 6, PY + PH - 12, PW - 12, 6, CLR_MEDIUM_GREY);
            rectfill(PX + 6, PY + PH - 12, (int)(p * (PW - 12)), 6, CLR_RED);    // capture progress
        }
    }

    // controls
    ui_begin();
    if (ui_button(4,  PY + PH + 3, 56, 16, playing ? "STOP" : "PLAY")) { playing = !playing; last_beat = -1; }
    if (ui_button(64, PY + PH + 3, 46, 16, "DIV")) {
        int k = 0; for (int j = 0; j < 3; j++) if (DIVS[j] == ndiv) k = j;
        ndiv = DIVS[(k + 1) % 3]; if (sel >= ndiv) sel = ndiv - 1;
    }
    if (ui_button(114, PY + PH + 3, 50, 16, GRIT_NM[grit])) { grit = (grit + 1) % 4; apply_grit(); }
    // REV + SPD act on the SELECTED (last-tapped) pad, and re-fire it so you hear the change
    if (ui_button(168, PY + PH + 3, 40, 16, pad_rev[sel] ? "REV*" : "REV")) { pad_rev[sel] ^= 1; fire_slice(sel, 1); }
    if (ui_button(212, PY + PH + 3, 52, 16, SPEED_NM[(int)pad_speed[sel]])) {
        pad_speed[sel] = (pad_speed[sel] + 1) % NSPEED; fire_slice(sel, 1);
    }
    // TONE — pitch WITHOUT speed (granular). The decoupled twin of SPD; cycles the selected pad + re-fires
    { char tl[8]; if (pad_tone[sel] != TONE0) snprintf(tl, sizeof tl, "%+d", TONES[(int)pad_tone[sel]]); else snprintf(tl, sizeof tl, "TONE");
      if (ui_button(266, PY + PH + 3, 50, 16, tl)) { pad_tone[sel] = (pad_tone[sel] + 1) % NTONE; fire_slice(sel, 1); } }
    // global RIDE-LIVE FX row: DJ filter (CUT/RES knobs + mode), echo throw, stutter
    ui_knob(&f_cut, 20, 106, "CUT");
    ui_knob(&f_res, 62, 106, "RES");
    if (ui_button(84,  98, 36, 16, FMODE_NM[f_mode])) f_mode = (f_mode + 1) % 3;
    if (ui_button(124, 98, 46, 16, echo_on ? "ECHO*" : "ECHO")) {
        echo_on = !echo_on;
        if (echo_on) echo((int)(60000.0f / derived_bpm() * 0.75f), 0.45f, 0.6f);   // dotted-8th dub throw
        instrument_echo(VOICE, echo_on ? 0.4f : 0.0f);                             // send on/off (tail rings after)
    }
    if (ui_button(172, 98, 46, 16, stut_on ? "STUT*" : "STUT")) stut_on = !stut_on;
    if (ui_button(222, 98, 50, 16, rec_state ? "STOP" : "REC")) {   // beatbox → auto-chop
        if      (rec_state == 2) { finish_rec(); rec_state = 0; }   // STOP = use the take NOW (also auto-fires at ~4s)
        else if (rec_state == 1) { rec_state = 0; mic_stop(); }     // still arming (no audio yet) → just cancel
        else                     { playing = 0; rec_state = 1; mic_start(); }
    }
    ui_end();
    apply_filter();                                       // rideable, every frame

    // pad grid
    for (int i = 0; i < ndiv; i++) {
        int x, y, w, h; pad_rect(i, &x, &y, &w, &h);
        float f = pad_flash[i]; if (f > 0) pad_flash[i] = f - 0.12f;
        int base = (i == cur_step && playing) ? CLR_DARK_GREEN : (i == sel ? CLR_DARK_GREEN : CLR_DARKER_GREY);
        rectfill(x, y, w, h, f > 0.05f ? CLR_LIME_GREEN : base);
        rect(x, y, w, h, CLR_MEDIUM_GREY);
        if (i < (int)sizeof(PADKEYS) - 1) {
            char t[2] = { PADKEYS[i], 0 };
            print(t, x + w / 2 - text_width(t) / 2, y + h / 2 - 3, CLR_WHITE);
        }
        if (pad_rev[i] || pad_speed[i] != SPD1X || pad_tone[i] != TONE0) {   // per-pad property tag (small, bottom of pad)
            char tag[16], tt[6] = "";
            if (pad_tone[i] != TONE0) snprintf(tt, sizeof tt, "%+d", TONES[(int)pad_tone[i]]);
            snprintf(tag, sizeof tag, "%s%s%s", pad_rev[i] ? "<" : "",
                     pad_speed[i] != SPD1X ? SPEED_NM[(int)pad_speed[i]] : "", tt);
            font(FONT_SMALL);
            print(tag, x + w / 2 - text_width(tag) / 2, y + h - 8, CLR_YELLOW);
            font(FONT_NORMAL);
        }
    }
}

void init(void) {
    for (int i = 0; i < MAXSLICE; i++) { pad_speed[i] = SPD1X; pad_tone[i] = TONE0; }   // default: 1x, no tone
    const char *p = de_data_path();
    load_loop(p ? p : DEFAULT_LOOP);
    apply_grit();
}
