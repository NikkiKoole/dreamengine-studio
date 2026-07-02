/* de:meta
{
  "title": "acid rack",
  "status": "active",
  "created": "2026-07-02",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "subtractive-synth",
    "drum-synthesis"
  ],
  "lineage": "Propellerhead ReBirth RB-338 (1997) — the 2×TB-303 + drum-machine acid rack — rebuilt as one cart from the shipped machine carts (tb303 ×2 on the new FILTER_DIODE, tr909 voices), with Reason-style rack-fold accordion panels and pattern banks + a chain row = song mode. Increment 1 of docs/design/rebirth-classic.md.",
  "homage": "Propellerhead ReBirth RB-338 (1997)",
  "todo": [
    "increment 2: full 808 + 909 panels (curated slots, strokes, metal XY, shuffle)",
    "increment 3: FX strip (drive/echo/glue comp/PCF filter lane)",
    "increment 4: seeded generator + song code + WAV export",
    "touch pass: per-finger roll/grid painting (pointer.h), fat-finger pads",
    "maker naming pass: 'acid rack' is a working title"
  ],
  "description": {
    "summary": "Two 303s and a drum machine in one rack — pattern banks A-D chained into a real song. The ReBirth move: everything runs together, you edit while it plays.",
    "detail": "The RB-338 homage, increment 1: two full TB-303 voices (the engine's FILTER_DIODE diode-ladder squelch, authentic non-retriggering slide, accent into the filter env, live CUT/RES/DRV knobs per machine) + a four-voice 909 starter kit (kick with the ATTACK click layer, clap retriggers, FM-clang hats with choke), all clocked off one transport. The rack is an ACCORDION: each machine is a slim strip showing its name, a live 16-tick mini pattern with the playhead, and a MUTE — tap a strip to expand its full editor (piano roll + flag rows + knobs for a 303, trigger grid for the drums). Sound never depends on what's open. Patterns live in four BANKS (A-D, the transport buttons); the SONG row at the bottom chains up to 64 bars of banks into a real track — tap a cell to cycle A→B→C→D→empty. Everything autosaves.",
    "controls": "SPACE run/stop · tap a strip header to expand · A-D buttons pick the edit bank (also LEFT/RIGHT) · SONG button toggles chain playback · tap SONG-row cells to write the arrangement · UP/DOWN tempo · roll: tap/drag paints notes, tap a note to erase · OCT/ACC/SLD rows toggle per step · drums: tap cells"
  }
}
de:meta */
#include "studio.h"
#include "ui.h"          // widgets: ui_button/ui_knob — per-finger capture, focus ring
#include <stdio.h>
#include <math.h>
#include <string.h>

// ── ACID RACK ─────────────────────────────────────────────────────────────
// ReBirth RB-338 (1997) put "acid techno in a box": two TB-303s and Roland
// drum machines wired to one transport, pattern banks, a song chain. This
// cart rebuilds that shape from the machines this console already ships:
// the tb303 cart's voice (upgraded to FILTER_DIODE — audio-notes §25) twice,
// and the tr909 cart's kick/clap/hats as the starter kit (the full curated
// 808 + 909 panels are increment 2 — see docs/design/rebirth-classic.md).
//
// The two design invariants (from the design doc):
//   1. sound NEVER depends on what's expanded — the sequencer runs globally;
//      the accordion is pure view. Fold the drums mid-roll, nothing changes.
//   2. folded ≠ blind — every strip header carries a live 16-tick mini
//      pattern with the playhead, so the whole rack visibly breathes.
//
// Data model: a PATTERN is a whole-rack snapshot (both 303 lines + the drum
// grid). Four pattern BANKS (A-D); the SONG row chains up to 64 bank picks,
// one per bar. The generator (increment 4) will fill A sparse → D drop.

#define STEPS   16
#define NBANK   4
#define CHAINN  64
#define BASE    36            // C2 — bottom row of the 303 roll
#define NDRUM   4             // increment-1 starter kit: BD CP CH OH

// instrument slots — the rack's slot plan (rebirth-classic §1: 2×303 = 9,10;
// drums from 11 up, growing to the full curated set in increment 2)
#define SLOT_A   9            // 303-A
#define SLOT_B   10           // 303-B
#define SL_BD    11
#define SL_BDC   12           // kick click layer (the 909 ATTACK knob sound)
#define SL_CP    13
#define SL_HHC   14
#define SL_HHO   15

