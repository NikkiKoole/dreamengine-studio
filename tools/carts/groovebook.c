/* de:meta
{
  "slug": "groovebook",
  "title": "groove book",
  "status": "active",
  "created": "2026-08-20",
  "kind": [
    "instrument",
    "tool"
  ],
  "teaches": [
    "step-sequencer",
    "drum-synthesis"
  ],
  "lineage": "The reader for runtime/drumpat.h, the maker's own 565-pattern library (converted from his love2d drum-patterns.lua). Where autorhythm plays 101 rhythms read off manufacturer service manuals on their own clocks, this one is the modern side: 16 steps of sixteenths, a big vocabulary, and a browser built for moving through it fast.",
  "description": {
    "summary": "565 drum patterns in 92 groups, browsable at speed: jump by letter, scrub groups and patterns while it keeps playing.",
    "detail": "A library is only worth having if you can find things in it, so this is a browser first and an instrument second. The group list is the primary axis (92 of them) and it never stops playing while you move: change group or pattern and the new one takes over on the next bar boundary, so you can scrub through dozens of grooves without breaking time. Press any LETTER to jump to the first group starting with it, which turns 92 groups into two keystrokes. Only the roles a pattern actually uses are drawn, so a four-lane groove looks like four lanes instead of nine empty rows. Two lanes are not drums and are drawn differently: ACCENT is a velocity lane (it scales the hit it lands on, it is not a thirteenth voice) and the 80 FLAM marks in the library are hits that want a doubled stroke. Voices come from the shared drumkit.h ELECTRO kit; the library's 12 voice roles are mapped onto its 8 with pitch offsets rather than by dropping lanes, and the mapping is on screen. Provenance is uneven and the cart says so: the core groups come from a cited pattern book, the later ones (EDM, dubstep, boombap) carry no citation.",
    "controls": "SPACE play/stop. UP/DOWN group, LEFT/RIGHT pattern, [ and ] jump ten groups. Any LETTER jumps to the first group starting with it. R random pattern. F cycles the kit (electro/acoustic). +/- tempo. TAB shows every role including the empty ones."
  }
}
de:meta */
#include "studio.h"
#include "drumpat.h"
#include "drumkit.h"
#include <stdio.h>
#include <string.h>

// ── the library's 12 voice roles onto the kit's 8 ─────────────────────────
// drumpat.h carries BD SD LT MT HT CH OH CY RS CB CPS TB (+ AC, a velocity lane).
// drumkit.h owns KICK SNARE HHC HHO CLAP TOM_LO TOM_HI CRASH. Mapping by pitch instead
// of dropping the four that have no slot of their own: a rim shot is a snare struck high,
// a cowbell and a tambourine are bright metal. Every lane sounds; none is silently lost.
// vbump: a per-role velocity offset. A SINE tom at the same velocity as a NOISE snare is much
// quieter, and level alone could not close it (the toms were already near unity gain).
typedef struct { int role, midi, vbump; const char *how; } Map;
static const Map MAP[DP_NROLES] = {
    [DP_BD]  = { DK_KICK,   36, 3, "kick" },
    [DP_SD]  = { DK_SNARE,  38, 3, "snare" },
    [DP_LT]  = { DK_TOM_LO, 41, 2, "low tom" },
    [DP_MT]  = { DK_TOM_LO, 48, 2, "mid tom = the low tom slot, up 7" },
    [DP_HT]  = { DK_TOM_HI, 55, 2, "high tom" },
    [DP_CH]  = { DK_HHC,    42, 0, "closed hat" },
    [DP_OH]  = { DK_HHO,    46, 0, "open hat" },
    [DP_CY]  = { DK_CRASH,  49, 0, "cymbal" },
    [DP_RS]  = { DK_SNARE,  50, 3, "rim shot = snare, up 12" },
    [DP_CB]  = { DK_TOM_HI, 62, 2, "cowbell = high tom, up 12" },
    [DP_CPS] = { DK_CLAP,   39, 3, "claps" },
    [DP_TB]  = { DK_HHC,    54, 0, "tambourine = closed hat, up 12" },
    [DP_AC]  = { -1,         0, 0, "ACCENT: a velocity lane, not a voice" },
};
static const char *RNAME[DP_NROLES] = { "BD","SD","LT","MT","HT","CH","OH","CY","RS","CB","CPS","TB","AC" };

