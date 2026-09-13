/* de:meta
{
  "slug": "patchbench",
  "title": "patchbench",
  "status": "active",
  "created": "2026-09-12",
  "kind": ["instrument", "probe"],
  "genre": null,
  "teaches": ["adsr-envelope"],
  "lineage": "The landing pad for tools/patch-match, and the sprout tree it was one mutation operator away from (docs/design/patch-matching-cart.md §7 + option B, #16). The CLI hands back a SET; this cart holds it, plays it, and lets the ear breed mutations around a keep. No scoring — the ear is the loss.",
  "description": {
    "summary": "Hear eight patches, keep the one you like, breed mutations around it. The landing pad for a patch-match run and the Synplant toy that grows from it: your ear is the loss, nothing is scored, nothing renders offline.",
    "detail": "A cart with one job that grew a second honest verb. It still holds a SET of instrument patches — the eight candidates live in a `// de:patch-slots` region the editor rewrites after a search — and it still A/B's them against the target, rides the three macros, and COPY-prints pasteable C. On top of that: pick a pad (that is the keep) and BREED refills the other pads with mutations of it, walked in the same 0..1 space `pmpatch.h` already defined, with snapped axes stepping detents instead of dying between them. UNDO walks back the last few litters. A run from the editor is generation 0; the first breed is when the bench becomes the sprout tree. With no run loaded it starts from three built-in patches, so there is always something to keep.",
    "controls": "1-8 — select and play that candidate (the selection is the keep). T — play the target. SPACE — play the selection. B — A/B (target, then the selection). R — breed mutations around the keep, refill the other pads. U — undo last breed. Click a pad to select and play it. HARM / TIMB / MORPH — ride the selected candidate's macros. SPRD — how far a breed wanders. COPY — print the selected patch as pasteable C. Up/down arrows move the note."
  },
  "todo": [
    "Maker ear: is mutation fun? That is #16's remaining blocker, not a missing operator.",
    "A run browser over build/patch-match/ belongs in the editor panel, not here."
  ]
}
de:meta */
#include "studio.h"
#include "ui.h"
#include "pmbreed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── THE EDITOR'S REGION ─────────────────────────────────────────────────────────────────────────
// Everything between the two markers is REWRITTEN by the editor after a patch-match search. Nothing
// else in this file is touched, which is the point: the result of a search has exactly one legal
// home, so it can never land in the middle of a cart you happened to have open.
//
// The shape is fixed. PB_RUN names the run directory under build/patch-match (the target lives
// there), PB_N is how many pads are live at generation 0, PB_NAME/PB_LOSS label them, pb_apply(i)
// configures PB_SLOT to be candidate i (the exact printed snippet — generation 0 plays this so an
// A landing sounds like what `pm` found), and pb_fill_seeds() writes the matching PmPatch vector
// so a breed has a real parent rather than a default-plus-macros guess.
// de:patch-slots begin
#define PB_RUN  ""              // "" = no run loaded, so no target to compare against
#define PB_N    3
static const char *PB_NAME[PB_N] = { "PLUCK", "EPIANO", "PIPE" };
static const float PB_LOSS[PB_N] = { -1.0f, -1.0f, -1.0f };   // <0 = no loss (hand-written, not searched)
static PmPatch PB_SEED[PB_N];
static void pb_fill_seeds(void) {
    pm_patch_default(&PB_SEED[0], 16);   // PLUCK
    PB_SEED[0].v[V_HARM] = 0.35f; PB_SEED[0].v[V_TIMB] = 0.50f; PB_SEED[0].v[V_MORPH] = 0.20f;
    pm_patch_adsr(&PB_SEED[0], 2, 90, 1, 120);
    pm_patch_default(&PB_SEED[1], 20);   // EPIANO
    PB_SEED[1].v[V_HARM] = 0.45f; PB_SEED[1].v[V_TIMB] = 0.55f; PB_SEED[1].v[V_MORPH] = 0.30f;
    pm_patch_adsr(&PB_SEED[1], 4, 400, 3, 300);
    pm_patch_default(&PB_SEED[2], 25);   // PIPE
    PB_SEED[2].v[V_HARM] = 0.20f; PB_SEED[2].v[V_TIMB] = 0.80f; PB_SEED[2].v[V_MORPH] = 0.10f;
    pm_patch_adsr(&PB_SEED[2], 60, 300, 5, 400);
}
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
#define PB_MAXN  8        // pads the layout draws; live_n grows to this on the first breed
#define PB_UNDO  4        // last-N litters; issue asked for at least undo-last