// ── pattern data ──────────────────────────────────────────────────────────
typedef struct {                       // one 303 line in one bank
    unsigned char  pitch[STEPS];       // semitone 0..12 above BASE
    unsigned short on, oct, acc, sld;  // per-step bitmasks
} Line;
typedef struct {                       // one bank = a whole-rack snapshot
    Line           ln[2];              // [0]=303-A [1]=303-B
    unsigned short drum[NDRUM];        // trigger masks
} Pattern;

static Pattern bank[NBANK];
static unsigned char chain[CHAINN];    // bank index per bar; 0xFF = empty (song ends)
static int  chainN     = 0;            // bars in the song (first empty cell)

// ── transport / play state ────────────────────────────────────────────────
static int  tempo = 130;
static bool running = true;
static bool songmode = true;           // play the chain (else: loop the edit bank)
static int  editBank = 0;              // which bank A-D the panels edit
static int  playBank = 0;              // which bank is sounding this bar
static int  barpos = 0;                // position in the chain
static int  last16 = -1, playhead = 0;
static int  drumflash[NDRUM];          // frames remaining on the header LED

// ── the machines ──────────────────────────────────────────────────────────
enum { K_CUT, K_RES, K_ENV, K_DEC, K_ACC, K_DRV, K_SQL, NK };
static const char *KNAME[NK] = { "CUT", "RES", "ENV", "DEC", "ACC", "DRV", "SQL" };

typedef struct {
    int   slot;
    const char *name;
    float knob[NK];         // 0..1 (ui_knob's range)
    int   wave;             // INSTR_SAW / INSTR_SQUARE
    int   h;                // held note handle (-1 = none)
    bool  prev_slide;
} M303;
static M303 m[2] = {
    { SLOT_A, "303-A", { 0.45f, 0.70f, 0.60f, 0.40f, 0.60f, 0.35f, 0.33f }, INSTR_SAW,    -1, false },
    { SLOT_B, "303-B", { 0.38f, 0.75f, 0.55f, 0.45f, 0.60f, 0.45f, 0.33f }, INSTR_SQUARE, -1, false },
};

// knob value mappings — same curves as tb303.c, knobs normalized 0..1
static int   cut_hz(M303 *s)  { return (int)(60.0f * powf(2.0f, s->knob[K_CUT] * 6.0f)); } // 60..3840
static int   res_q(M303 *s)   { return (int)(s->knob[K_RES] * 15.0f); }
static float env_hz(M303 *s)  { return s->knob[K_ENV] * 3000.0f; }
static int   dec_ms(M303 *s)  { return 30 + (int)(s->knob[K_DEC] * 500.0f); }
static float acc_mul(M303 *s) { return 1.0f + s->knob[K_ACC] * 1.5f; }
static float sq_mul(M303 *s)  { return 1.0f + s->knob[K_SQL] * 2.0f; }

// the 303 voice — tb303.c's recipe verbatim, on the diode ladder
static void define_303(M303 *s) {
    instrument(s->slot, s->wave, 2, 60, 6, 25);
    instrument_duty(s->slot, 0.48f);
    instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
    instrument_drive(s->slot, s->knob[K_DRV]);
    instrument_echo(s->slot, 0.10f);                 // the subtle acid slapback
}
static void knob_changed_303(M303 *s, int k) {
    if (k == K_CUT || k == K_RES) {
        instrument_filter(s->slot, FILTER_DIODE, cut_hz(s), res_q(s));
        if (s->h >= 0) { note_cutoff(s->h, cut_hz(s)); note_res(s->h, (float)res_q(s)); }
    } else if (k == K_DRV) {
        instrument_drive(s->slot, s->knob[K_DRV]);
        if (s->h >= 0) note_drive(s->h, s->knob[K_DRV]);
    }                                                // ENV/DEC/ACC/SQL apply at next trigger
}
static void off_303(M303 *s) { if (s->h >= 0) { note_off(s->h); s->h = -1; } s->prev_slide = false; }

