/* de:meta
{
  "slug": "bossabloom",
  "title": "bossa bloom",
  "status": "active",
  "created": "2026-07-17",
  "collection": ["radio", "device-face"],
  "kind": [
    "instrument",
    "toy"
  ],
  "teaches": [
    "chord-voicing",
    "generative-melody",
    "schedule-driven-agents"
  ],
  "lineage": "The first CHORD-BLOOM rack (docs/design/bossa-rack.md): the bossa radio turned inside-out into an instrument — you PLAY the chord chart, the band (bass/guitar-voicing/melody, all copied verbatim from bossa.c) blooms it live, and the chord-locked solo strip is the jam. The candy device-face skin from bossaface.c.",
  "description": {
    "summary": "Play the changes, the band follows. A bossa chord-bloom rack on the candy device-face: tap a bar to select it, tap a chord from the diatonic FUNCTION palette (tonic=gold, subdominant=green, dominant=orange) to place it — the guitar re-voices, the bass re-walks, and the melody re-pitches around your chord, live and in idiom. GEN rolls a fresh idiomatic AABA; jam the flute over your own changes on the chord-locked solo strip.",
    "controls": "SPACE play/stop · G generate a fresh chart · tap a chart bar to select+audition · tap a palette chord to place it (auditions) · tap VOICE/FEEL to cycle · UP/DOWN tempo · tap the tabs (or 1-4) · drag the solo strip to jam.",
    "detail": "The bossa band engine (jazz-function Markov harmony, rootless 3-voice voice-leading via rad_lead_to, the surdo bass with chromatic approach, the One-Note-Samba melody cell re-pitched per chord) is copied verbatim from bossa.c so it can't drift; the rack only makes prog[16] editable and wraps it in the candy device-face. Functions-only palette (extensible via the CHORDVOX table). MEL/MIX/SONG faces are stubs for now (build-order items 4-5)."
  }
}
de:meta */
#include "studio.h"
#include "radio.h"   // the shared station chassis (PRNG, clock, voice-leading)
#include "solo.h"    // the jam layer — the chord-locked solo strip
#include "harmony.h" // the shared harmony brain — vocab + the HB_BOSSA walk
#include "ui.h"      // contacts for the solo strip
#include <stdio.h>
#include <string.h>

// ── BOSSA BLOOM — the chord-bloom rack ──────────────────────────────────────
// bossa.c turned inside-out: you PLAY the chord chart, the band blooms it. The
// whole band engine below (harmony, voice-leading, bass, melody) is copied from
// bossa.c verbatim — only prog[16] became editable and the face became candy.

// ── instrument slots (same as bossa.c) ──────────────────────────────────────
#define I_GTR    5
#define I_BASS   6
#define I_FLUTE  7
#define I_SHAKER 8
#define I_RIM    9
#define I_SOLO   10

// ── chord qualities — the shared harmony brain (harmony.h). bossabloom was a
// verbatim bossa fork; aliases keep the body unchanged. QVOICE (guitar voicing)
// stays local; the chord TONES are hb_tones now, the walk is HB_BOSSA.
enum { Q_MAJ7 = HBQ_MAJ7, Q_MIN7 = HBQ_MIN7, Q_DOM7 = HBQ_DOM7,
       Q_M7B5 = HBQ_M7B5, Q_MIN6 = HBQ_MIN6, NQUAL = HB_NQUAL };
#define QNAME hb_qname
static const int QVOICE[NQUAL][3] = {
    { 4, 11, 14 }, { 3, 10, 14 }, { 4, 10, 14 }, { 3, 6, 10 }, { 3, 9, 14 },
};
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// ── harmonic functions — harmony.h's frozen 13-function vocab (HB_*). F_OFF/
// F_QUAL alias hb_off/hb_qual; F_LABEL/F_FAM below stay local (palette colour).
enum { F_I = HB_I, F_ii = HB_ii, F_iii = HB_iii, F_IV = HB_IV, F_V = HB_V,
       F_vi = HB_vi, F_II7 = HB_II7, F_VI7 = HB_VI7, F_bII7 = HB_bII7,
       F_iv = HB_iv, F_bVII7 = HB_bVII7, F_v = HB_v, F_I7 = HB_I7, NFUNC = HB_NFUNC };