static int   sel = 0;                 // which candidate is selected (= the keep)
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

static PmPatch live[PB_MAXN];
static char    live_name[PB_MAXN][12];
static float   live_loss[PB_MAXN];
static int     live_kind[PB_MAXN];    // 0 = seed (gen 0), 1 = keep, 2 = child
static int     live_n = 0;
static int     bred = 0;              // 0 = still playing via pb_apply (exact A snippet)
static int     gen_n = 0;
static float   spread = 0.18f;

typedef struct {
    PmPatch p[PB_MAXN];
    char    name[PB_MAXN][12];
    float   loss[PB_MAXN];
    float   mac[PB_MAXN][3];
    int     ready[PB_MAXN];
    int     kind[PB_MAXN];
    int     n, sel, bred, gen;
} PbSnap;
static PbSnap undo[PB_UNDO];
static int    undo_n = 0;

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

static void load_generation0(void) {
    pb_fill_seeds();
    live_n = PB_N < PB_MAXN ? PB_N : PB_MAXN;
    for (int i = 0; i < live_n; i++) {
        live[i] = PB_SEED[i];
        snprintf(live_name[i], sizeof live_name[i], "%s", PB_NAME[i]);
        live_loss[i] = PB_LOSS[i];
        live_kind[i] = 0;
        mac[i][0] = live[i].v[V_HARM];
        mac[i][1] = live[i].v[V_TIMB];
        mac[i][2] = live[i].v[V_MORPH];
        mac_ready[i] = 0;                    // first play leaves the snippet's own macros
    }
    bred = 0;
    gen_n = 0;
    sel = 0;
    applied = -1;
}

static void capture_macros(int i) {
    if (i < 0 || i >= live_n) return;
    if (!mac_ready[i]) return;
    live[i].v[V_HARM] = mac[i][0];
    live[i].v[V_TIMB] = mac[i][1];
    live[i].v[V_MORPH] = mac[i][2];
}