// step trigger — the authentic circuit, from tb303.c: a slid step does NOT
// retrigger (glide carries the pitch, the filter env keeps decaying); accent
// is louder AND a harder env kick; non-slid gates drop at ~70% of the step.
static void step_303(M303 *s, Line *ln, int st) {
    int b = 1 << st;
    if (ln->on & b) {
        int midi = BASE + ln->pitch[st] + ((ln->oct & b) ? 12 : 0);
        int vol  = (ln->acc & b) ? 7 : 5;
        if (s->h >= 0 && s->prev_slide) {
            note_glide(s->h, 60);
            note_pitch(s->h, (float)midi);
            note_vol(s->h, vol);                     // env does NOT refire — authentic
        } else {
            if (s->h >= 0) note_off(s->h);
            float e = env_hz(s) * sq_mul(s) * ((ln->acc & b) ? acc_mul(s) : 1.0f);
            instrument_env(s->slot, 0, ENV_CUTOFF, 0, dec_ms(s), e);
            s->h = note_on(midi, s->slot, vol);
            note_glide(s->h, 0);
        }
        s->prev_slide = (ln->sld & b) != 0;
    } else {
        off_303(s);
    }
}
static void gate_303(M303 *s, Line *ln, int st) {   // staccato release mid-step
    int b = 1 << st;
    if (s->h >= 0 && (ln->on & b) && !(ln->sld & b)) {
        float f = beat_pos() * 4.0f; f -= (int)f;
        if (f > 0.7f) { note_off(s->h); s->h = -1; }
    }
}

// ── the starter drum kit — tr909.c voice recipes verbatim ─────────────────
static const char *DNAME[NDRUM] = { "BD", "CP", "CH", "OH" };
static void define_drums(void) {
    // kick — sine body, +30st sweep over 35ms, run hot into the desk (tanh)
    instrument(SL_BD, INSTR_SINE, 0, 300, 0, 50);
    instrument_filter(SL_BD, FILTER_LOW, 380, 2);
    instrument_env(SL_BD, 0, ENV_PITCH, 0, 35, 30.0f);
    instrument_drive(SL_BD, 0.35f);
    instrument(SL_BDC, INSTR_NOISE, 0, 9, 0, 4);          // the ATTACK click
    instrument_filter(SL_BDC, FILTER_HIGH, 2500, 2);
    // clap — bandpassed noise; the retriggers in fire make the clap
    instrument(SL_CP, INSTR_NOISE, 0, 100, 0, 45);
    instrument_filter(SL_CP, FILTER_BAND, 1200, 5);
    // hats — the FM-clang ROM stand-ins: 3.5 detent, feedback high, then a
    // highpass that starts 5kHz LOW and rises (negative ENV_CUTOFF) = the
    // sampled hat's fast-closing sizzle. Closed chokes open, like hardware.
    instrument(SL_HHC, INSTR_FM, 0, 40, 0, 16);
    instrument_harmonics(SL_HHC, 0.55f);
    instrument_timbre(SL_HHC, 0.90f);
    instrument_morph(SL_HHC, 0.85f);
    instrument_env(SL_HHC, 0, ENV_CUTOFF, 0, 28, -5000.0f);
    instrument_filter(SL_HHC, FILTER_HIGH, 7310, 4);      // tr909's metal-pad default
    instrument(SL_HHO, INSTR_FM, 0, 380, 0, 110);
    instrument_harmonics(SL_HHO, 0.55f);
    instrument_timbre(SL_HHO, 0.90f);
    instrument_morph(SL_HHO, 0.85f);
    instrument_env(SL_HHO, 0, ENV_CUTOFF, 0, 220, -4500.0f);
    instrument_filter(SL_HHO, FILTER_HIGH, 6880, 4);
    instrument_choke(SL_HHC, SL_HHO);
}
static void fire_drum(int v) {
    switch (v) {
    case 0:  // BD: fast sweep body + click layer
        schedule_hit(0, 32, SL_BD, 6, 320);
        schedule_hit(0, 60, SL_BDC, 3, 10);
        break;
    case 1:  // CP: three retriggers ~9ms apart + a soft room tail
        schedule_hit(0,  60, SL_CP, 5, 11);
        schedule_hit(9,  60, SL_CP, 5, 11);
        schedule_hit(18, 60, SL_CP, 5, 11);
        schedule_hit(26, 60, SL_CP, 3, 130);
        break;
    case 2: schedule_hit(0, 97, SL_HHC, 4, 40);  break;  // CH
    case 3: schedule_hit(0, 97, SL_HHO, 4, 400); break;  // OH
    }
    drumflash[v] = 6;
}