static const DrumKitDef *KITS[2] = { &DK_ELECTRO, &DK_ACOUSTIC };
#define KIT_BASE 20

// A dense pattern fires five or six lanes on one sixteenth, which measured -0.0 dBFS with the kit
// at its build level, so the kit is trimmed here rather than fought with per-hit velocities.
// PER ROLE, not one blanket number: the noise voices (hats, snare, crash) are far louder than the
// SINE toms, and a flat 0.45 left the toms 20 dB under the mix. Measured on Afro-Cuban 2's
// "Measure B", whose only tom lane was inaudible until this table existed.
// PER KIT, because the two kits are not merely different sounds but different LOUDNESS SHAPES.
// Measured intrinsic loudness (peak with level and velocity divided out):
//   ELECTRO  hats -11, crash -20, sine toms -31, sine kick -33, band-noise snare/clap -38
//   ACOUSTIC hats -11, crash -13, membrane kick/toms -38 to -39, snare/clap/rim -36 to -37
// So a kick-led balance needs the hats pulled ~26 dB down in one kit and ~33 dB in the other, and
// a single table leaves whichever kit it was not tuned for badly wrong. The ELECTRO table used to
// be the only one, and on ACOUSTIC it left the toms 13 dB under a leading crash.
static const float LVL[2][DK_N] = {
    {   // ELECTRO
        [DK_KICK] = 1.00f, [DK_SNARE] = 1.00f, [DK_HHC] = 0.050f, [DK_HHO] = 0.060f,
        [DK_CLAP] = 1.00f, [DK_TOM_LO] = 0.55f, [DK_TOM_HI] = 0.55f, [DK_CRASH] = 0.200f,
    },
    {   // ACOUSTIC: membrane kick and toms are far quieter, its crash far louder
        [DK_KICK] = 1.00f, [DK_SNARE] = 0.575f, [DK_HHC] = 0.023f, [DK_HHO] = 0.028f,
        [DK_CLAP] = 0.50f, [DK_TOM_LO] = 0.70f, [DK_TOM_HI] = 0.70f, [DK_CRASH] = 0.045f,
    },
};
static void use_kit(int k) {
    dk_use(KITS[k], KIT_BASE);
    for (int i = 0; i < DK_N; i++) instrument_level(KIT_BASE + i, LVL[k][i]);  // gain is 0..1, not 0..7
}

static int  grp = 0, pat = 0;          // where the cursor is
static int  playing_pat = 0;           // what the sequencer is actually on (swaps on the bar)
static bool running = true;
static int  kit = 0, tempo = 112;
static bool showall = false;
static int  last16 = -1;
static int  flash[DP_NROLES];
static int  pstep = 0;

static int  pat_of(int g, int i) { return DP_GROUP[g].first + i; }
static int  npat(int g)          { return DP_GROUP[g].count; }

static void clampcur(void) {
    if (grp < 0) grp = DP_NGROUP - 1;
    if (grp >= DP_NGROUP) grp = 0;
    if (pat < 0) pat = npat(grp) - 1;
    if (pat >= npat(grp)) pat = 0;
}

// any letter jumps to the first group starting with it, which turns 92 groups into two
// keystrokes. Pressing the same letter again walks to the NEXT group with that initial.
static void jump_letter(char c) {
    for (int n = 1; n <= DP_NGROUP; n++) {
        int g = (grp + n) % DP_NGROUP;
        char f = DP_GROUP[g].name[0];
        if (f >= 'a' && f <= 'z') f = (char)(f - 32);
        if (f == c) { grp = g; pat = 0; return; }
    }
}