// Configure PB_SLOT to be candidate i, then overlay whatever the knobs have been dragged to.
// Only on a CHANGE: instrument() rebuilds a slot, and doing that every frame is the set-and-hold
// mistake that makes a cart stutter rather than fail.
//
// Generation 0 plays the editor's pb_apply snippet so an A landing is bit-exact with what `pm`
// printed. After the first breed the live PmPatch is the source of truth (the snippet cannot
// express a child).
static void use(int i) {
    if (i < 0 || i >= live_n) return;
    if (applied != i) {
        if (!bred && i < PB_N) pb_apply(i, PB_SLOT);
        else                   pm_apply_slot(&live[i], PB_SLOT);
        applied = i;
    }
    if (!mac_ready[i]) {                      // first visit: the knobs start where the patch is
        mac[i][0] = live[i].v[V_HARM];
        mac[i][1] = live[i].v[V_TIMB];
        mac[i][2] = live[i].v[V_MORPH];
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

static void play_cand(void) { use(sel); apply_bal(); hit(note_midi, PB_SLOT, 5, NOTE_MS); }
static void play_target(void) {
    if (!have_target) { say("no target: this run has none"); return; }
    hit(60, PB_TGT, 5, NOTE_MS);              // 60 = the sample's own speed, never transposed
}

static void push_undo(void) {
    if (undo_n == PB_UNDO) {
        memmove(&undo[0], &undo[1], sizeof(undo[0]) * (PB_UNDO - 1));
        undo_n--;
    }
    PbSnap *s = &undo[undo_n++];
    memset(s, 0, sizeof *s);
    s->n = live_n; s->sel = sel; s->bred = bred; s->gen = gen_n;
    for (int i = 0; i < live_n; i++) {
        s->p[i] = live[i];
        memcpy(s->name[i], live_name[i], sizeof live_name[i]);
        s->loss[i] = live_loss[i];
        s->kind[i] = live_kind[i];
        s->mac[i][0] = mac[i][0]; s->mac[i][1] = mac[i][1]; s->mac[i][2] = mac[i][2];
        s->ready[i] = mac_ready[i];
    }
}

static void breed(void) {
    if (sel < 0 || sel >= live_n) return;
    capture_macros(sel);
    push_undo();
    PmPatch parent = live[sel];
    char pname[12];
    snprintf(pname, sizeof pname, "%s", live_name[sel]);
    int keep = sel;
    live_n = PB_MAXN;
    for (int i = 0; i < PB_MAXN; i++) {
        if (i == keep) {
            live[i] = parent;
            snprintf(live_name[i], sizeof live_name[i], "%s", pname);
            live_kind[i] = 1;
            live_loss[i] = -1.0f;
        } else {
            unsigned seed = 1u + (unsigned)(gen_n + 1) * 7919u + (unsigned)(i + 1) * 104729u;
            pm_mutate(&parent, &live[i], seed, spread);
            snprintf(live_name[i], sizeof live_name[i], "%s~", pm_engine_short(live[i].engine));
            live_kind[i] = 2;
            live_loss[i] = -1.0f;
        }
        mac[i][0] = live[i].v[V_HARM];
        mac[i][1] = live[i].v[V_TIMB];
        mac[i][2] = live[i].v[V_MORPH];
        mac_ready[i] = 1;
    }
    bred = 1;
    gen_n++;
    applied = -1;
    {
        char msg[64];
        snprintf(msg, sizeof msg, "bred 7 around %d %s", keep + 1, pname);
        say(msg);
    }
}

static void undo_breed(void) {
    if (undo_n <= 0) { say("nothing to undo"); return; }
    PbSnap *s = &undo[--undo_n];
    live_n = s->n; sel = s->sel; bred = s->bred; gen_n = s->gen;
    for (int i = 0; i < live_n; i++) {
        live[i] = s->p[i];
        memcpy(live_name[i], s->name[i], sizeof live_name[i]);
        live_loss[i] = s->loss[i];
        live_kind[i] = s->kind[i];
        mac[i][0] = s->mac[i][0]; mac[i][1] = s->mac[i][1]; mac[i][2] = s->mac[i][2];
        mac_ready[i] = s->ready[i];
    }
    applied = -1;
    say("undid last breed");
}

static void copy_patch(void) {
    capture_macros(sel);
    const PmPatch *p = &live[sel];
    printh("// patchbench: %s  pad %d (%s)  gen %d%s",
           PB_RUN[0] ? PB_RUN : "built-in", sel + 1, live_name[sel], gen_n,
           live_kind[sel] == 2 ? "  child" : live_kind[sel] == 1 ? "  keep" : "");
    printh("    // paste this into your cart");
    printh("    instrument(5, %s, %d, %d, %d, %d);",
           pm_engine_name(p->engine),
           pm_atk_ms(p->v[V_ATK]), pm_dec_ms(p->v[V_DEC]),
           pm_sus(p->v[V_SUS]), pm_rel_ms(p->v[V_REL]));
    printh("    instrument_harmonics(5, %.3ff);  instrument_timbre(5, %.3ff);  instrument_morph(5, %.3ff);",
           p->v[V_HARM], p->v[V_TIMB], p->v[V_MORPH]);
    int fb = pm_bin(p->v[V_FMODE], 4);
    if (fb)
        printh("    instrument_filter(5, %s, %d, %d);",
               PM_FILTER_NAME[fb], pm_cut_hz(p->v[V_CUT]), pm_res(p->v[V_RES]));
    if (p->f[F_DRIVE] > 0.02f)
        printh("    instrument_drive(5, %.3ff);  instrument_drive_mode(5, %s);",
               p->f[F_DRIVE], PM_DRIVE_NAME[pm_bin(p->f[F_DRIVEMODE], 4)]);
    if (p->f[F_TAPEWOW] > 0.02f || p->f[F_TAPEFLUT] > 0.02f || p->f[F_TAPESAT] > 0.02f)
        printh("    instrument_tape(5, %.3ff, %.3ff, %.3ff);",
               p->f[F_TAPEWOW], p->f[F_TAPEFLUT], p->f[F_TAPESAT]);
    if (p->f[F_ECHOSEND] > 0.02f)
        printh("    echo(%d, %.3ff, %.3ff);  instrument_echo(5, %.3ff);",
               pm_echo_ms(p->f[F_ECHOTIME]), pm_echo_fb(p->f[F_ECHOFB]), p->f[F_ECHOTONE], p->f[F_ECHOSEND]);
    if (p->f[F_RVBSEND] > 0.02f)
        printh("    reverb(%.3ff, %.3ff);  instrument_reverb(5, %.3ff);",
               p->f[F_RVBSIZE], p->f[F_RVBDAMP], p->f[F_RVBSEND]);
    say("patch printed to the log panel");
}

void init(void) {
    load_generation0();
    load_target();
    use(0);
    apply_bal();
    if (!have_target && PB_RUN[0]) say("target.wav not found for this run");
}

void update(void) {
    float dt = 1.0f / 60.0f;
    if (status_t > 0) status_t -= dt;

    for (int i = 0; i < live_n && i < 9; i++)
        if (keyp('1' + i)) { sel = i; play_cand(); }
    if (keyp('T')) play_target();
    if (keyp(KEY_SPACE)) play_cand();
    if (keyp('B')) { ab_stage = 1; ab_t = 0; play_target(); say("A/B: target"); }
    if (keyp('R')) breed();
    if (keyp('U')) undo_breed();
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
    watch("gen", "%d", gen_n);
    watch("undo", "%d", undo_n);
    watch("bred", "%d", bred);
    watch("live_n", "%d", live_n);
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

    print("PATCHBENCH", 4, 2, CLR_WHITE);
    font(FONT_SMALL);
    {
        char sub[72];
        if (gen_n > 0)
            snprintf(sub, sizeof sub, "%s  note %d  gen %d",
                     PB_RUN[0] ? PB_RUN : "built-in", note_midi, gen_n);
        else
            snprintf(sub, sizeof sub, "%s  note %d",
                     PB_RUN[0] ? PB_RUN : "built-in", note_midi);
        print(sub, 96, 4, CLR_MEDIUM_GREY);
    }

    draw_target_strip(4, 14, 248, 20);
    // right rail: every verb lives here so eight pads never collide with a button
    if (ui_button(256, 14, 60, 10, have_target ? "T target" : "no tgt")) play_target();
    if (ui_button(256, 25, 60, 10, "B  A/B")) { ab_stage = 1; ab_t = 0; play_target(); say("A/B: target"); }
    if (ui_button(256, 36, 60, 10, "R BREED")) breed();
    if (ui_button(256, 47, 60, 10, undo_n ? "U UNDO" : "U undo")) undo_breed();
    if (ui_button(256, 58, 60, 10, "play")) play_cand();
    if (ui_button(256, 69, 60, 10, "COPY")) copy_patch();

    // the pads: four columns, two rows, leave the rail free
    for (int i = 0; i < live_n && i < PB_MAXN; i++) {
        int col = i % 4, row = i / 4;
        int x = 4 + col * 62, y = 38 + row * 24, w = 58, h = 22;
        int act = ui_button(x, y, w, h, NULL);
        if (i == sel) rect(x - 1, y - 1, w + 2, h + 2, CLR_YELLOW);
        font(FONT_SMALL);   // ui_button may leave FONT_NORMAL; 8px "EPIANO" overflows a 58px pad
        char lab[24];
        snprintf(lab, sizeof lab, "%d %s", i + 1, live_name[i]);
        print(lab, x + 3, y + 3, i == sel ? CLR_WHITE : CLR_LIGHT_GREY);
        if (live_kind[i] == 1)      print("keep", x + 3, y + 12, CLR_YELLOW);
        else if (live_kind[i] == 2) print("child", x + 3, y + 12, CLR_INDIGO);
        else if (live_loss[i] >= 0) {
            char ls[16]; snprintf(ls, sizeof ls, "fx %.3f", live_loss[i]);
            print(ls, x + 3, y + 12, CLR_MEDIUM_GREY);
        } else {
            print("seed", x + 3, y + 12, CLR_MEDIUM_GREY);
        }
        if (act) { sel = i; play_cand(); }
    }
    font(FONT_NORMAL);

    int by = 38 + ((live_n + 3) / 4) * 24 + 4;
    {
        char h[48];
        snprintf(h, sizeof h, "keep %d %s", sel + 1, live_name[sel]);
        print(h, 4, by, CLR_YELLOW);
    }
    if (ui_knob(&mac[sel][0],  28, by + 28, "harm"))  { mac_ready[sel] = 1; capture_macros(sel); use(sel); }
    if (ui_knob(&mac[sel][1],  74, by + 28, "timb"))  { mac_ready[sel] = 1; capture_macros(sel); use(sel); }
    if (ui_knob(&mac[sel][2], 120, by + 28, "morph")) { mac_ready[sel] = 1; capture_macros(sel); use(sel); }
    if (ui_knob(&spread,      166, by + 28, "sprd"))  { if (spread < 0.04f) spread = 0.04f; }
    if (ui_knob(&bal,         212, by + 28, "bal"))   { bal_ready = 1; apply_bal(); }

    font(FONT_SMALL);
    print("1-8 keep  R breed  U undo  T target  B a/b  SPACE play", 4, SCREEN_H - 9,
          status_t > 0 ? CLR_DARK_GREY : CLR_DARK_GREY);
    if (status_t > 0) print(status, 4, SCREEN_H - 18, CLR_YELLOW);
    else              print("up/down moves the note the bench plays", 4, SCREEN_H - 18, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_end();
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(1);                                          // init() + one update
    expect(live_n == 3, "generation 0 is the built-in set");
    expect(undo_n == 0, "no undo on a fresh bench");
    expect(bred == 0, "generation 0 still plays via pb_apply");
    expect_eq(pm_mutate_selfcheck(), 0, "pm_mutate known answers");

    sel = 1;                                          // keep the EPIANO
    breed();
    expect(live_n == 8, "breed fills eight pads");
    expect(bred == 1, "after breed the live vector is the source of truth");
    expect_eq(gen_n, 1, "first breed is generation 1");
    expect_eq(undo_n, 1, "breed pushes one undo");
    expect(live_kind[1] == 1, "the pick stays the keep, on its own pad");
    expect(live[1].engine == 20, "keep is still EPIANO");
    int moved = 0;
    for (int i = 0; i < 8; i++)
        if (i != 1 && memcmp(&live[i], &live[1], sizeof(PmPatch)) != 0) moved++;
    expect(moved >= 6, "the other pads are children, not copies");
    expect(live[0].engine == 20 && live[7].engine == 20, "children stay in the keep's family");

    int n1 = live_n, g1 = gen_n;
    undo_breed();
    expect(live_n == 3, "undo restores the set size");
    expect(bred == 0, "undo restores generation-0 playback");
    expect_eq(gen_n, 0, "undo restores the generation counter");
    expect_eq(undo_n, 0, "undo consumes the stack");
    expect(live[1].engine == 20, "undo restores the pick");
    expect(n1 == 8 && g1 == 1, "the pre-undo snapshot was the bred litter");

    spec_tap('R');
    expect(live_n == 8, "R breeds");
    spec_tap('U');
    expect(live_n == 3, "U undoes");

    // two breeds, one undo — the stack is a lineage, not a toggle
    breed();
    int keep_pad = sel;
    int child_engine = live[keep_pad == 0 ? 1 : 0].engine;
    breed();
    expect_eq(gen_n, 2, "second breed is generation 2");
    expect_eq(undo_n, 2, "two litters on the stack");
    undo_breed();
    expect_eq(gen_n, 1, "undo last breed only");
    expect(live_n == 8, "the previous litter is still eight pads");
    expect(live[keep_pad == 0 ? 1 : 0].engine == child_engine, "gen-1 children come back");
}
#endif