// ── the accordion ─────────────────────────────────────────────────────────
enum { STRIP_A, STRIP_B, STRIP_DRUM, NSTRIP };
static const char *SNAME[NSTRIP] = { "303-A", "303-B", "DRUMS" };
static int  expanded = STRIP_A;
static bool mute[NSTRIP];

// layout — transport bar / strips / song chain row
#define TB_H    22
#define HDR_H   20
#define PANEL_H 120
#define CHAIN_Y 222
static int strip_y(int i) {            // header top of strip i
    int y = TB_H + 2;
    for (int k = 0; k < i; k++) y += HDR_H + (k == expanded ? PANEL_H : 0);
    return y;
}

// ── authored demo patterns (the generator fills these in increment 4) ─────
// 303 lines as tb303-style strings: nt '.'=rest '0'-'9','A'-'C'=semitone;
// oc/ac/sl = 'x' masks. Drums as one 'x' mask per voice (BD CP CH OH).
typedef struct { const char *nt, *oc, *ac, *sl; } LineSrc;
typedef struct { LineSrc a, b; const char *dr[NDRUM]; } PatSrc;
static const PatSrc DEMO[NBANK] = {
    { // A — sparse intro: one 303 murmurs over the kick
      { "0...3...0...7...", "................", "x...........x...", "................" },
      { "................", "................", "................", "................" },
      { "x...x...x...x...", "................", "................", "................" } },
    { // B — the groove: clap lands, hats breathe, B answers A
      { "0.C03.7A0.C0537A", "................", "x...x...x...x...", "......x.......x." },
      { "0...............", "x...............", "x...............", "................" },
      { "x...x...x...x...", "....x.......x...", "..x...x...x...x.", "................" } },
    { // C — the build: A climbs an octave, kick doubles into the turn
      { "0.C03.7A0.C0537A", "........xxxxxxxx", "x...x...x...x...", "......x.......x." },
      { "0.0.0.0.0.0.0.0.", "................", "x...x...x...x...", "................" },
      { "x...x...x...x.xx", "....x.......x...", "xxxxxxxxxxxxxxxx", "................" } },
    { // D — the drop: open hats, both boxes accented and sliding
      { "0.C03.7A0.C0537A", "x.......x.......", "x.x.x.x.x.x.x.x.", "..x...x...x...x." },
      { "0..7..A.0..3..C.", ".........x......", "x.......x.......", "..x.....x......." },
      { "x...x...x...x...", "....x.......x...", "x...x...x...x...", "..x...x...x...x." } },
};
static const char *DEMO_CHAIN = "AABBAABBCCCDBBDD";

static void decode_line(Line *ln, const LineSrc *src) {
    memset(ln, 0, sizeof *ln);
    for (int s = 0; s < STEPS; s++) {
        char c = src->nt[s];
        if (c != '.') {
            ln->on |= 1 << s;
            ln->pitch[s] = (c >= '0' && c <= '9') ? c - '0' : c - 'A' + 10;
        }
        if (src->oc[s] == 'x') ln->oct |= 1 << s;
        if (src->ac[s] == 'x') ln->acc |= 1 << s;
        if (src->sl[s] == 'x') ln->sld |= 1 << s;
    }
}
static void load_demo(void) {
    for (int bk = 0; bk < NBANK; bk++) {
        decode_line(&bank[bk].ln[0], &DEMO[bk].a);
        decode_line(&bank[bk].ln[1], &DEMO[bk].b);
        for (int d = 0; d < NDRUM; d++) {
            bank[bk].drum[d] = 0;
            for (int s = 0; s < STEPS; s++)
                if (DEMO[bk].dr[d][s] == 'x') bank[bk].drum[d] |= 1 << s;
        }
    }
    chainN = 0;
    memset(chain, 0xFF, sizeof chain);
    for (const char *c = DEMO_CHAIN; *c && chainN < CHAINN; c++)
        chain[chainN++] = *c - 'A';
}