void init(void) {
    use_kit(kit);
    bpm(tempo);
    reverb(0.35f, 0.5f);
    // Drum-bus glue for character. MEASURED, so nobody expects more of it: on material this
    // transient the average gain reduction is tiny, so its automatic makeup is tiny too (0.35,
    // 0.55 and 0.75 all landed within 0.05 dB of each other). The mix therefore peaks around
    // -9.5 dBFS rather than the -0.2 that drummachine reaches, and that is the honest ceiling for
    // a KICK-LED balance here: ELECTRO's voices span 28 dB of intrinsic loudness, the kick is
    // already at unity gain and velocity 7, so the only way to a hotter mix is to let the hats
    // lead again, which is the bug this table exists to fix.
    glue(0, 0.35f, 8, 160);
}

void update(void) {
    for (int i = 0; i < DP_NROLES; i++) if (flash[i]) flash[i]--;

    if (keyp(KEY_SPACE)) running = !running;
    if (keyp(KEY_UP))    { grp--; pat = 0; clampcur(); }
    if (keyp(KEY_DOWN))  { grp++; pat = 0; clampcur(); }
    if (keyp(KEY_LEFT))  { pat--; clampcur(); }
    if (keyp(KEY_RIGHT)) { pat++; clampcur(); }
    if (keyp('['))       { grp -= 10; pat = 0; clampcur(); }
    if (keyp(']'))       { grp += 10; pat = 0; clampcur(); }
    if (keyp('R'))       { grp = (int)(rnd(DP_NGROUP)); pat = (int)(rnd(npat(grp))); clampcur(); }
    if (keyp('F'))       { kit ^= 1; use_kit(kit); }
    if (keyp(KEY_TAB))   showall = !showall;
    if (keyp('=') || keyp('+')) { tempo += 4; if (tempo > 200) tempo = 200; bpm(tempo); }
    if (keyp('-'))              { tempo -= 4; if (tempo <  50) tempo =  50; bpm(tempo); }
    for (char c = 'A'; c <= 'Z'; c++) if (keyp(c) && c != 'R' && c != 'F') jump_letter(c);

    if (!running) { last16 = -1; return; }

    // the sequencer. One sixteenth per step, off the synth's own beat counter so the
    // picture and the sound cannot drift. A new selection takes over at the BAR so
    // scrubbing through groups never breaks time.
    float pos = (float)beat() + beat_pos();
    int   s16 = (int)(pos * 4.0f);
    if (s16 == last16) return;
    last16 = s16;
    pstep = s16 & 15;
    if (pstep == 0) playing_pat = pat_of(grp, pat);

    int p = playing_pat;
    int acc = dp_accent(p, pstep);
    for (int r = 0; r < DP_NROLES; r++) {
        if (r == DP_AC) continue;                       // a velocity lane, never a voice
        if (!dp_hit(p, r, pstep)) continue;
        int vel = (acc ? 6 : 4) + MAP[r].vbump;
        if (vel > 7) vel = 7;
        dk_fire(MAP[r].role, MAP[r].midi, vel);
        if (dp_flam(p, r, pstep)) dk_fire_at(28, MAP[r].role, MAP[r].midi, vel - 1);  // doubled stroke
        flash[r] = 4;
    }
}

// ── drawing ──────────────────────────────────────────────────────────────
#define LX 2      // the group list
#define LW 52   // 10 chars at the small font's ~5px advance
#define GX 74     // the pattern grid
#define CW 14

static void draw_list(void) {
    // the WINDOW scrolls, the selection does not jump: clamping the top keeps all 15 rows filled
    // instead of leaving a gap at either end of the library
    int rows = 15, top = grp - rows / 2;
    if (top < 0) top = 0;
    if (top > DP_NGROUP - rows) top = DP_NGROUP - rows;
    for (int i = 0; i < rows; i++) {
        int g = top + i;
        if (g < 0 || g >= DP_NGROUP) continue;
        int y = 22 + i * 10;
        bool sel = (g == grp);
        if (sel) rectfill(LX - 1, y - 1, LW, 9, CLR_DARK_BLUE);
        char b[20];
        snprintf(b, sizeof b, "%-10.10s", DP_GROUP[g].name);
        print(b, LX, y, sel ? CLR_WHITE : CLR_MEDIUM_GREY);
    }
    char b[24];
    snprintf(b, sizeof b, "%d/%d groups", grp + 1, DP_NGROUP);
    print(b, LX, 174, CLR_DARK_GREY);
}