#define F_OFF  hb_off
#define F_QUAL hb_qual
// the palette label + a FUNCTION FAMILY for colour (0 tonic · 1 subdominant · 2 dominant)
static const char *F_LABEL[NFUNC] = { "I","ii","iii","IV","V","vi","II7","VI7","bII7","iv","b7","v","I7" };
static const int   F_FAM[NFUNC]   = {  0,  1,   0,   1,  2,  0,   2,    2,    2,     1,   2,   1,  2 };

// the weighted walk lives in harmony.h as the HB_BOSSA style — these rows WERE
// it, byte-for-byte (markov_next reads it, so the srnd sequence is unchanged).

// ── comping (bossa.c) ───────────────────────────────────────────────────────
static const int COMPS[3][6] = {
    { 0, 6, 12, 20, 26, -1 }, { 0, 6, 12, 20, 26, 30 }, { 4, 10, 16, 22, 28, -1 },
};
static const int CLAVE[2][5] = { { 0, 6, 12, 20, 26 }, { 4, 10, 16, 22, 28 } };

// ── the (now editable) song ─────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  prog[16];          // THE CHART — function per bar; [0..7] A, [8..15] B. EDITABLE now.
    int  comp, claveSwap;
    int  cellOn[6], cellN;
    char title[24];
    unsigned seed;
} Song;

static Song       sng;
static RadioSeed  rs;
static RadioClock clk = { -1, 0, 119.0 };
#define stepMs    (clk.stepMs)
#define songBase  (clk.songBase)
#define scheduled (clk.scheduled)
#define srnd(n)    rad_srnd(&rs, (n))
#define schance(p) (srnd(100) < (p))

static int    tempo     = 126;
static int    intensity = 2;         // FEEL: 0 midnight · 1 cafe · 2 classic · 3 festival
static bool   playing   = true;
static int    gv[3]     = { 64, 67, 71 };
static bool   gvInit    = false;
static int    melPitch  = 79;
static bool   melOn     = true;
static int    face      = 0;         // 0 CHORD · 1 MEL · 2 MIX · 3 SONG
static int    sel       = -1;        // selected chart bar (-1 = none → palette hidden)
static int    voicing   = 1;         // 0..2 voicing spread (register window)
static float  vu        = 0;

static int iabs(int v) { return v < 0 ? -v : v; }

// ── song generation (bossa.c) ───────────────────────────────────────────────
static int markov_next(int f) { return hb_pick(&HB_BOSSA, f, srnd(hb_nopts(&HB_BOSSA, f))); }
static void gen_section(int *bars, int startFunc) {
    bars[0] = startFunc;
    for (int i = 1; i < 6; i++) bars[i] = markov_next(bars[i - 1]);
    bars[6] = F_ii;
    bars[7] = schance(25) ? F_bII7 : F_V;
}
static const char *TW1[] = { "Onda","Praia","Lua","Saudade","Garota","Brisa","Chuva",
    "Tarde","Janela","Estrela","Areia","Flor","Sombra","Manha","Ipanema","Coqueiro" };
static const char *TW2[] = { "de Verao","do Mar","da Manha","do Rio","de Marco",
    "da Lua","do Vento","de Abril","da Praia","do Sol" };

static void new_song(double pos, unsigned seed) {
    sng.seed = rad_seed_begin(&rs, seed);
    sng.keyPc = srnd(12);
    gen_section(sng.prog,     F_I);
    gen_section(sng.prog + 8, schance(60) ? F_IV : F_vi);
    sng.comp      = srnd(3);
    sng.claveSwap = (sng.comp == 2) ? 0 : schance(50);
    sng.cellN = 0;
    for (int s = 0; s < 31 && sng.cellN < 6; s += 2) {
        int p = (s % 8 == 0) ? 22 : (s % 4 == 2 ? 42 : 30);
        if (schance(p)) sng.cellOn[sng.cellN++] = s;
    }
    if (sng.cellN < 3) {
        sng.cellN = 4;
        sng.cellOn[0] = 2; sng.cellOn[1] = 8; sng.cellOn[2] = 14; sng.cellOn[3] = 22;
    }
    if (schance(20)) snprintf(sng.title, sizeof sng.title, "Minha %s", TW1[srnd(16)]);
    else             snprintf(sng.title, sizeof sng.title, "%s %s", TW1[srnd(16)], TW2[srnd(10)]);
    tempo = 112 + srnd(28);
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    melPitch = 79;
}