// ── save / load (autosaves the whole song) ────────────────────────────────
#define SAVE_MAGIC 0xAC1D0001u
typedef struct {
    unsigned magic;
    Pattern  bank[NBANK];
    unsigned char chain[CHAINN];
    int      chainN, tempo, editBank;
    float    knob[2][NK];
    int      wave[2];
    bool     songmode, mute[NSTRIP];
} SaveBlob;
static int  save_cooldown = 0;         // >0 → a save is pending
static void mark_dirty(void) { save_cooldown = 45; }
static void save_song(void) {
    SaveBlob sb; memset(&sb, 0, sizeof sb);
    sb.magic = SAVE_MAGIC;
    memcpy(sb.bank, bank, sizeof bank);
    memcpy(sb.chain, chain, sizeof chain);
    sb.chainN = chainN; sb.tempo = tempo; sb.editBank = editBank;
    for (int i = 0; i < 2; i++) { memcpy(sb.knob[i], m[i].knob, sizeof m[i].knob); sb.wave[i] = m[i].wave; }
    sb.songmode = songmode;
    memcpy(sb.mute, mute, sizeof mute);
    save_bytes(&sb, sizeof sb);
}
static bool load_song(void) {
    SaveBlob sb;
    if (load_bytes(&sb, sizeof sb) != sizeof sb || sb.magic != SAVE_MAGIC) return false;
    memcpy(bank, sb.bank, sizeof bank);
    memcpy(chain, sb.chain, sizeof chain);
    chainN = sb.chainN; tempo = sb.tempo; editBank = sb.editBank;
    for (int i = 0; i < 2; i++) { memcpy(m[i].knob, sb.knob[i], sizeof m[i].knob); m[i].wave = sb.wave[i]; }
    songmode = sb.songmode;
    memcpy(mute, sb.mute, sizeof sb.mute);
    return true;
}

// ── init ──────────────────────────────────────────────────────────────────
void init(void) {
    if (!load_song()) load_demo();
    define_303(&m[0]);
    define_303(&m[1]);
    define_drums();
    bpm(tempo);
    echo(60000 * 3 / (tempo * 4), 0.3f, 0.35f);    // dotted-8th slapback, shared
}

// ── per-finger surface routing (the dubdesk fix, dubdesk.c:97) ────────────
// owned = the finger was captured by a ui.h widget → raw surfaces skip it
// until it lifts. seen = the finger existed last frame → !seen = a fresh tap.
#define NFING 12
static int  own_ids[NFING], own_n = 0;
static int  seen_ids[NFING], seen_n = 0;
static bool is_owned(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) return true; return false; }
static bool was_seen(int id)  { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) return true; return false; }
static void own_add(int id)   { if (!is_owned(id) && own_n < NFING) own_ids[own_n++] = id; }
static void seen_add(int id)  { if (!was_seen(id) && seen_n < NFING) seen_ids[seen_n++] = id; }
static void own_drop(int id)  { for (int i = 0; i < own_n; i++)  if (own_ids[i]  == id) { own_ids[i]  = own_ids[--own_n];   return; } }
static void seen_drop(int id) { for (int i = 0; i < seen_n; i++) if (seen_ids[i] == id) { seen_ids[i] = seen_ids[--seen_n]; return; } }

// ── update ────────────────────────────────────────────────────────────────
static void stop_all(void) { off_303(&m[0]); off_303(&m[1]); }