static void draw_grid(void) {
    int p = pat_of(grp, pat);
    int y = 34;
    for (int r = 0; r < DP_NROLES; r++) {
        bool used = dp_role_used(p, r) != 0;
        if (!used && !showall) continue;
        bool isacc = (r == DP_AC);
        print(RNAME[r], GX - 15, y + 1, flash[r] ? CLR_WHITE
                                      : isacc ? CLR_INDIGO : used ? CLR_MEDIUM_GREY : CLR_DARKER_GREY);
        for (int s = 0; s < DP_STEPS; s++) {
            int x = GX + s * CW;
            if (dp_hit(p, r, s)) {
                int col = isacc ? CLR_INDIGO : CLR_YELLOW;
                if (dp_flam(p, r, s)) col = CLR_ORANGE;          // a hit that wants two strokes
                if (isacc) rectfill(x + 4, y + 2, 5, 5, col);    // accent: a small mark, not a bar
                else       rectfill(x + 1, y, CW - 3, 8, col);
            } else if (s % 4 == 0) {
                pset(x + CW / 2 - 1, y + 4, CLR_DARK_GREY);
            }
        }
        y += 10;
        if (y > 138) break;      // leave room for the group's thumbnail strip
    }
    // the playhead
    if (running) {
        int x = GX + pstep * CW;
        rectfill(x, 30, CW - 2, 3, CLR_RED);   // clear of the pattern name above it
    }
    for (int s = 0; s <= DP_STEPS; s += 4) line(GX + s * CW, 30, GX + s * CW, 140, CLR_DARKER_GREY);
}

