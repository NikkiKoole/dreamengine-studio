/* de:meta
{
  "slug": "acidface",
  "title": "acidface",
  "status": "active",
  "created": "2026-07-13",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [],
  "lineage": "acidwire",
  "description": {
    "summary": "A single-machine FULL-FACE wireframe (the 'Acid Pure' idiom, on Elektron x Roland x0x bones): one acid box given the whole phone screen — 6 knobs, a context DISPLAY flanked by 6 soft-keys, a lane strip, the 16-STEP BUTTON ROW (double-duty: step-select for the note lane, drum cells for a drum voice), and a keybed. The counter-proposal to acidwire's collapsing multi-machine rack: don't fold the band, show ONE machine as real hardware and switch fast.",
    "detail": "A design-play wireframe, born from a maker sketch (2026-07-13) after seeing the 'Acid Pure' app. Hardware lineage: the screen-flanked-by-soft-keys is Elektron (Digitakt/Rytm); the 16-step button row + acid voice are Roland x0x (TB-303/TR-808/909). Where acidwire (its lineage) fits acidrack's 4-5 machines onto a phone by an accordion (fold/compact/expand), this asks the opposite: give ONE machine the entire screen as a purpose-built hardware face. The 6 orange SOFT-KEYS flanking the display swap the flow (SEQ step/kit inspector / PAT / SONG / MIX / FX / SCOPE). The LANE STRIP (NOTE + BD/SD/CH/OH drum voices) picks what the 16-STEP ROW edits — the maker's key idea: those buttons do DOUBLE DUTY. NOTE lane = each button selects that step and the KEYBED sets its pitch (303 step-entry); a DRUM lane = each button toggles that voice's hit (a drum cell, 808/909 style). SHIFT flips the whole row to pattern-select. The SEQ display pairs with the row: NOTE shows the selected step's pitch + ACC/SLD/TIE toggles, a drum lane shows the whole-kit overview. Same device-honest scaffolding as acidwire: fixed 940x700 window, de_resize() reshapes the CANVAS to each device's logical @K=2 size (letterboxed), so a 44pt finger is a constant ~22 logical px and finger-comfort is HONEST across the device matrix. It's a feel-it-on-glass toy, not a spec — play it, then decide if the full-face model beats the accordion for a phone.",
    "controls": "TAP/CLICK: a SOFT-KEY (orange, flanking the screen) picks the display flow - a KNOB drag-turns - a LANE chip (NOTE/BD/SD/CH/OH) picks what the 16-step row edits - a STEP BUTTON: in NOTE lane SELECTS the step (then a KEYBED key sets its pitch + advances), in a drum lane TOGGLES that voice's hit - CLR/RND (lane strip) wipe/re-roll the active lane's pattern - ACC/SLD/TIE (in the SEQ display) flag the selected note step - < > (transport) step the pattern - ▶ plays (animates the playhead across the row). KEYS: ]/->/space/` next device shape - [/<- prev - 1-9 jump - l cycle lane - w SHIFT (row -> pattern-select) - , . pattern prev/next - s safe-area skin - g 1-finger grid - h hide the label."
  }
}
de:meta */
// ACIDFACE — the single-machine full-face wireframe (the "Acid Pure" idiom).
// The counter-proposal to acidwire's collapsing rack: one machine, the whole
// screen, as real hardware. Same honest-device scaffolding as acidwire —
// de_resize() reshapes the CANVAS per device (device-matrix.md §2), letterboxed,
// so finger-comfort is honest at every shape. See acidrack-layout-brief.md.

#include "studio.h"
#include <math.h>
#include <string.h>
#include "lay.h"
#include "ui.h"   // real widgets (ui_knob/ui_slider) — per-finger capture + fat hit pads, so the
                  // wireframe's controls actually MOVE (no sound; just honest finger-ergonomics)

extern void de_resize(int w, int h);   // engine seam (platform.h): set the active canvas size
#define KEY_GRAVE 96                   // the key left of `1` (a `§` on ISO Mac boards)