void update(void) {
    // keys
    if (keyp(KEY_SPACE)) { running = !running; last16 = -1; if (!running) stop_all(); }
    if (keyp(KEY_UP))    { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    if (keyp(KEY_DOWN))  { tempo -= 2; if (tempo < 60)  tempo = 60;  bpm(tempo); mark_dirty(); }
    if (keyp(KEY_LEFT))  { editBank = (editBank + NBANK - 1) % NBANK; }
    if (keyp(KEY_RIGHT)) { editBank = (editBank + 1) % NBANK; }
    if (keyp('S'))       { songmode = !songmode; mark_dirty(); }

    // ── raw-tap surfaces (roll / flags / grid / chain / headers) ──────────
    // sticky widget-ownership (the dubdesk fix): once ui.h captures a finger
    // (a knob/button grab) it stays owned until it LIFTS, so a knob drag that
    // wanders down over the roll never paints it. Capture is visible from the
    // frame after the press — geometry covers the press frame (no ui widget
    // sits on a raw surface). Every unowned finger paints independently.
    for (int i = 0; i < touch_count(); i++) if (ui_captured(touch_id(i))) own_add(touch_id(i));
    for (int i = 0; i < touch_ended_count(); i++) { own_drop(touch_ended_id(i)); seen_drop(touch_ended_id(i)); }

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i);
        if (is_owned(id)) continue;            // finger belongs to a knob/button
        int px = touch_x(i), py = touch_y(i);
        bool tap = !was_seen(id);              // first frame of this finger
        seen_add(id);
        Pattern *P = &bank[editBank];

        // strip headers: tap to expand (press only, and not on the MUTE zone)
        if (tap) for (int i = 0; i < NSTRIP; i++) {
            int y = strip_y(i);
            if (py >= y && py < y + HDR_H && px < 250) { expanded = i; break; }
        }

        // song chain row: tap a cell to cycle A→B→C→D→empty
        if (tap && py >= CHAIN_Y && py < CHAIN_Y + 14 && px >= 34 && px < 34 + CHAINN * 4) {
            int c = (px - 34) / 4;
            chain[c] = (chain[c] == 0xFF) ? 0 : (chain[c] + 1 > 3 ? 0xFF : chain[c] + 1);
            chainN = 0; while (chainN < CHAINN && chain[chainN] != 0xFF) chainN++;
            mark_dirty();
        }

        // expanded 303 panel: roll + flag rows (paint on drag, erase on tap)
        if (expanded == STRIP_A || expanded == STRIP_B) {
            Line *ln = &P->ln[expanded];
            int y0 = strip_y(expanded) + HDR_H;
            int rx = 40, ry = y0 + 30;
            if (px >= rx && px < rx + STEPS * 15 && py >= ry && py < ry + 13 * 5) {
                int st = (px - rx) / 15, row = 12 - (py - ry) / 5, b = 1 << st;
                if (tap && (ln->on & b) && ln->pitch[st] == row) ln->on &= ~b;   // tap the note = erase
                else { ln->on |= b; ln->pitch[st] = (unsigned char)row; }
                mark_dirty();
            }
            int fy = ry + 13 * 5 + 2;
            if (tap && px >= rx && px < rx + STEPS * 15 && py >= fy && py < fy + 21) {
                int st = (px - rx) / 15, row = (py - fy) / 7, b = 1 << st;
                if (row == 0) ln->oct ^= b; else if (row == 1) ln->acc ^= b; else ln->sld ^= b;
                mark_dirty();
            }
        }

        // expanded drum panel: trigger grid
        if (expanded == STRIP_DRUM && tap) {
            int y0 = strip_y(STRIP_DRUM) + HDR_H;
            int gx = 40, gy = y0 + 8;
            if (px >= gx && px < gx + STEPS * 15 && py >= gy && py < gy + NDRUM * 13) {
                int st = (px - gx) / 15, v = (py - gy) / 13;
                P->drum[v] ^= 1 << st;
                mark_dirty();
            }
        }
    }

    // deferred autosave (don't write the file on every painted cell)
    if (save_cooldown > 0 && --save_cooldown == 0) save_song();

#ifdef DE_TRACE
    watch("step", "%d", playhead);
    watch("bar",  "%d", barpos);
    watch("bank", "%d", playBank);
#endif

    // ── the clock — sixteenths off beat(), like tb303/drummachine ─────────
    if (!running) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int st  = s16 & 15;
    if (s16 != last16) {
        bool newbar = (st == 0 && last16 >= 0) || last16 < 0;
        last16 = s16;
        playhead = st;
        if (newbar) {                              // pick the bar's bank
            if (songmode && chainN > 0) {
                playBank = chain[barpos % chainN];
                barpos = (barpos + 1) % (chainN > 0 ? chainN : 1);
            } else playBank = editBank;
        }
        Pattern *P = &bank[playBank];
        if (!mute[STRIP_A]) step_303(&m[0], &P->ln[0], st); else off_303(&m[0]);
        if (!mute[STRIP_B]) step_303(&m[1], &P->ln[1], st); else off_303(&m[1]);
        if (!mute[STRIP_DRUM])
            for (int d = 0; d < NDRUM; d++)
                if (P->drum[d] & (1 << st)) fire_drum(d);
    } else {
        Pattern *P = &bank[playBank];
        gate_303(&m[0], &P->ln[0], st);
        gate_303(&m[1], &P->ln[1], st);
    }
}