// ── harmony helpers (bossa.c) ───────────────────────────────────────────────
static int bar_to_prog(long bar32) {   // AABA
    long b = bar32 % 32;
    if (b < 16) return (int)(b % 8);
    if (b < 24) return (int)(8 + b - 16);
    return (int)(b - 24);
}
static int func_at(long s) {
    long bar = s / 16;
    if (s % 16 >= 14) bar++;
    return sng.prog[bar_to_prog(bar)];
}
static int root_pc(int f) { return (sng.keyPc + F_OFF[f]) % 12; }
static void lead_voices(int f) {
    int lo = 56 + voicing * 2, hi = 80 + voicing * 3;   // voicing = register spread
    rad_lead_to(root_pc(f), QVOICE[F_QUAL[f]], gv, 3, lo, hi, &gvInit);
}
static int pick_mel(int f) {
    int q = F_QUAL[f], rp = root_pc(f);
    int bestM = melPitch, bestScore = -999;
    for (int t = 0; t < 5; t++) {
        int off = (t < 4) ? hb_tones[q][t] : 14;   // chord tone, then the 9th
        int pc = (rp + off) % 12;
        for (int oct = 6; oct <= 7; oct++) {
            int m = oct * 12 + pc;
            if (m < 72 || m > 91) continue;
            int score = 12 - iabs(m - melPitch) + rnd(5);
            if (m == melPitch) score -= 4;
            if (score > bestScore) { bestScore = score; bestM = m; }
        }
    }
    melPitch = bestM;
    return bestM;
}

// ── the step player (bossa.c — loops the AABA forever; reads the editable chart) ─
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly  = rad_step_dly(&clk, abs, pos);
    int step = (int)(s % 16);
    int s32  = (int)(s % 32);
    int f    = func_at(s);

    if (step == 0 || step == 8 || (step == 6 && chance(50)) || (step == 14 && chance(35))) {
        int b = 36 + root_pc(f);
        int n = b, vol = 5, dur = (int)(stepMs * 5);
        if (step == 6)  { n = b + 7 <= 50 ? b + 7 : b - 5; vol = 3; dur = (int)(stepMs * 1.6); }
        if (step == 8)  { n = b + 7 <= 50 ? b + 7 : b - 5; vol = 4; }
        if (step == 14) { n = 36 + root_pc(func_at(s + 2)) + (chance(50) ? -1 : 1); vol = 3; dur = (int)(stepMs * 1.6); }
        schedule_hit(dly, n, I_BASS, vol, dur);
        vu += vol * 0.8f;
    }
    for (int i = 0; i < 6; i++) {
        if (COMPS[sng.comp][i] != s32) continue;
        lead_voices(f);
        int gap = 32;
        for (int j = 0; j < 6; j++) {
            int o = COMPS[sng.comp][j];
            if (o < 0) continue;
            int g = (o - s32 + 32) % 32;
            if (g > 0 && g < gap) gap = g;
        }
        int dur = (int)(gap * stepMs * 0.8);
        if (dur > 360) dur = 360;
        int vol = (s32 == COMPS[sng.comp][0]) ? 4 : 3;
        for (int k = 0; k < 3; k++)
            schedule_hit(dly + k * 8 + rnd(4), gv[k], I_GTR, vol, dur);
        vu += vol;
    }
    if (intensity >= 1) {
        int vol = (step % 4 == 2) ? 3 : (step % 2 == 0 ? 2 : 1);
        if (intensity == 0) vol--;
        schedule_hit(dly + rnd(4), 84, I_SHAKER, vol, vol >= 3 ? 55 : 35);
    }
    if (intensity >= 2)
        for (int i = 0; i < 5; i++)
            if (CLAVE[sng.claveSwap][i] == s32) { schedule_hit(dly, 76, I_RIM, 3, 30); vu += 1.5f; }
    if (intensity >= 2 && !solo_open()) {
        if (s32 == 0) melOn = chance(intensity >= 3 ? 85 : 60);
        if (melOn)
            for (int i = 0; i < sng.cellN; i++)
                if (sng.cellOn[i] == s32) {
                    int gap = (i + 1 < sng.cellN) ? sng.cellOn[i + 1] - s32 : 32 - s32;
                    int dur = (int)(gap * stepMs * 0.9);
                    if (dur > 900) dur = 900;
                    schedule_hit(dly + 10 + rnd(8), pick_mel(f), I_FLUTE, intensity >= 3 ? 4 : 3, dur);
                    vu += 2;
                }
    }
}

// audition a chord immediately (tap-to-audition) — comp its 3 voices now
static void audition(int f) {
    lead_voices(f);
    for (int k = 0; k < 3; k++) schedule_hit(k * 8, gv[k], I_GTR, 4, 300);
}