// ───────── device matrix (device-matrix.md §2) — logical @ K=2 + safe hardware (from acidwire) ─────────
enum { TALL, WIDE, ROOMY };
enum { N_HOME, N_NOTCH, N_ISLAND, N_PAD };
typedef struct { const char *name; int w, h, cls, insT, insB, notch, cornR; } Dev;
static Dev DEV[] = {
    { "iPhone SE",        188, 334, TALL,  10,  0, N_HOME,    3 },
    { "iPhone 13 mini",   188, 406, TALL,  24, 16, N_NOTCH,  11 },
    { "iPhone 16 / 15",   196, 426, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Plus",   215, 466, TALL,  24, 16, N_ISLAND, 14 },
    { "iPhone 16 Pro Max", 220, 478, TALL, 26, 16, N_ISLAND, 15 },
    { "iPhone SE land",   334, 188, WIDE,   6,  0, N_HOME,    3 },
    { "iPhone 16 land",   426, 196, WIDE,   8, 12, N_ISLAND, 14 },
    { "iPad mini",        372, 566, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 11\"",        417, 597, ROOMY, 12,  8, N_PAD,     9 },
    { "iPad 13\"",        516, 688, ROOMY, 12,  8, N_PAD,    10 },
    { "iPad 13\" land",   688, 516, ROOMY, 12,  8, N_PAD,    10 },
};
#define NDEV 11
#define FU ((float)finger_px())   // a comfortable finger from the engine: 44pt in logical px

// ───────── the machine + its state ─────────
// 6 knobs = the full acid-box panel (no compact compromise — the whole point of a full face).
static const char *const KNOB[6] = { "TUN", "CUT", "RES", "ENV", "DEC", "ACC" };
static float kv[6];

// the DISPLAY flows the soft-keys swap between — "the big screen shows dedicated flows"
enum { FL_SEQ, FL_PAT, FL_SONG, FL_MIX, FL_FX, FL_SCOPE, NFLOW };
static const char *const FLOW[NFLOW] = { "SEQ", "PAT", "SONG", "MIX", "FX", "SCOPE" };
static int flow = FL_SEQ;

#define STEPS 16
#define NPAT 6
// The machine has a pitched NOTE lane + a few DRUM voices. The 16-step BUTTON ROW (the x0x
// signature — the maker's idea) edits whichever lane is active: it does DOUBLE DUTY as step-select
// for the note lane and as drum cells for a drum voice. The lane strip picks which.
enum { LN_NOTE, LN_BD, LN_SD, LN_CH, LN_OH, NLANE };
static const char *const LANE[NLANE] = { "NOTE", "BD", "SD", "CH", "OH" };
static signed char  pit[NPAT][STEPS];    // NOTE lane: pitch 0..11 within an octave, or -1 = rest
static unsigned char acc[NPAT][STEPS], sld[NPAT][STEPS], tie[NPAT][STEPS];   // NOTE per-step flags
static unsigned char drum[NPAT][NLANE][STEPS];   // DRUM lanes: hit on/off (the LN_NOTE slot unused)
static int patn = 0, selstep = 0, lane = LN_NOTE;   // pattern · selected step (NOTE cursor) · active lane
static int chain[8];                     // SONG: a chain of pattern indices
static float mixlv[4] = { 0.85f, 0.7f, 0.75f, 0.6f };   // MIX faders: MASTER / OSC / DRUM / ACID
static float fxk[3];                     // FX knobs: DLY / FB / RVB
static int shift = 0;                    // SHIFT: flips the 16-step row to PATTERN-SELECT (x0x mode)

static float phase = 0;                  // transport playhead phase (advances when playing)
static int   playing = 0, lastnote = -1; // lastnote = the last key tapped (for the display header)

// device-flip / overlay tool state (from acidwire)
static int cur = 0, applied = -1, showlabel = 1, safehint = 1, fingergrid = 0, seeded = 0;

// ───────── one-press tap layer (touch + mouse), from acidwire ─────────
static int m_press = 0, m_x = 0, m_y = 0;
static int clicked(Box b) {
    if (m_press && m_x >= b.x && m_x < b.x + b.w && m_y >= b.y && m_y < b.y + b.h) { m_press = 0; return 1; }
    return 0;
}

static const char *notename(int p) {   // p 0..11 → note name (no octave)
    static const char *const N[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return (p >= 0 && p < 12) ? N[p] : "--";
}

static void seed(void) {   // spread values so nothing sits dead; deterministic
    if (seeded) return; seeded = 1;
    for (int i = 0; i < 6; i++) kv[i] = 0.25f + 0.11f * ((i * 3 + 1) % 6);
    for (int i = 0; i < 3; i++) fxk[i] = 0.3f + 0.15f * i;
    for (int p = 0; p < NPAT; p++) for (int i = 0; i < STEPS; i++) {
        int on = ((i + p) % 3) != 0;
        pit[p][i] = on ? (signed char)((i * 5 + p * 3) % 12) : -1;
        acc[p][i] = (i % 4 == 0); sld[p][i] = (i % 6 == 3); tie[p][i] = 0;
        drum[p][LN_BD][i] = (i % 4 == 0);                 // four-on-the-floor
        drum[p][LN_SD][i] = (i % 8 == 4);                 // backbeat
        drum[p][LN_CH][i] = (i % 2 == 0);                 // 8ths
        drum[p][LN_OH][i] = (i % 8 == 6);                 // an off-beat open hat
    }
    for (int i = 0; i < 8; i++) chain[i] = i % NPAT;
}

// ───────── a labelled soft-key (the orange screen-flanking buttons) ─────────
static void softkey(Box b, int f) {
    int on = (flow == f);
    boxfill(b, on ? CLR_ORANGE : CLR_DARK_ORANGE);
    boxrect(b, on ? CLR_LIGHT_YELLOW : CLR_BROWN);
    font(FONT_TINY);
    print_centered(FLOW[f], (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_BLACK : CLR_LIGHT_PEACH);
    if (clicked(b)) flow = f;
}

// ───────── the DISPLAY — a black LCD that renders the current flow ─────────
// a small labelled flag toggle inside the display (ACC/SLD/TIE for the selected note step)
static void flagbtn(Box b, const char *lbl, unsigned char *v) {
    boxfill(b, *v ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
    boxrect(b, *v ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_TINY);
    print_centered(lbl, (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), *v ? CLR_LIME_GREEN : CLR_LIGHT_GREY);
    if (clicked(b)) *v = !*v;
}
// pattern edit ops (the "screen is a UI element" point — these live IN the display, next to what
// they act on, not out on the chrome). They act on the ACTIVE lane's current pattern.
static void lane_clear(void) {
    if (lane == LN_NOTE) for (int i = 0; i < STEPS; i++) { pit[patn][i] = -1; acc[patn][i] = sld[patn][i] = tie[patn][i] = 0; }
    else for (int i = 0; i < STEPS; i++) drum[patn][lane][i] = 0;
}
static void lane_rand(void) {
    for (int i = 0; i < STEPS; i++)
        if (lane == LN_NOTE) pit[patn][i] = ((i * 7 + selstep * 3) % 4 == 0) ? -1 : (signed char)((i * 5 + patn * 2 + selstep) % 12);
        else drum[patn][lane][i] = ((i * 3 + patn + lane) % 3 == 0);
}
// a momentary op button drawn inside the display; returns 1 the frame it's tapped
static int opbtn(Box b, const char *lbl) {
    boxfill(b, CLR_DARK_GREEN); boxrect(b, CLR_LIME_GREEN);
    font(FONT_TINY);
    print_centered(lbl, (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), CLR_LIGHT_PEACH);
    return clicked(b);
}
// SEQ flow = the interactive EDITOR inside the display (Pure Acid's pink screen: the pattern lives
// here and is edited here; the ops live here too). NOTE lane: a touchable piano-roll + a step edit
// strip (ACC/SLD/TIE for the selected step, CLR/RND for the pattern). DRUM lane: the whole-kit
// overview + CLR/RND for the active voice. Pairs with the physical 16-step row (tactile toggle).
static void screen_seq(Box s) {
    font(FONT_TINY);
    print(str("%s  PAT %c", LANE[lane], 'A' + patn), (int)s.x + 3, (int)s.y + 2, CLR_LIME_GREEN);
    int pl = playing ? ((int)phase) % STEPS : -1;
    float editH = lay_clamp(s.h * 0.28f, 12, 26);
    if (lane == LN_NOTE) {
        print_right(pit[patn][selstep] < 0 ? "REST" : notename(pit[patn][selstep]),
                    (int)(s.x + s.w - 3), (int)s.y + 2, pit[patn][selstep] < 0 ? CLR_DARK_GREY : CLR_BLUE_GREEN);
        Box body = lay_pad(s, 9, 3, 3, 3);
        Box roll; Box edit = lay_split(body, EDGE_BOTTOM, editH, &roll);
        // the touchable piano-roll: pitch as bar height, accent bright, slide underlined, tie dot
        float gap = lay_clamp(roll.w * 0.008f, 0.4f, 1.5f);
        for (int i = 0; i < STEPS; i++) {
            Box c = lay_grid(roll, STEPS, STEPS, i, gap);
            boxfill(c, i == pl ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
            if (pit[patn][i] >= 0) {
                float fr = (pit[patn][i] + 1) / 12.0f, bh = c.h * (0.2f + 0.72f * fr);
                boxfill(box(c.x, c.y + c.h - bh, c.w, bh), acc[patn][i] ? CLR_LIGHT_YELLOW : CLR_LIME_GREEN);
                if (sld[patn][i]) boxfill(box(c.x, c.y + c.h - 2, c.w, 1), CLR_ORANGE);
                if (tie[patn][i]) pset((int)(c.x + c.w / 2), (int)(c.y + 1), CLR_PINK);
            }
            if (i == selstep) boxrect(c, CLR_WHITE);
            if (clicked(c)) selstep = i;
        }
        // step edit strip: ACC · SLD · TIE (selected step) | CLR · RND (pattern)
        Box f0 = lay_inset(lay_grid(edit, 5, 5, 0, 2), 1), f1 = lay_inset(lay_grid(edit, 5, 5, 1, 2), 1),
            f2 = lay_inset(lay_grid(edit, 5, 5, 2, 2), 1), c0 = lay_inset(lay_grid(edit, 5, 5, 3, 2), 1),
            r0 = lay_inset(lay_grid(edit, 5, 5, 4, 2), 1);
        flagbtn(f0, "ACC", &acc[patn][selstep]); flagbtn(f1, "SLD", &sld[patn][selstep]); flagbtn(f2, "TIE", &tie[patn][selstep]);
        if (opbtn(c0, "CLR")) lane_clear();
        if (opbtn(r0, "RND")) lane_rand();
    } else {
        Box body = lay_pad(s, 9, 3, 3, 3);
        Box kit; Box edit = lay_split(body, EDGE_BOTTOM, editH, &kit);
        // the whole-kit overview (all 4 voices × 16), the active voice's row highlighted
        for (int v = LN_BD; v < NLANE; v++) {
            Box rowb = lay_grid(kit, 1, 4, v - LN_BD, 2);
            int active = (v == lane);
            font(FONT_TINY);
            print(LANE[v], (int)rowb.x, (int)(rowb.y + (rowb.h - 5) / 2), active ? CLR_ORANGE : CLR_LIGHT_GREY);
            Box cells = lay_pad(rowb, 0, 0, 0, 14);
            for (int i = 0; i < STEPS; i++) {
                Box c = lay_inset(lay_grid(cells, STEPS, STEPS, i, 0.5f), 0.3f);
                boxfill(c, i == pl ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
                if (drum[patn][v][i]) boxfill(c, active ? CLR_ORANGE : CLR_MEDIUM_GREEN);
                if (active && clicked(c)) drum[patn][v][i] = !drum[patn][v][i];   // the overview is editable too
            }
        }
        // CLR · RND for the active drum voice
        if (opbtn(lay_inset(lay_grid(edit, 2, 2, 0, 2), 1), "CLR")) lane_clear();
        if (opbtn(lay_inset(lay_grid(edit, 2, 2, 1, 2), 1), "RND")) lane_rand();
    }
}
static void screen_pat(Box s) {
    font(FONT_TINY);
    print("PATTERN", (int)s.x + 3, (int)s.y + 2, CLR_LIME_GREEN);
    Box g = lay_pad(s, 10, 3, 3, 3);
    for (int i = 0; i < NPAT; i++) {
        Box c = lay_inset(lay_grid(g, 3, NPAT, i, 2), 1);   // 2 rows × 3
        int on = (i == patn);
        boxfill(c, on ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
        boxrect(c, on ? CLR_LIME_GREEN : CLR_DARK_GREY);
        print_centered(str("%c", 'A' + i), (int)(c.x + c.w / 2), (int)(c.y + (c.h - 5) / 2),
                       on ? CLR_LIME_GREEN : CLR_LIGHT_GREY);
        if (clicked(c)) patn = i;
    }
}
static void screen_song(Box s) {
    font(FONT_TINY);
    print("SONG CHAIN", (int)s.x + 3, (int)s.y + 2, CLR_LIME_GREEN);
    Box g = lay_pad(s, 10, 3, 3, 3);
    int pl = playing ? ((int)(phase / STEPS)) % 8 : -1;
    for (int i = 0; i < 8; i++) {
        Box c = lay_inset(lay_grid(g, 8, 8, i, 1), 1);
        boxfill(c, i == pl ? CLR_DARK_GREEN : CLR_BROWNISH_BLACK);
        boxrect(c, CLR_DARK_GREY);
        print_centered(str("%c", 'A' + chain[i]), (int)(c.x + c.w / 2), (int)(c.y + (c.h - 5) / 2), CLR_BLUE_GREEN);
        if (clicked(c)) chain[i] = (chain[i] + 1) % NPAT;   // tap cycles the slot's pattern
    }
}
static void screen_mix(Box s) {
    static const char *const L[4] = { "MST", "OSC", "DRM", "ACD" };
    Box g = lay_pad(s, 3, 3, 3, 3);
    for (int i = 0; i < 4; i++) {
        Box c = lay_grid(g, 4, 4, i, 3);
        float bh = c.h * 0.72f;
        Box track = box(c.x + c.w / 2 - 3, c.y + 4, 6, bh);
        boxfill(track, CLR_BROWNISH_BLACK); boxrect(track, CLR_DARK_GREY);
        boxfill(box(track.x, track.y + track.h * (1 - mixlv[i]), track.w, track.h * mixlv[i]),
                CLR_LIME_GREEN);
        font(FONT_TINY);
        print_centered(L[i], (int)(c.x + c.w / 2), (int)(c.y + c.h - 6), CLR_LIGHT_GREY);
        // drag the whole track column to set the level
        Box hit = box(c.x, c.y, c.w, c.h);
        if (m_press && m_x >= hit.x && m_x < hit.x + hit.w && m_y >= hit.y && m_y < hit.y + hit.h) {
            mixlv[i] = 1 - lay_clamp((m_y - track.y) / track.h, 0, 1); m_press = 0;
        }
    }
}
static void screen_fx(Box s) {
    static const char *const L[3] = { "DLY", "FB", "RVB" };
    Box g = lay_pad(s, 2, 2, 2, 2);
    for (int i = 0; i < 3; i++) {
        Box c = lay_grid(g, 3, 3, i, 2);
        ui_knob(&fxk[i], (int)(c.x + c.w / 2), (int)(c.y + c.h / 2 - 2), L[i]);
    }
}
static void screen_scope(Box s) {   // pure eye-candy flow — reacts to tempo + last note
    Box g = lay_inset(s, 3);
    int base = lastnote >= 0 ? lastnote : 5;
    for (int x = 0; x < (int)g.w; x++) {
        float t = x / g.w * 6.283f * (2 + base * 0.25f) + phase * 0.6f;
        float y = g.y + g.h / 2 + sinf(t) * g.h * 0.32f * (0.4f + fxk[2] * 0.6f)
                  + sinf(t * 2.3f) * g.h * 0.12f;
        pset((int)(g.x + x), (int)y, CLR_LIME_GREEN);
    }
    font(FONT_TINY);
    print(str("SCOPE  %s", notename(lastnote < 0 ? -1 : lastnote % 12)), (int)g.x + 2, (int)g.y + 2, CLR_BLUE_GREEN);
}
static void draw_screen(Box s) {
    boxfill(s, CLR_BLACK); boxrect(lay_inset(s, -1), CLR_DARK_GREEN);
    switch (flow) {
        case FL_SEQ:   screen_seq(s);   break;
        case FL_PAT:   screen_pat(s);   break;
        case FL_SONG:  screen_song(s);  break;
        case FL_MIX:   screen_mix(s);   break;
        case FL_FX:    screen_fx(s);    break;
        case FL_SCOPE: screen_scope(s); break;
    }
}

// ───────── the KEYBED — plays notes; with SHIFT it flips to 16 step-pads ─────────
// (the maker's "use the drum keys for mode switching" — the bottom row is dual-purpose.)
static const int WHITE[7] = { 0, 2, 4, 5, 7, 9, 11 };   // semitone of each white key
static const int BLACKAT[5] = { 0, 1, 3, 4, 5 };        // white index a black key sits after (skip E,B)
static const int BLACKST[5] = { 1, 3, 6, 8, 10 };       // its semitone

static void play_note(int semi) {
    lastnote = semi;
    if (lane == LN_NOTE) {                 // NOTE lane: set the selected step's pitch, then advance
        pit[patn][selstep] = (signed char)semi;
        selstep = (selstep + 1) % STEPS;
    }
}
static void draw_keybed(Box area) {
    // one octave of piano keys (whites full-height, blacks half-height overlaid at the gaps)
    float kw = area.w / 7.0f;
    for (int i = 0; i < 7; i++) {                       // white keys
        Box k = box(area.x + i * kw, area.y, kw - 1, area.h);
        int lit = (lastnote == WHITE[i]);
        boxfill(k, lit ? CLR_LIME_GREEN : CLR_WHITE); boxrect(k, CLR_DARK_GREY);
        if (clicked(k)) play_note(WHITE[i]);
    }
    for (int i = 0; i < 5; i++) {                        // black keys
        Box k = box(area.x + (BLACKAT[i] + 1) * kw - kw * 0.3f, area.y, kw * 0.6f, area.h * 0.6f);
        int lit = (lastnote == BLACKST[i]);
        boxfill(k, lit ? CLR_ORANGE : CLR_BROWNISH_BLACK); boxrect(k, CLR_BLACK);
        if (clicked(k)) play_note(BLACKST[i]);
    }
}

// ───────── the LANE strip — picks what the 16-step row edits (NOTE + drum voices) ─────────
// Just the lanes now — the CLR/RND edit ops moved INTO the display (the maker's "the screen is a UI
// element; clear/random live in the little screen" call, 2026-07-13).
static void draw_lanestrip(Box row) {
    font(FONT_TINY);
    float gap = lay_clamp(row.w * 0.006f, 0.5f, 2);
    for (int i = 0; i < NLANE; i++) {
        Box b = lay_inset(lay_grid(row, NLANE, NLANE, i, gap), 0.5f);
        int on = (i == lane);
        boxfill(b, on ? CLR_TRUE_BLUE : CLR_DARK_GREEN);
        boxrect(b, on ? CLR_LIGHT_YELLOW : CLR_DARKER_GREY);
        print_centered(LANE[i], (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_WHITE : CLR_LIGHT_PEACH);
        if (clicked(b)) lane = i;
    }
}

// ───────── the 16-STEP BUTTON ROW — the x0x signature, double-duty by lane ─────────
// NOTE lane: tap SELECTS the step (the keybed then sets its pitch); lit = the step has a note.
// DRUM lane: tap TOGGLES that voice's hit (a drum cell); lit = hit. Beat-grouped 4s; playhead lit.
// SHIFT flips the whole row to PATTERN-SELECT (buttons 1..NPAT pick the pattern — the x0x mode key).
static void draw_steprow(Box row) {
    font(FONT_TINY);
    float gap = lay_clamp(row.w * 0.004f, 0.5f, 2.5f);
    int pl = playing ? ((int)phase) % STEPS : -1;
    for (int i = 0; i < STEPS; i++) {
        // beat-group gaps: a wider gutter every 4 steps so the bar reads at a glance
        Box b = lay_grid(row, STEPS, STEPS, i, gap);
        if (i % 4 == 0 && i) b = lay_pad(b, 0, 0, 0, gap);
        int grouped = ((i / 4) % 2) == 0;                       // alternate 4-group base shade
        if (shift) {                                            // PATTERN-SELECT mode
            int usable = (i < NPAT), on = (i == patn);
            boxfill(b, on ? CLR_ORANGE : (usable ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK));
            boxrect(b, on ? CLR_LIGHT_YELLOW : CLR_BROWN);
            if (usable) print_centered(str("%c", 'A' + i), (int)(b.x + b.w / 2), (int)(b.y + (b.h - 5) / 2), on ? CLR_BLACK : CLR_LIGHT_PEACH);
            if (clicked(b) && usable) patn = i;
            continue;
        }
        int on, sel = 0;
        if (lane == LN_NOTE) { on = pit[patn][i] >= 0; sel = (i == selstep); }
        else on = drum[patn][lane][i];
        int base = grouped ? CLR_DARK_BROWN : CLR_BROWNISH_BLACK;
        boxfill(b, i == pl ? CLR_DARK_GREEN : base);            // playhead tints the whole cell
        if (on) boxfill(lay_inset(b, b.w * 0.16f), lane == LN_NOTE ? (acc[patn][i] ? CLR_LIGHT_YELLOW : CLR_ORANGE) : CLR_ORANGE);
        boxrect(b, sel ? CLR_WHITE : CLR_BROWN);               // selected note step gets a white ring
        if (lane == LN_NOTE && on && sld[patn][i]) boxfill(box(b.x + 1, b.y + b.h - 2, b.w - 2, 1), CLR_LIME_GREEN);
        if (clicked(b)) { if (lane == LN_NOTE) selstep = i; else drum[patn][lane][i] = !drum[patn][lane][i]; }
    }
}

// ───────── device safe-area skin + finger grid (from acidwire) ─────────
static void round_corner(int ox, int oy, int r, int sx, int sy) {
    for (int y = 0; y < r; y++) for (int x = 0; x < r; x++) {
        int dx = r - x, dy = r - y;
        if (dx * dx + dy * dy > r * r) pset(ox + sx * x, oy + sy * y, CLR_BLACK);
    }
}
static void hpill(float x, float y, float w, float h, int col) {   // rounded-end horizontal pill
    int r = (int)(h / 2);
    boxfill(box(x + r, y, w - 2 * r, h), col);
    circfill((int)x + r, (int)y + r, r, col); circfill((int)(x + w) - r, (int)y + r, r, col);
}
static float fmin2(float a, float b) { return a < b ? a : b; }
static void dashrect(Box b, int col) {   // dashed outline = "keep controls inside here"
    int d = 4, g = 3;
    for (int x = (int)b.x; x < b.x + b.w; x += d + g) { line(x, (int)b.y, (int)fmin2(x + d, b.x + b.w), (int)b.y, col); line(x, (int)(b.y + b.h) - 1, (int)fmin2(x + d, b.x + b.w), (int)(b.y + b.h) - 1, col); }
    for (int y = (int)b.y; y < b.y + b.h; y += d + g) { line((int)b.x, y, (int)b.x, (int)fmin2(y + d, b.y + b.h), col); line((int)(b.x + b.w) - 1, y, (int)(b.x + b.w) - 1, (int)fmin2(y + d, b.y + b.h), col); }
}
static void draw_safe_skin(int W, int H, Dev d) {
    if (d.insT > 0) boxfill(box(0, 0, W, d.insT), CLR_DARKER_PURPLE);
    if (d.insB > 0) boxfill(box(0, H - d.insB, W, d.insB), CLR_DARKER_PURPLE);
    if (d.insT >= 9) { font(FONT_TINY);
        print("9:41", 4, (int)(d.insT - 5) / 2, CLR_LIGHT_GREY);
        boxfill(box(W - 12, (d.insT - 5) / 2, 9, 5), CLR_DARK_GREY); boxfill(box(W - 12, (d.insT - 5) / 2 + 1, 7, 3), CLR_LIME_GREEN); }
    int cx = W / 2;
    if (d.notch == N_NOTCH) {
        int nw = (int)(W * 0.46f), nh = d.insT;
        boxfill(box(cx - nw / 2, 0, nw, nh), CLR_BLACK);
        circfill(cx - nw / 2, nh, 3, CLR_BLACK); circfill(cx + nw / 2, nh, 3, CLR_BLACK);
        boxfill(box(cx - nw / 2 - 3, nh - 3, 3, 3), CLR_BLACK); boxfill(box(cx + nw / 2, nh - 3, 3, 3), CLR_BLACK);
    } else if (d.notch == N_ISLAND) {
        int iw = (int)(W * 0.30f), ih = (int)(d.insT * 0.55f); if (ih < 6) ih = 6;
        hpill(cx - iw / 2, 4, iw, ih, CLR_BLACK);
    }
    if (d.notch != N_HOME) { int hw = (int)(W * (d.cls == WIDE ? 0.22f : 0.34f));
        hpill(cx - hw / 2, H - 5, hw, 3, CLR_LIGHT_GREY); }
    int r = d.cornR;
    round_corner(0, 0, r, 1, 1); round_corner(W - 1, 0, r, -1, 1);
    round_corner(0, H - 1, r, 1, -1); round_corner(W - 1, H - 1, r, -1, -1);
    dashrect(box(2, d.insT, W - 4, H - d.insT - d.insB), CLR_ORANGE);
}
static void draw_fingergrid(int W, int H) {
    for (float x = FU; x < W; x += FU) for (int y = 0; y < H; y += 3) pset((int)x, y, CLR_INDIGO);
    for (float y = FU; y < H; y += FU) for (int x = 0; x < W; x += 3) pset(x, (int)y, CLR_INDIGO);
    boxfill(box(3, 3, FU, FU), CLR_INDIGO);
    font(FONT_TINY); print("1 finger", 3, (int)FU + 4, CLR_INDIGO);
}
static void play_tri(Box b, int col) {
    int n = (int)b.h; if (n < 1) n = 1;
    for (int i = 0; i < n; i++) {
        float d = fabsf(i - (n - 1) / 2.0f) / ((n - 1) / 2.0f + 0.001f);
        line((int)b.x, (int)b.y + i, (int)b.x + (int)(b.w * (1.0f - d)), (int)b.y + i, col);
    }
}

void update(void) {
    if (keyp(KEY_RIGHT) || keyp(']') || keyp(KEY_SPACE) || keyp(KEY_GRAVE)) cur = (cur + 1) % NDEV;
    if (keyp(KEY_LEFT)  || keyp('[')) cur = (cur + NDEV - 1) % NDEV;
    for (int i = 0; i < 9 && i < NDEV; i++) if (keyp('1' + i)) cur = i;
    if (keyp('H')) showlabel = !showlabel;
    if (keyp('S')) safehint = !safehint;
    if (keyp('G')) fingergrid = !fingergrid;
    if (keyp('W')) shift = !shift;                         // SHIFT: step row → pattern-select
    if (keyp(',')) patn = (patn + NPAT - 1) % NPAT;        // pattern prev / next
    if (keyp('.')) patn = (patn + 1) % NPAT;
    if (keyp('L')) lane = (lane + 1) % NLANE;              // cycle the active lane
    if (playing) phase += 0.08f;   // playhead advance (constant; a tempo knob can drive this later)
#ifndef DE_RESIZABLE
    if (cur != applied) { de_resize(DEV[cur].w, DEV[cur].h); applied = cur; }
#endif
}

void draw(void) {
    Dev d = DEV[cur];
    int W = screen_w(), H = screen_h();
    int cls_, insT, insB;
#ifdef DE_RESIZABLE
    int device_mode = 1;
    cls_ = (W >= 360 && H >= 360) ? ROOMY : (W > H ? WIDE : TALL);
    { int sx, sy, sw, sh; safe_rect(&sx, &sy, &sw, &sh); insT = sy; insB = H - (sy + sh); }
#else
    int device_mode = 0;
    cls_ = d.cls; insT = d.insT; insB = d.insB;
#endif
    m_press = mouse_pressed(0); m_x = mouse_x(); m_y = mouse_y();
    cls(CLR_MEDIUM_GREEN);   // the green device body (the sketch)
    seed(); ui_begin();

    Box full = box(0, 0, W, H);
    Box safe = lay_pad(full, insT, 0, insB, 0);

    // ─── pinned transport (top): ▶/■ + BPM + LOOP ───
    // landscape is vertical-starved — the topbar carries little, so keep it THIN there and hand the
    // reclaimed height to the screen (the maker's landscape note, 2026-07-13).
    float trH = (cls_ == WIDE) ? lay_clamp(FU * 0.55f, 11, 15) : lay_clamp(FU * 1.0f, 14, 26);
    Box afterTr; Box transport = lay_split(safe, EDGE_TOP, trH, &afterTr);
    boxfill(transport, CLR_DARK_GREEN);
    float bs = lay_clamp(trH * 0.7f, 8, 22);
    Box pbtn = box(transport.x + 3, transport.y + (transport.h - bs) / 2, bs, bs);
    boxfill(pbtn, CLR_BROWNISH_BLACK); boxrect(pbtn, CLR_LIGHT_PEACH);
    if (playing) boxfill(box(pbtn.x + bs * 0.3f, pbtn.y + bs * 0.3f, bs * 0.4f, bs * 0.4f), CLR_LIGHT_PEACH);
    else         play_tri(lay_inset(pbtn, bs * 0.28f), CLR_LIME_GREEN);
    if (clicked(pbtn)) playing = !playing;
    // pattern prev/next ◀ ▶ right after the play button
    float nb = lay_clamp(bs * 0.7f, 7, 16);
    Box prevb = box(pbtn.x + bs + 4, pbtn.y + (bs - nb) / 2, nb, nb);
    Box nextb = box(prevb.x + nb + 2, prevb.y, nb, nb);
    boxfill(prevb, CLR_BROWNISH_BLACK); boxrect(prevb, CLR_LIGHT_PEACH);
    boxfill(nextb, CLR_BROWNISH_BLACK); boxrect(nextb, CLR_LIGHT_PEACH);
    font(FONT_TINY);
    print_centered("<", (int)(prevb.x + prevb.w / 2), (int)(prevb.y + (prevb.h - 5) / 2), CLR_LIGHT_PEACH);
    print_centered(">", (int)(nextb.x + nextb.w / 2), (int)(nextb.y + (nextb.h - 5) / 2), CLR_LIGHT_PEACH);
    if (clicked(prevb)) patn = (patn + NPAT - 1) % NPAT;
    if (clicked(nextb)) patn = (patn + 1) % NPAT;
    print(str("%s %c  %s%s", LANE[lane], 'A' + patn, shift ? "PAT-SEL " : "", playing ? "" : ""),
          (int)(nextb.x + nb + 5), (int)(transport.y + (transport.h - 5) / 2), shift ? CLR_ORANGE : CLR_LIGHT_PEACH);
    print_right("LOOP", (int)(transport.x + transport.w - 3), (int)(transport.y + (transport.h - 6) / 2), CLR_LIGHT_GREY);

    Box body = lay_pad(afterTr, 2, 2, 2, 2);

    // ─── the bands of the face (sketch top→bottom): knobs · screen+softkeys · lane strip ·
    //     16-step row · keybed. The lane strip + step row are the x0x double-duty surface. ───
    // Shape-aware band heights. In landscape everything shrinks so the SCREEN keeps the center — the
    // knob row and keybed get lean (they're wide-and-short there), the step/lane rows stay finger-sized.
    Box afterKnob, afterKb, afterStep, afterLane;
    int wide = (cls_ == WIDE);
    float knobH = wide ? lay_clamp(FU * 1.25f, 20, 38) : lay_clamp(FU * 1.9f, 26, 74);
    float kbH   = wide ? lay_clamp(body.h * 0.16f, 18, 44) : lay_clamp(body.h * 0.2f, 24, 88);
    float stepH = wide ? lay_clamp(FU * 0.85f, 14, 26)    : lay_clamp(FU * 1.0f, 16, 32);
    float laneH = wide ? lay_clamp(FU * 0.7f, 12, 20)     : lay_clamp(FU * 0.8f, 13, 26);

    Box knobrow   = lay_split(body, EDGE_TOP, knobH, &afterKnob);
    Box keybed    = lay_split(afterKnob, EDGE_BOTTOM, kbH, &afterKb);
    Box steprow   = lay_split(afterKb, EDGE_BOTTOM, stepH, &afterStep);
    Box lanestrip = lay_split(afterStep, EDGE_BOTTOM, laneH, &afterLane);
    Box screenband = lay_pad(afterLane, 2, 0, 3, 0);

    // 6 knobs across the top
    float kgap = lay_clamp(FU * 0.1f, 1, 3);
    for (int i = 0; i < 6; i++) {
        Box c = lay_grid(knobrow, 6, 6, i, kgap);
        if (c.w > 4 && c.h > 6) ui_knob(&kv[i], (int)(c.x + c.w / 2), (int)(c.y + c.h / 2), KNOB[i]);
    }

    // the screen flanked by 3 soft-keys each side
    float skw = lay_clamp(FU * 1.5f, 22, 48);
    Box afterL, afterR;
    Box lcol = lay_split(screenband, EDGE_LEFT, skw, &afterL);
    Box rcol = lay_split(afterL, EDGE_RIGHT, skw, &afterR);
    Box scr  = lay_pad(afterR, 0, 2, 0, 2);
    int lf[3] = { FL_SEQ, FL_PAT, FL_SONG }, rf[3] = { FL_MIX, FL_FX, FL_SCOPE };
    for (int i = 0; i < 3; i++) softkey(lay_inset(lay_grid(lcol, 1, 3, i, 3), 1), lf[i]);
    for (int i = 0; i < 3; i++) softkey(lay_inset(lay_grid(rcol, 1, 3, i, 3), 1), rf[i]);
    draw_screen(scr);

    draw_lanestrip(lay_pad(lanestrip, 1, 0, 1, 0));
    draw_steprow(lay_pad(steprow, 1, 0, 1, 0));
    draw_keybed(keybed);

    // ─── device chrome / tool overlays ───
    if (!device_mode && safehint) draw_safe_skin(W, H, d);
    if (fingergrid) draw_fingergrid(W, H);
    if (!device_mode && showlabel) {
        font(FONT_TINY);
        const char *l1 = str("%s  %dx%d  full-face", d.name, d.w, d.h);
        const char *l2 = str("%.1fx%.1f fing  ]/[ shape  l lane  w shift  s skin  g grid", d.w / FU, d.h / FU);
        int lw = 8 + (int)(strlen(l1) > strlen(l2) ? strlen(l1) : strlen(l2)) * 4;
        Box chip = box(2, H - d.insB - 20, lw < W - 4 ? lw : W - 4, 16);
        boxfill(chip, CLR_BLACK); boxrect(chip, CLR_ORANGE);
        print(l1, (int)chip.x + 3, (int)chip.y + 2, CLR_LIGHT_PEACH);
        print(l2, (int)chip.x + 3, (int)chip.y + 9, CLR_ORANGE);
        if (clicked(chip)) cur = (cur + 1) % NDEV;
    }
    ui_end();
    font(FONT_NORMAL);
}