// ── draw ──────────────────────────────────────────────────────────────────
static const int BANKCLR[NBANK] = { CLR_ORANGE, CLR_GREEN, CLR_BLUE, CLR_RED };

static void draw_header(int i, int y) {
    bool open = (i == expanded);
    rectfill(2, y, 316, HDR_H - 2, open ? CLR_DARKER_GREY : CLR_DARK_GREY);
    print(open ? "v" : ">", 8, y + 6, CLR_LIGHT_GREY);
    print(SNAME[i], 18, y + 6, mute[i] ? CLR_MEDIUM_GREY : CLR_WHITE);
    // live mini pattern — the "folded isn't blind" ticks
    Pattern *P = &bank[playBank];
    for (int s = 0; s < STEPS; s++) {
        int tx = 96 + s * 6;
        bool onn = false;
        if (i == STRIP_DRUM) { for (int d = 0; d < NDRUM; d++) if (P->drum[d] & (1 << s)) { onn = true; break; } }
        else onn = (P->ln[i].on >> s) & 1;
        int c = onn ? (mute[i] ? CLR_MEDIUM_GREY : CLR_ORANGE) : CLR_DARKER_GREY;
        if (running && s == playhead) c = CLR_WHITE;
        rectfill(tx, y + 7, 4, 6, c);
    }
    // activity LED
    bool lit = false;
    if (i == STRIP_DRUM) { for (int d = 0; d < NDRUM; d++) if (drumflash[d] > 0) lit = true; }
    else lit = (m[i].h >= 0);
    circfill(206, y + 10, 2, lit && !mute[i] ? CLR_GREEN : CLR_DARKER_GREY);
    // MUTE toggle (ui.h button — per-finger capture, focus ring)
    if (ui_button(252, y + 2, 34, HDR_H - 5, mute[i] ? "MUTED" : "mute")) { mute[i] = !mute[i]; mark_dirty(); }
}

static void draw_303_panel(int i, int y0) {
    M303 *s = &m[i];
    Line *ln = &bank[editBank].ln[i];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);

    // knob row
    for (int k = 0; k < NK; k++)
        if (ui_knob(&s->knob[k], 26 + k * 38, y0 + 12, KNAME[k])) { knob_changed_303(s, k); mark_dirty(); }
    if (ui_button(288, y0 + 4, 28, 16, s->wave == INSTR_SAW ? "SAW" : "SQR")) {
        s->wave = (s->wave == INSTR_SAW) ? INSTR_SQUARE : INSTR_SAW;
        define_303(s); mark_dirty();
    }

    // piano roll: 13 rows (root..octave), playhead column, root rows tinted
    int rx = 40, ry = y0 + 30;
    for (int st = 0; st < STEPS; st++) {
        int cx = rx + st * 15;
        int bg = (running && st == playhead) ? CLR_DARKER_GREY : CLR_BLACK;
        rectfill(cx, ry, 14, 13 * 5 - 1, bg);
        for (int r = 0; r <= 12; r += 12) {  // root + octave guide rows
            rectfill(cx, ry + (12 - r) * 5, 14, 4, bg == CLR_BLACK ? CLR_DARKER_GREY : CLR_DARK_GREY);
        }
        if ((ln->on >> st) & 1) {
            int row = ln->pitch[st];
            int c = ((ln->acc >> st) & 1) ? CLR_RED : BANKCLR[editBank];
            rectfill(cx, ry + (12 - row) * 5, 14, 4, c);
            if ((ln->sld >> st) & 1) rectfill(cx + 12, ry + (12 - row) * 5 + 1, 4, 2, CLR_WHITE);
        }
    }
    font(FONT_SMALL);
    print("C3", 2, ry + 4, CLR_MEDIUM_GREY);
    print("C2", 2, ry + 12 * 5 - 5, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // flag rows
    static const char *FLAG[3] = { "OCT", "ACC", "SLD" };
    int fy = ry + 13 * 5 + 2;
    font(FONT_SMALL);
    for (int r = 0; r < 3; r++) {
        print(FLAG[r], 14, fy + r * 7, CLR_MEDIUM_GREY);
        unsigned short mask = r == 0 ? ln->oct : r == 1 ? ln->acc : ln->sld;
        for (int st = 0; st < STEPS; st++)
            rectfill(rx + st * 15, fy + r * 7, 14, 6,
                     (mask >> st) & 1 ? (r == 1 ? CLR_RED : CLR_YELLOW) : CLR_DARKER_GREY);
    }
    font(FONT_NORMAL);
}

