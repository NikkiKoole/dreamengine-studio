/* de:meta
{
  "slug": "patchbench",
  "title": "patchbench",
  "status": "active",
  "created": "2026-09-12",
  "kind": ["instrument", "probe"],
  "genre": null,
  "teaches": ["adsr-envelope"],
  "lineage": "The landing pad for tools/patch-match. The CLI hands back a SET of candidate patches and the repo had no object that held a set, so a match used to be pasted at the cursor into whatever cart happened to be open (docs/design/patch-matching-cart.md §7). This is where a set lands instead: the candidates on pads, the target on its own key, and one A/B button between them.",
  "description": {
    "summary": "The bench where a matched patch gets judged. tools/patch-match listens to a sample and hands back eight dreamengine patches that try to sound like it; this is where you hear whether any of them actually do. The candidates sit on pads, the original sits on its own key, and A/B plays them back to back so the comparison is a keypress instead of a memory. Pick one, ride its three macros while it sounds, and COPY prints the patch as C you can paste. With no run loaded it holds three built-in patches, so there is always something to play.",
    "detail": "A cart with one job: hold a SET of instrument patches and let an ear choose between them. The eight candidates live in a `// de:patch-slots` region that the editor rewrites after a search (drop a WAV on the editor window), so the pasted code has exactly ONE legal home and can never land in an unrelated cart. Each pad shows its engine and its fx loss, which is the search's own opinion and deliberately not the last word: a spectral distance is not perception, which is the whole reason this bench exists. The TARGET key plays the original sample the search was chasing, loaded from the run directory next to the candidates; when no run is loaded the bench falls back to three built-in patches and says the target is missing rather than going quiet. A/B alternates target and selection at the same pitch and the same length, which is the only comparison that means anything. The three macro knobs (harmonics / timbre / morph) ride the SELECTED candidate live, so a near miss can be walked in by hand, and COPY prints the patch including whatever the knobs are set to now.",
    "controls": "1-8 — select and play that candidate. T — play the target. SPACE — play the selection. B — A/B (target, then the selection, same note, same length). Click a pad to select and play it. HARM / TIMB / MORPH — drag the knobs to ride the selected candidate's three macros. COPY — print the selected patch as pasteable C into the editor's log panel. Up/down arrows move the note the bench plays."
  },
  "todo": [
    "The editor writes the de:patch-slots region (docs/design/patch-matching-cart.md §7) — until that lands, a run is pasted in by hand.",
    "A run browser over build/patch-match/* belongs in the editor panel, not here.",
    "One mutation operator away from option B (#16): perturb the selected PmPatch and refill the pads with its children. That is the whole sprout tree, and this is its shell."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── THE EDITOR'S REGION ─────────────────────────────────────────────────────────────────────────
// Everything between the two markers is REWRITTEN by the editor after a patch-match search. Nothing
// else in this file is touched, which is the point: the result of a search has exactly one legal
// home, so it can never land in the middle of a cart you happened to have open.
//
// The shape is fixed. PB_RUN names the run directory under build/patch-match (the target lives
// there), PB_N is how many pads are live, PB_NAME/PB_LOSS label them, and pb_apply(i) configures
// PB_SLOT to be candidate i. The default below is three hand-written patches rather than a real
// run, so the bench is playable on a fresh clone with no search ever having happened.
// de:patch-slots begin
#define PB_RUN  ""              // "" = no run loaded, so no target to compare against
#define PB_N    3
static const char *PB_NAME[PB_N] = { "PLUCK", "EPIANO", "PIPE" };
static const float PB_LOSS[PB_N] = { -1.0f, -1.0f, -1.0f };   // <0 = no loss (hand-written, not searched)
static void pb_apply(int i, int slot) {
    switch (i) {
    case 0:
        instrument(slot, INSTR_PLUCK, 2, 90, 1, 120);
        instrument_harmonics(slot, 0.35f); instrument_timbre(slot, 0.50f); instrument_morph(slot, 0.20f);
        break;
    case 1:
        instrument(slot, INSTR_EPIANO, 4, 400, 3, 300);
        instrument_harmonics(slot, 0.45f); instrument_timbre(slot, 0.55f); instrument_morph(slot, 0.30f);
        break;
    default:
        instrument(slot, INSTR_PIPE, 60, 300, 5, 400);
        instrument_harmonics(slot, 0.20f); instrument_timbre(slot, 0.80f); instrument_morph(slot, 0.10f);
        break;
    }
}
// de:patch-slots end

#define PB_SLOT  5        // the candidate under test
#define PB_TGT   6        // the original the search was chasing
#define PB_SAMP  0        // sample buffer holding target.wav
#define PB_MAXN  8        // pads the layout draws; PB_N may be fewer

static int   sel = 0;                 // which candidate is selected
static int   note_midi = 60;
static int   have_target = 0;
static int   target_len = 0;          // samples, 0 = nothing loaded
static float tgt_peak[64];            // a cheap waveform for the target strip
static float mac[PB_MAXN][3];         // per-candidate harmonics/timbre/morph, ridden live
static int   mac_ready[PB_MAXN];      // has this candidate's knob state been read off the patch yet
static int   ab_stage = 0;            // 0 = idle, 1 = target playing, 2 = candidate playing
static float ab_t = 0;
static int   applied = -1;            // which candidate PB_SLOT currently holds (-1 = none)
static float bal = 0.30f;             // candidate trim so it sits at the target's level (measured)
static int   bal_ready = 0;
static char  status[64] = "";
static float status_t = 0;

#define NOTE_MS 900

// ── the target ──────────────────────────────────────────────────────────────────────────────────
// pm writes a mono 16-bit WAV. Parsing it here rather than leaning on a header keeps the cart
// self-contained: this file compiles with nothing but studio.h and ui.h, like every other cart.
static int wav_read_mono(const char *path, float **out, int *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) || memcmp(riff + 8, "WAVE", 4)) { fclose(f); return 0; }
    int chan = 1, bits = 16;
    for (;;) {
        char id[4]; unsigned int sz;
        if (fread(id, 1, 4, f) != 4 || fread(&sz, 4, 1, f) != 1) { fclose(f); return 0; }
        if (!memcmp(id, "fmt ", 4)) {
            unsigned char fmt[16];
            if (sz < 16 || fread(fmt, 1, 16, f) != 16) { fclose(f); return 0; }
            chan = fmt[2] | (fmt[3] << 8);
            bits = fmt[14] | (fmt[15] << 8);
            if (sz > 16) fseek(f, (long)sz - 16, SEEK_CUR);
        } else if (!memcmp(id, "data", 4)) {
            if (bits != 16 || chan < 1) { fclose(f); return 0; }
            int frames = (int)(sz / (unsigned)(2 * chan));
            if (frames < 64) { fclose(f); return 0; }
            short *raw = (short *)malloc((size_t)frames * chan * sizeof(short));
            float *buf = (float *)malloc((size_t)frames * sizeof(float));
            if (!raw || !buf) { free(raw); free(buf); fclose(f); return 0; }
            size_t got = fread(raw, sizeof(short), (size_t)frames * chan, f);
            fclose(f);
            frames = (int)(got / (size_t)chan);
            for (int i = 0; i < frames; i++) buf[i] = raw[i * chan] / 32768.0f;   // channel 0, mono or left
            free(raw);
            *out = buf; *n = frames;
            return frames > 0;
        } else {
            fseek(f, (long)sz + (sz & 1), SEEK_CUR);
        }
    }
}

static void load_target(void) {
    if (!PB_RUN[0]) return;
    char path[256];
    // cwd is build/ (studio.c spawns the cart there), and pm writes build/patch-match/<run>/.
    snprintf(path, sizeof path, "patch-match/%s/target.wav", PB_RUN);
    float *buf = NULL; int n = 0;
    if (!wav_read_mono(path, &buf, &n)) return;
    // ⚠ LEVEL-MATCH, or the A/B is worthless. The target plays at whatever level it was recorded
    // at and the candidate plays at hit() volume 5, and measured on the first real run those were
    // 10 dB apart. In a back-to-back comparison the louder one simply wins, whatever it sounds
    // like, which is the oldest trap in listening tests and would have made this bench actively
    // misleading rather than merely rough. pm scored a SPECTRUM, not a loudness.
    //
    // The target is normalised UP to near full scale here and the CANDIDATE is trimmed down to meet
    // it (the BAL knob), because instrument_level only attenuates: 0..1, no boost. Measured on the
    // ambitone run, an untrimmed candidate sits ~10.6 dB above the target, hence BAL's default.
    // BAL is a knob and not a constant on purpose, since candidates differ in level from each other
    // too and no fixed number matches them all: the last 3 dB is a job for your ear.
    {
        float pk = 0;
        for (int i = 0; i < n; i++) { float v = buf[i] < 0 ? -buf[i] : buf[i]; if (v > pk) pk = v; }
        if (pk > 0.0001f) { float g = 0.95f / pk; for (int i = 0; i < n; i++) buf[i] *= g; }
    }
    sample_load(PB_SAMP, buf, n);
    instrument(PB_TGT, INSTR_SAMPLE, 1, 0, 7, 200);
    instrument_sample(PB_TGT, PB_SAMP, 60);
    for (int i = 0; i < 64; i++) {           // 64-bucket peak envelope for the strip
        int a = (int)((long)i * n / 64), b = (int)((long)(i + 1) * n / 64);
        float p = 0;
        for (int k = a; k < b; k++) { float v = buf[k] < 0 ? -buf[k] : buf[k]; if (v > p) p = v; }
        tgt_peak[i] = p;
    }
    free(buf);
    target_len = n; have_target = 1;
}

static void say(const char *s) { snprintf(status, sizeof status, "%s", s); status_t = 1.6f; }

// Configure PB_SLOT to be candidate i, then overlay whatever the knobs have been dragged to.
// Only on a CHANGE: instrument() rebuilds a slot, and doing that every frame is the set-and-hold
// mistake that makes a cart stutter rather than fail.
static void use(int i) {
    if (i < 0 || i >= PB_N) return;
    if (applied != i) { pb_apply(i, PB_SLOT); applied = i; }
    if (!mac_ready[i]) {                      // first visit: the knobs start where the patch is
        mac[i][0] = 0.5f; mac[i][1] = 0.5f; mac[i][2] = 0.5f;
        mac_ready[i] = 1;
        return;                               // leave the patch's own macro values alone
    }
    instrument_harmonics(PB_SLOT, mac[i][0]);
    instrument_timbre(PB_SLOT, mac[i][1]);
    instrument_morph(PB_SLOT, mac[i][2]);
}

// instrument_level SURVIVES a redefine (studio.h: only the wave and the ADSR are replaced), so the
// trim is applied once and holds across candidate switches. Set-and-hold, never per frame.
static void apply_bal(void) { instrument_level(PB_SLOT, bal); }

static void play_cand(void) { use(sel); hit(note_midi, PB_SLOT, 5, NOTE_MS); }
static void play_target(void) {
    if (!have_target) { say("no target: this run has none"); return; }
    hit(60, PB_TGT, 5, NOTE_MS);              // 60 = the sample's own speed, never transposed
}

static void copy_patch(void) {
    printh("// patchbench: %s candidate %d (%s)%s",
           PB_RUN[0] ? PB_RUN : "built-in", sel + 1, PB_NAME[sel],
           mac_ready[sel] ? "  [macros as set on the bench]" : "");
    printh("    // paste this into your cart");
    if (mac_ready[sel])
        printh("    instrument_harmonics(5, %.3ff);  instrument_timbre(5, %.3ff);  instrument_morph(5, %.3ff);",
               mac[sel][0], mac[sel][1], mac[sel][2]);
    printh("    // (the instrument() line is candidate %d in this cart's de:patch-slots region)", sel + 1);
    say("patch printed to the log panel");
}

void init(void) {
    load_target();
    use(0);
    apply_bal();
    if (!have_target && PB_RUN[0]) say("target.wav not found for this run");
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (status_t > 0) status_t -= dt;

    for (int i = 0; i < PB_N && i < 9; i++)
        if (keyp('1' + i)) { sel = i; play_cand(); }
    if (keyp('T')) play_target();
    if (keyp(KEY_SPACE)) play_cand();
    if (keyp('B')) { ab_stage = 1; ab_t = 0; play_target(); say("A/B: target"); }
    if (keyp(KEY_UP)   && note_midi < 96) note_midi++;
    if (keyp(KEY_DOWN) && note_midi > 24) note_midi--;

    if (ab_stage == 1) {                       // hand over to the candidate when the target is done
        ab_t += dt;
        if (ab_t > NOTE_MS / 1000.0f + 0.10f) { ab_stage = 2; play_cand(); say("A/B: candidate"); }
    } else if (ab_stage == 2) {
        ab_t += dt;
        if (ab_t > 2 * (NOTE_MS / 1000.0f) + 0.20f) ab_stage = 0;
    }

#ifdef DE_TRACE
    watch("sel", "%d", sel);
    watch("note", "%d", note_midi);
    watch("target", "%d", have_target);
#endif
}

static void draw_target_strip(int x, int y, int w, int h) {
    rectfill(x, y, w, h, CLR_DARKER_GREY);
    rect(x, y, w, h, ab_stage == 1 ? CLR_YELLOW : CLR_DARK_GREY);
    if (have_target) {
        int mid = y + h / 2;
        for (int i = 0; i < 64; i++) {
            int px = x + 2 + i * (w - 4) / 64;
            int ph = (int)(tgt_peak[i] * (h / 2 - 2));
            if (ph < 1) ph = 1;
            line(px, mid - ph, px, mid + ph, CLR_INDIGO);
        }
    } else {
        font(FONT_SMALL);
        print(PB_RUN[0] ? "target.wav missing" : "no run loaded - drop a .wav on the editor",
              x + 4, y + h / 2 - 3, CLR_DARK_GREY);
        font(FONT_NORMAL);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    ui_begin();       // presses are recorded here and resolved in ui_end(); without the pair
                      // every widget draws but nothing clicks, and the engine says so out loud

    print("PATCHBENCH", 4, 3, CLR_WHITE);
    font(FONT_SMALL);
    {
        char sub[64];
        snprintf(sub, sizeof sub, "run: %s   note %d", PB_RUN[0] ? PB_RUN : "(built-in patches)", note_midi);
        print(sub, 4, 12, CLR_MEDIUM_GREY);
    }
    font(FONT_NORMAL);

    draw_target_strip(4, 22, 240, 26);
    if (ui_button(248, 22, 68, 12, have_target ? "T target" : "no target")) play_target();
    if (ui_button(248, 36, 68, 12, "B  A/B")) { ab_stage = 1; ab_t = 0; play_target(); say("A/B: target"); }

    // the pads: the candidate set, which is the whole reason this cart exists
    for (int i = 0; i < PB_N && i < PB_MAXN; i++) {
        int col = i % 4, row = i / 4;
        int x = 4 + col * 79, y = 54 + row * 30, w = 74, h = 26;
        // NULL label: ui_button draws the fill, frame and capture, and the two lines of text are
        // drawn here instead. Its own label is CENTRED, which put it straight through the loss
        // figure below — ui-audit called that out as 8 overlapping pairs, which is what it is for.
        int act = ui_button(x, y, w, h, NULL);
        if (i == sel) rect(x - 1, y - 1, w + 2, h + 2, CLR_YELLOW);   // selection reads OUTSIDE it
        char lab[24];
        snprintf(lab, sizeof lab, "%d %s", i + 1, PB_NAME[i]);
        print(lab, x + 4, y + 5, i == sel ? CLR_WHITE : CLR_LIGHT_GREY);
        font(FONT_SMALL);
        if (PB_LOSS[i] >= 0) {
            char ls[16]; snprintf(ls, sizeof ls, "fx %.3f", PB_LOSS[i]);
            print(ls, x + 4, y + 16, CLR_MEDIUM_GREY);
        } else {
            print("hand-written", x + 4, y + 16, CLR_MEDIUM_GREY);
        }
        font(FONT_NORMAL);
        if (act) { sel = i; play_cand(); }
    }

    // the selected candidate, and the three macros you can ride while it sounds
    int by = 54 + ((PB_N + 3) / 4) * 30 + 6;
    {
        char h[48];
        snprintf(h, sizeof h, "selected: %d  %s", sel + 1, PB_NAME[sel]);
        print(h, 4, by, CLR_YELLOW);
    }
    if (ui_knob(&mac[sel][0],  30, by + 32, "harm"))  { mac_ready[sel] = 1; use(sel); }
    if (ui_knob(&mac[sel][1],  86, by + 32, "timb"))  { mac_ready[sel] = 1; use(sel); }
    if (ui_knob(&mac[sel][2], 142, by + 32, "morph")) { mac_ready[sel] = 1; use(sel); }
    if (ui_knob(&bal, 254, by + 32, "bal"))            { bal_ready = 1; apply_bal(); }

    if (ui_button(186, by + 12, 56, 13, "play")) play_cand();
    if (ui_button(186, by + 28, 56, 13, "COPY")) copy_patch();

    font(FONT_SMALL);
    print("1-8 pick + play   T target   B a/b   SPACE play", 4, SCREEN_H - 18, CLR_DARK_GREY);
    if (status_t > 0) print(status, 4, SCREEN_H - 9, CLR_YELLOW);
    else print("up/down moves the note the bench plays", 4, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}