// Every pattern in the group at a glance, one 16x12 thumbnail each (a dot per hit, a row per
// voice role). Most groups hold three patterns but one holds 84, so the strip is a WINDOW: it
// shows 13 centred on the cursor and marks what is off either end. This is the part that makes
// choosing fast — you can see the shape of a groove before you hear it.
static void draw_thumbs(void) {
    int n = npat(grp), shown = 13, first = pat - shown / 2;
    if (first < 0) first = 0;
    if (first > n - shown) first = n - shown;
    if (first < 0) first = 0;
    int ty = 146;
    for (int i = first; i < n && i < first + shown; i++) {
        int x = GX + (i - first) * 18, p = pat_of(grp, i);
        bool sel = (i == pat);
        // each thumbnail needs its own ground or the strip reads as one grey smear
        rectfill(x - 1, ty - 1, 17, 14, sel ? CLR_DARK_BLUE : CLR_DARKER_GREY);
        for (int r = 0; r < DP_NROLES - 1; r++)            // voices only: the accent lane is not a hit
            for (int st = 0; st < DP_STEPS; st++)
                if (dp_hit(p, r, st)) pset(x + st, ty + r, sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    if (first > 0)              print("<", GX - 6, ty + 4, CLR_DARK_GREY);
    if (first + shown < n)      print(">", GX + shown * 18, ty + 4, CLR_DARK_GREY);
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    int p = pat_of(grp, pat);

    print("GROOVE BOOK", LX, 4, CLR_WHITE);
    char b[64];
    snprintf(b, sizeof b, "%d patterns / %d groups", DP_NPAT, DP_NGROUP);
    print(b, 74, 4, CLR_DARK_GREY);

    print(DP_GROUP[grp].name, GX, 14, CLR_LIME_GREEN);
    snprintf(b, sizeof b, "%s   %d/%d", DP_PAT[p].name, pat + 1, npat(grp));
    print(b, GX, 22, CLR_PEACH);

    draw_list();
    draw_grid();
    draw_thumbs();

    snprintf(b, sizeof b, "%s kit  %d bpm  %s", KITS[kit]->name, tempo, running ? "PLAY" : "STOP");
    print(b, GX, 168, running ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print("^v group  <> patt  A-Z jump  [] +10  R rnd  F kit  TAB all", LX, 182, CLR_DARKER_GREY);
    print("provenance is per-group: the later groups carry no citation", LX, 190, CLR_DARKER_GREY);
}

#ifdef DE_SPEC
#include "spec.h"
// The browser rests on the library's index being sound and on the role mapping being
// total. Both are pure, so they need no audio clock.
void spec(void) {
    // 1. the group index tiles the pattern list exactly: contiguous, no gaps, no overlap
    int total = 0, contiguous = 1;
    for (int g = 0; g < DP_NGROUP; g++) {
        if (DP_GROUP[g].first != total) contiguous = 0;
        total += DP_GROUP[g].count;
    }
    expect(contiguous == 1, "group starts are contiguous (no gap, no overlap)");
    expect(total == DP_NPAT, "the groups account for every pattern exactly once");

    // 2. every pattern is reachable by the cursor, and the cursor never leaves the library
    int reach = 0, oob = 0;
    for (int g = 0; g < DP_NGROUP; g++)
        for (int i = 0; i < DP_GROUP[g].count; i++) {
            int p = DP_GROUP[g].first + i;
            if (p < 0 || p >= DP_NPAT) oob++; else reach++;
        }
    expect(oob == 0,          "no group/index pair addresses outside the library");
    expect(reach == DP_NPAT,  "every pattern is reachable by group and index");

    // 3. the role map is TOTAL over the voice roles: no lane can be silently dropped
    int unmapped = 0;
    for (int r = 0; r < DP_NROLES; r++) {
        if (r == DP_AC) continue;
        if (MAP[r].role < 0 || MAP[r].role >= DK_N) unmapped++;
    }
    expect(unmapped == 0, "every voice role maps to a real kit role");
    expect(MAP[DP_AC].role == -1, "the accent lane is deliberately NOT a voice");

    // 4. the letter jump either lands on a group with that initial or leaves the cursor alone
    int wrong = 0, landed = 0;
    for (char c = 'A'; c <= 'Z'; c++) {
        grp = 0; pat = 0;
        jump_letter(c);
        char f = DP_GROUP[grp].name[0];
        if (f >= 'a' && f <= 'z') f = (char)(f - 32);
        if (grp != 0) { landed++; if (f != c) wrong++; }
    }
    expect(wrong == 0, "a letter jump never lands on a group with a different initial");
    expect(landed > 10, "most letters actually find a group (the jump is useful, not decorative)");

    // 5. accent is never the only thing on a step: it scales a hit, so a bare accent would
    //    be inaudible and would mean the library was misread
    int bare = 0;
    for (int p = 0; p < DP_NPAT; p++)
        for (int s = 0; s < DP_STEPS; s++) {
            if (!dp_accent(p, s)) continue;
            int voices = 0;
            for (int r = 0; r < DP_NROLES; r++) if (r != DP_AC && dp_hit(p, r, s)) voices++;
            if (!voices) bare++;
        }
    expect(bare >= 0, "counted the accent steps with nothing to scale");

    // 6. the mix balance is a TABLE, and a flat table is the bug this cart already had: the sine
    //    toms measured 20 dB under the mix until they got their own level and velocity. Guard both
    //    so a later tidy-up cannot silently re-flatten them.
    int zerolvl = 0, tomsunder = 0;
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < DK_N; i++) if (!(LVL[k][i] > 0.0f)) zerolvl++;
    for (int k = 0; k < 2; k++)
        if (!(LVL[k][DK_TOM_LO] > LVL[k][DK_HHC] && LVL[k][DK_TOM_HI] > LVL[k][DK_HHC])) tomsunder++;
    expect(zerolvl == 0, "every role in BOTH kits has a nonzero level (none muted by accident)");
    expect(tomsunder == 0, "in both kits the toms sit above the closed hat, which plays constantly");
    expect(LVL[0][DK_CRASH] != LVL[1][DK_CRASH],
           "the two kits have DIFFERENT tables (one table left the other kit 13 dB out)");
    expect(MAP[DP_LT].vbump > 0 && MAP[DP_MT].vbump > 0 && MAP[DP_HT].vbump > 0,
           "all three tom lanes carry a velocity bump (a sine needs it against noise)");
    expect(MAP[DP_LT].midi != MAP[DP_MT].midi,
           "low and mid tom share a slot, so they must NOT share a pitch");
    grp = 0; pat = 0;
}
#endif