static void draw_drum_panel(int y0) {
    Pattern *P = &bank[editBank];
    rectfill(2, y0, 316, PANEL_H - 2, CLR_BLACK);
    int gx = 40, gy = y0 + 8;
    for (int v = 0; v < NDRUM; v++) {
        print(DNAME[v], 16, gy + v * 13 + 3, drumflash[v] > 0 ? CLR_WHITE : CLR_MEDIUM_GREY);
        for (int st = 0; st < STEPS; st++) {
            int cx = gx + st * 15;
            bool onn = (P->drum[v] >> st) & 1;
            int c = onn ? BANKCLR[editBank] : ((st & 3) == 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
            if (running && st == playhead && onn) c = CLR_WHITE;
            rectfill(cx, gy + v * 13, 14, 12, c);
        }
    }
    print("full 808 + 909 panels arrive in increment 2", 40, gy + NDRUM * 13 + 10, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_DARKER_GREY);
    ui_begin();

    // ── transport bar ─────────────────────────────────────────────────────
    rectfill(0, 0, 320, TB_H, CLR_BLACK);
    if (ui_button(4, 2, 36, 18, running ? "STOP" : "RUN")) {
        running = !running; last16 = -1; if (!running) stop_all();
    }
    char buf[24];
    snprintf(buf, sizeof buf, "%d", tempo);
    print("BPM", 46, 8, CLR_MEDIUM_GREY);
    print(buf, 70, 8, CLR_WHITE);
    if (ui_button(94, 2, 12, 18, "-")) { tempo -= 2; if (tempo < 60) tempo = 60; bpm(tempo); mark_dirty(); }
    if (ui_button(108, 2, 12, 18, "+")) { tempo += 2; if (tempo > 200) tempo = 200; bpm(tempo); mark_dirty(); }
    for (int bk = 0; bk < NBANK; bk++) {
        char nm[2] = { (char)('A' + bk), 0 };
        int x = 136 + bk * 20;
        if (ui_button(x, 2, 17, 18, nm)) editBank = bk;
        if (bk == editBank) rect(x, 2, 17, 18, BANKCLR[bk]);
        if (running && bk == playBank) rectfill(x + 6, 17, 5, 2, CLR_WHITE);
    }
    if (ui_button(226, 2, 42, 18, songmode ? "SONG" : "LOOP")) { songmode = !songmode; barpos = 0; mark_dirty(); }
    font(FONT_SMALL);
    print("acid rack", 272, 9, CLR_INDIGO);
    font(FONT_NORMAL);

    // ── strips ────────────────────────────────────────────────────────────
    for (int i = 0; i < NSTRIP; i++) {
        int y = strip_y(i);
        draw_header(i, y);
        if (i == expanded) {
            if (i == STRIP_DRUM) draw_drum_panel(y + HDR_H);
            else draw_303_panel(i, y + HDR_H);
        }
    }

    // ── song chain row ────────────────────────────────────────────────────
    print("SONG", 4, CHAIN_Y + 3, songmode ? CLR_WHITE : CLR_MEDIUM_GREY);
    for (int c = 0; c < CHAINN; c++) {
        int x = 34 + c * 4;
        int bk = chain[c];
        int col = (bk == 0xFF) ? CLR_DARK_GREY : BANKCLR[bk];
        rectfill(x, CHAIN_Y + 2, 3, 10, col);
        if (songmode && running && chainN > 0 && c == (barpos + chainN - 1) % chainN)
            rectfill(x, CHAIN_Y, 3, 2, CLR_WHITE);
    }

    for (int d = 0; d < NDRUM; d++) if (drumflash[d] > 0) drumflash[d]--;
    ui_end();
}