// ── setup (bossa.c) ─────────────────────────────────────────────────────────
static void setup_instruments(void) {
    instrument(I_GTR, INSTR_TRI, 1, 180, 1, 120);
    instrument_filter(I_GTR, FILTER_LOW, 2200, 3);
    instrument_env(I_GTR, 0, ENV_CUTOFF, 0, 90, 1400);
    instrument(I_BASS, INSTR_TRI, 2, 200, 4, 80);
    instrument_filter(I_BASS, FILTER_LOW, 700, 2);
    instrument(I_FLUTE, INSTR_SINE, 25, 120, 5, 140);
    instrument_lfo(I_FLUTE, 0, LFO_PITCH, 5.2f, 0.18f);
    instrument_filter(I_FLUTE, FILTER_LOW, 2600, 2);
    instrument(I_SHAKER, INSTR_NOISE, 1, 45, 0, 25);
    instrument_filter(I_SHAKER, FILTER_HIGH, 5200, 4);
    instrument(I_RIM, INSTR_NOISE, 0, 28, 0, 18);
    instrument_filter(I_RIM, FILTER_BAND, 1800, 9);
    instrument_env(I_RIM, 0, ENV_PITCH, 0, 20, 18);
    instrument(I_SOLO, INSTR_SINE, 20, 140, 6, 200);
    instrument_lfo(I_SOLO, 0, LFO_PITCH, 5.6f, 0.22f);
    instrument_filter(I_SOLO, FILTER_LOW, 3600, 2);
}

// ── the candy device-face art (from bossaface.c) ────────────────────────────
static void mnote(int x, int y, int col) {
    circfill(x, y + 5, 1, col); rectfill(x + 1, y, 1, 6, col); line(x + 2, y, x + 4, y - 1, col);
}
static void bird(int cx, int cy, float t) {
    int bop = (int)(sin_deg(t * 250) * 2);
    cy += bop;
    trifill(cx + 6, cy - 2, cx + 6, cy + 3, cx + 13, cy + 1, CLR_DARK_BROWN);
    ovalfill(cx, cy, 8, 6, CLR_ORANGE);
    ovalfill(cx + 1, cy + 2, 6, 4, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); ovalfill(cx + 1, cy - 1, 5, 3, CLR_DARK_BROWN); blend_reset();
    circfill(cx - 6, cy - 3, 4, CLR_DARK_ORANGE);
    trifill(cx - 10, cy - 4, cx - 10, cy - 2, cx - 14, cy - 3, CLR_YELLOW);
    int blink = ((int)(t * 1.4f)) % 4 == 0 && sin_deg(t * 300) > 0.6f;
    if (blink) line(cx - 7, cy - 4, cx - 4, cy - 4, CLR_BROWNISH_BLACK);
    else { pset(cx - 6, cy - 4, CLR_WHITE); pset(cx - 5, cy - 4, CLR_BROWNISH_BLACK); }
    mnote(cx + 12, cy - 11 - bop, CLR_LIGHT_YELLOW);
}
static void teak_knob(int cx, int cy, int r, float v, const char *lbl) {
    blend(BLEND_AVG); circfill(cx + 1, cy + 2, r, CLR_BLACK); blend_reset();
    circfill(cx, cy, r, CLR_BROWN);
    ring(cx, cy, r - 2, r, 165, 285, CLR_LIGHT_PEACH);
    ring(cx, cy, r - 2, r, -15, 105, CLR_DARK_BROWN);
    circ(cx, cy, r, CLR_BROWNISH_BLACK);
    float ang = 150 + v * 240;
    line(cx + (int)dx(r * 0.3f, ang), cy + (int)dy(r * 0.3f, ang),
         cx + (int)dx(r - 2, ang),    cy + (int)dy(r - 2, ang), CLR_WHITE);
    font(FONT_TINY); print(lbl, cx - text_width(lbl) / 2, cy + r + 2, CLR_DARK_BROWN);
}
// family colours: 0 tonic gold · 1 subdominant palm-green · 2 dominant sunset
static int fam_col(int fam) { return fam == 0 ? CLR_YELLOW : fam == 1 ? CLR_GREEN : CLR_ORANGE; }
static int fam_hi(int fam)  { return fam == 0 ? CLR_LIGHT_YELLOW : fam == 1 ? CLR_LIME_GREEN : CLR_LIGHT_PEACH; }

// a chord chip (palette) — label = chord NAME, coloured by function family
static void chip(int x, int y, int w, int h, int f, int on) {
    blend(BLEND_AVG); rrectfill(x + 1, y + 2, w, h, 3, CLR_BLACK); blend_reset();
    int dy = on ? 1 : 0, hh = on ? h - 1 : h;
    rrectfill(x, y + dy, w, hh, 3, fam_col(F_FAM[f]));
    line(x + 2, y + dy + 1, x + w - 3, y + dy + 1, fam_hi(F_FAM[f]));
    rrect(x, y + dy, w, hh, 3, CLR_BROWNISH_BLACK);
    font(FONT_TINY);
    print(F_LABEL[f], x + (w - text_width(F_LABEL[f])) / 2, y + dy + (hh - 5) / 2 + 1, CLR_BROWNISH_BLACK);
}

// ── update ──────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);
    if (!booted) { setup_instruments(); new_song(pos, 0); scheduled = (long)pos; booted = true; }

    // transport + shortcuts
    if (keyp(KEY_SPACE)) { playing = !playing; if (!playing) note_off_all(); else scheduled = (long)pos; }
    if (keyp('G'))     { new_song(pos, 0); sel = -1; }
    if (keyp(KEY_UP)   && tempo < 152) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 100) { tempo -= 2; bpm(tempo); }
    for (int i = 0; i < 4; i++) if (keyp('1' + i)) face = i;

    if (playing) {
        long st;
        while (rad_clock_step(&clk, pos, &st)) play_step(st, pos);
    }
    vu *= 0.86f; if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    watch("face", "%d", face);
    watch("sel",  "%d", sel);
    watch("bar",  "%ld", ss >= 0 ? (ss / 16) % 32 : -1);
    watch("key",  "%d", sng.keyPc);
#endif
}

// ── draw ──────────────────────────────────────────────────────────────────
static void nav_and_shell(long bar) {
    cls(CLR_BROWNISH_BLACK);
    rrectfill(0, 0, 160, 100, 8, CLR_DARK_BROWN);
    rrectfill(1, 1, 157, 97, 8, CLR_BROWN);
    rrectfill(5, 5, 150, 90, 6, CLR_LIGHT_PEACH);
    blend(BLEND_AVG); line(9, 6, 150, 6, CLR_WHITE); blend_reset();
    // play/stop
    if (playing) { rectfill(9, 8, 2, 7, CLR_DARK_BROWN); rectfill(13, 8, 2, 7, CLR_DARK_BROWN); }
    else trifill(9, 8, 9, 15, 15, 12, CLR_DARK_BROWN);
    // tabs
    font(FONT_TINY);
    static const char *TAB[4] = { "CHORD", "MEL", "MIX", "SONG" };
    int tx = 20;
    for (int i = 0; i < 4; i++) {
        int w = text_width(TAB[i]) + 4;
        if (i == face) { rrectfill(tx, 8, w, 8, 2, CLR_ORANGE); print(TAB[i], tx + 2, 10, CLR_BROWNISH_BLACK); }
        else print(TAB[i], tx + 2, 10, CLR_DARK_BROWN);
        tx += w + 2;
    }
    char hdr[24]; snprintf(hdr, 24, "key %s  %d", PCNAME[sng.keyPc], tempo);
    print(hdr, 112, 10, CLR_DARK_BROWN);
}

static void face_chord(long bar, float t) {
    // LCD — the playable chart + current chord + the pet bird
    rrectfill(7, 19, 98, 44, 4, CLR_BROWNISH_BLACK);
    rrectfill(9, 21, 94, 40, 3, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 23; y < 61; y += 2) line(9, y, 102, y, CLR_BROWNISH_BLACK); blend_reset();

    int curF = sng.prog[bar_to_prog(bar)];
    char cn[12]; snprintf(cn, 12, "%s%s", PCNAME[root_pc(curF)], QNAME[F_QUAL[curF]]);
    font(FONT_SMALL); print(sng.title, 12, 23, CLR_MEDIUM_GREEN);
    print(cn, 12, 31, CLR_LIME_GREEN);
    bird(88, 32, t);   // corner pet

    // the 16-bar chart — two rows of 8 (A then B); playhead + selection
    int ph = (int)(bar % 32); ph = bar_to_prog(bar);   // which prog index plays now
    for (int i = 0; i < 16; i++) {
        int sx = 11 + (i % 8) * 11, sy = 43 + (i / 8) * 9;
        int on = (i == sel), play = (i == ph);
        int bg = on ? CLR_ORANGE : play ? CLR_LIME_GREEN : CLR_DARK_GREEN;
        rrectfill(sx, sy, 10, 8, 2, bg);
        rrect(sx, sy, 10, 8, 2, CLR_MEDIUM_GREEN);
        font(FONT_TINY);
        const char *lb = F_LABEL[sng.prog[i]];
        int tc = (on || play) ? CLR_BROWNISH_BLACK : CLR_MEDIUM_GREEN;
        print(lb, sx + (10 - text_width(lb)) / 2, sy + 2, tc);
    }

    // knobs
    teak_knob(120, 30, 9, voicing / 2.0f, "VOICE");
    teak_knob(143, 30, 9, intensity / 3.0f, "FEEL");
    static const char *FEEL[4] = { "midnite", "cafe", "classic", "festiv" };
    font(FONT_TINY); print(FEEL[intensity], 131, 47, CLR_DARK_BROWN);

    // zone 4: the palette (only when a bar is selected) — else a hint
    if (sel >= 0) {
        for (int i = 0; i < NFUNC; i++) {
            int col = i % 7, row = i / 7;
            chip(8 + col * 21, 66 + row * 11, 19, 10, i, sng.prog[sel] == i);
        }
    } else {
        font(FONT_TINY); print("tap a bar to set its chord", 12, 70, CLR_DARK_BROWN);
    }

    // zone 5: the chord-locked solo strip (only when not editing, to free room)
    if (sel < 0) {
        int f = sng.prog[bar_to_prog(bar)];
        int chord[4]; hb_chord_pcs(sng.keyPc, f, chord);   // the bar's chord tones
        static const int PENT[5] = { 0, 2, 4, 7, 9 };
        SoloCtx jc = { sng.keyPc, PENT, 5, chord, 4, I_SOLO, 72, 91, false, SOLO_Y_VOL, 2, 6, false, true };
        solo_strip(&jc, 10, 82, 128, 11, CLR_ORANGE);
        font(FONT_TINY); print("SOLO", 140, 85, CLR_DARK_BROWN);
    }
}

static void face_stub(const char *name) {
    rrectfill(7, 19, 146, 74, 4, CLR_BROWNISH_BLACK);
    rrectfill(9, 21, 142, 70, 3, CLR_DARK_GREEN);
    blend(BLEND_AVG); for (int y = 23; y < 91; y += 2) line(9, y, 150, y, CLR_BROWNISH_BLACK); blend_reset();
    font(FONT_SMALL);
    print(name, 80 - text_width(name) / 2, 48, CLR_LIME_GREEN);
    print("soon", 80 - text_width("soon") / 2, 58, CLR_MEDIUM_GREEN);
}

// input hit-testing (runs in draw so it sees the same rects)
static void handle_taps(long bar, double pos) {
    // tabs
    font(FONT_TINY);
    static const char *TAB[4] = { "CHORD", "MEL", "MIX", "SONG" };
    int tx = 20;
    for (int i = 0; i < 4; i++) { int w = text_width(TAB[i]) + 4; if (tapp(tx, 8, w, 8)) face = i; tx += w + 2; }
    if (tapp(7, 7, 12, 9)) { playing = !playing; if (!playing) note_off_all(); else scheduled = (long)pos; }

    if (face != 0) return;
    // chart bars
    for (int i = 0; i < 16; i++) {
        int sx = 11 + (i % 8) * 11, sy = 43 + (i / 8) * 9;
        if (tapp(sx, sy, 10, 8)) { sel = (sel == i) ? -1 : i; if (sel >= 0) audition(sng.prog[sel]); }
    }
    // palette
    if (sel >= 0)
        for (int i = 0; i < NFUNC; i++) {
            int col = i % 7, row = i / 7;
            if (tapp(8 + col * 21, 66 + row * 11, 19, 10)) { sng.prog[sel] = i; audition(i); }
        }
    // knobs (tap to cycle for now — drag is a later refinement)
    if (tapp(111, 21, 18, 18)) voicing   = (voicing + 1) % 3;
    if (tapp(134, 21, 18, 18)) intensity = (intensity + 1) % 4;
}

void draw(void) {
    float t = now();
    ui_begin();
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? (ss / 16) % 32 : 0;

    nav_and_shell(bar);
    if (face == 0) face_chord(bar, t);
    else if (face == 1) face_stub("MELODY");
    else if (face == 2) face_stub("MIX");
    else face_stub("SONG");

    handle_taps(bar, (double)beat() * 4.0 + beat_pos() * 4.0);
    ui_end();
    font(FONT_NORMAL);
}
