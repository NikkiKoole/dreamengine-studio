/* de:meta
{
  "slug": "tubescreamer",
  "title": "tube screamer",
  "status": "active",
  "created": "2026-07-20",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Third gated famous-hardware voicing (after aquapuss's BBD delay + springtank's spring reverb): the Ibanez Tube Screamer as drive_voice(DRIVE_VOICE_TS) on the drive insert. The clip was always there (DRIVE_ASYM); the TS is the FILTERING around it.",
  "homage": "Ibanez TS808 Tube Screamer (1979).",
  "description": "The Ibanez Tube Screamer — a clean guitar chugging through the most-copied overdrive on earth. Three knobs, exactly like the real pedal: OVERDRIVE (how hard the mids clip), TONE (dark<->bright), LEVEL (output). What makes it a TS and not a fuzz is the FILTERING, not the clip: the bass BYPASSES the clipper so the low end stays tight (no flub), the highs get rolled off, and clean-lows + soft-clipped-mids + rolled-highs give the famous MID HUMP that cuts through a band. Stomp the FOOTSWITCH (SPACE or tap) to bypass. Press B to A/B the TS voice against a plain clip (same drive, no tone-shaping) — the mid hump appears and disappears. GarageBand keybed (ASDFGHJKL / WETYUO) plays the guitar, Z/X octave, R toggles a chuggy auto-riff so you hear the grind, knobs drag or wheel, ? for help."
}
de:meta */
#include "studio.h"
#include "pointer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── TUBE SCREAMER ────────────────────────────────────────────────────────────
// The Ibanez TS808 (1979): the overdrive everyone clones. Its character is the
// FILTERING around a mild soft-asymmetric clip, not the clip itself —
//   • a highpass into the clipper so the BASS stays clean/tight (no flub)
//   • a post lowpass (the TONE knob) that tames fizz
//   • clean lows + clipped mids + rolled highs = the famous MID HUMP.
// Here that's drive_voice(DRIVE_VOICE_TS): the clip is our DRIVE_ASYM (always
// had it), the TS voice adds the tone-shaping. Third gated hardware voicing,
// sibling of aquapuss (BBD delay) + springtank (spring reverb).
//
//   OVERDRIVE  how hard the mids clip (drive_insert amount)
//   TONE       drive_voice() tone — dark..bright post-LP
//   LEVEL      output volume (instrument_level)
//   SPACE/tap  stomp the footswitch (bypass)
//   B          A/B the TS voice vs a plain clip (the mid hump on/off)
//   R          chuggy auto-riff · KEYS play · Z/X octave · ? help

#define SLOT 8
#define STEPS 16

enum { K_DRIVE, K_TONE, K_LEVEL, NK };
static const char *KNAME[NK] = { "DRIVE", "TONE", "LEVEL" };
static int knob[NK] = { 55, 50, 70 };

static int   base = 40;            // low E-ish — chug territory
static bool  bypass = false;       // the footswitch
static bool  ts     = true;        // B: TS voice vs plain clip
static bool  riff   = true;        // R: the chug
static bool  show_help;
static int   last16 = -1;

enum { PTR_IDLE, PTR_KNOB, PTR_KEY };
typedef struct { int id, mode, k, lastY, semi; } Ptr;
static Ptr ptr[PTR_MAX];
static int key_flash[32];

static float drive_x(void) { return knob[K_DRIVE] / 100.0f; }
static float tone_x(void)  { return knob[K_TONE]  / 100.0f; }
static float level_x(void) { return 0.3f + knob[K_LEVEL] / 100.0f * 0.7f; }   // 0.3..1.0

static void apply_pedal(void) {
    drive_insert(drive_x(), DRIVE_ASYM, bypass ? 0.0f : 1.0f);   // mix 0 = true bypass
    drive_voice(ts ? DRIVE_VOICE_TS : DRIVE_VOICE_NONE, tone_x());
    instrument_level(SLOT, level_x());
}

// ── GarageBand keybed ─────────────────────────────────────────────────────────
static const struct { char k; int semi; } KMAP[] = {
    { 'A', 0 }, { 'S', 2 }, { 'D', 4 }, { 'F', 5 }, { 'G', 7 }, { 'H', 9 },
    { 'J', 11 }, { 'K', 12 }, { 'L', 14 },
    { 'W', 1 }, { 'E', 3 }, { 'T', 6 }, { 'Y', 8 }, { 'U', 10 }, { 'O', 13 },
};
#define NKMAP ((int)(sizeof KMAP / sizeof KMAP[0]))
#define NWHITE 9
static const int  W_SEMI[NWHITE] = { 0, 2, 4, 5, 7, 9, 11, 12, 14 };
static const char WLBL[NWHITE + 1] = "ASDFGHJKL";
#define NBLACK 6
static const int  B_SEMI[NBLACK]  = { 1, 3, 6, 8, 10, 13 };
static const int  B_AFTER[NBLACK] = { 0, 1, 3, 4, 5, 7 };
static const char BLBL[NBLACK + 1] = "WETYUO";

static void play_note(int midi) { hit(midi, SLOT, 6, 240); }

// chuggy auto-riff — low root chug with a couple moves (minor pentatonic from base)
static const char RIFF[STEPS + 1] = "0.0.0.5.0.0.3.5.";
static const int  POOL[6] = { 0, 3, 5, 7, 10, 12 };

#define KBX 10
#define KBY 138
#define KBW 33
#define KBH 54
static int key_at(int x, int y) {
    if (y < KBY || y >= KBY + KBH) return -1;
    for (int j = 0; j < NBLACK; j++) {
        int bx = KBX + B_AFTER[j] * KBW + KBW - 8;
        if (y < KBY + 32 && x >= bx && x < bx + 16) return B_SEMI[j];
    }
    if (x >= KBX && x < KBX + NWHITE * KBW) return W_SEMI[(x - KBX) / KBW];
    return -1;
}
static void flash_semi(int s) {
    for (int k = 0; k < NKMAP; k++) if (KMAP[k].semi == s) { key_flash[k] = 6; break; }
}

void init(void) {
    bpm(100);
    PTR_CLEAR(ptr);
    instrument(SLOT, INSTR_GUITAR, 1, 0, 7, 500);
    instrument_harmonics(SLOT, 0.55f);
    instrument_timbre(SLOT, 0.80f);
    instrument_morph(SLOT, 0.15f);
    int chain[] = { FX_DRIVE };
    fx_order(0, chain, 1);
    apply_pedal();
}

static const int KX[NK] = { 58, 160, 262 };
#define KY 44
#define KR 15

void update(void) {
    if (keyp(KEY_SPACE)) { bypass = !bypass; apply_pedal(); }
    if (keyp('B'))       { ts = !ts; apply_pedal(); }
    if (keyp('R'))       { riff = !riff; last16 = -1; }
    if (keyp('X'))      { base += 12; if (base > 76) base = 76; }
    if (keyp('Z'))      { base -= 12; if (base < 28) base = 28; }

    for (int i = 0; i < NKMAP; i++)
        if (keyp(KMAP[i].k)) { play_note(base + KMAP[i].semi); key_flash[i] = 6; }
    for (int i = 0; i < NKMAP; i++) if (key_flash[i] > 0) key_flash[i]--;

    int mx = mouse_x(), my = mouse_y();

    if (show_help) {
        if (keyp('/') || tapp(0, 0, 320, 200)) show_help = false;
        goto clock;
    }
    if (keyp('/') || tapp(304, 0, 16, 20)) show_help = true;
    if (tapp(196, 0, 30, 20)) { base -= 12; if (base < 28) base = 28; }
    if (tapp(226, 0, 28, 20)) { base += 12; if (base > 76) base = 76; }
    if (tapp(256, 0, 40, 20)) { riff = !riff; last16 = -1; }
    if (tapp(220, 76, 90, 54)) { bypass = !bypass; apply_pedal(); }   // footswitch box
    if (tapp(112, 78, 94, 12)) { ts = !ts; apply_pedal(); }           // TS-voice A/B (in the REPEATS box row)

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh; Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;
        if (fresh) {
            *p = (Ptr){ id, PTR_IDLE, -1, ty, -1 };
            for (int k = 0; k < NK; k++) {
                int dx = tx - KX[k], dy = ty - KY;
                if (dx * dx + dy * dy <= (KR + 4) * (KR + 4)) { p->mode = PTR_KNOB; p->k = k; }
            }
            if (p->mode == PTR_IDLE) {
                int s = key_at(tx, ty);
                if (s >= 0) { play_note(base + s); flash_semi(s); p->mode = PTR_KEY; p->semi = s; }
            }
        } else if (p->mode == PTR_KNOB) {
            if (ty != p->lastY) {
                knob[p->k] += (p->lastY - ty) * 2;
                if (knob[p->k] < 0) knob[p->k] = 0;
                if (knob[p->k] > 100) knob[p->k] = 100;
                apply_pedal();
            }
        } else if (p->mode == PTR_KEY) {
            int s = key_at(tx, ty);
            if (s >= 0 && s != p->semi) { play_note(base + s); flash_semi(s); p->semi = s; }
        }
        p->lastY = ty;
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    float wh = mouse_wheel();
    if (wh != 0.0f)
        for (int k = 0; k < NK; k++) {
            int dx = mx - KX[k], dy = my - KY;
            if (dx * dx + dy * dy <= (KR + 4) * (KR + 4)) {
                knob[k] += wh > 0 ? 4 : -4;
                if (knob[k] < 0) knob[k] = 0;
                if (knob[k] > 100) knob[k] = 100;
                apply_pedal();
            }
        }

clock:
    if (!riff) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int s = s16 & 15;
    if (s16 != last16) {
        last16 = s16;
        char c = RIFF[s];
        if (c != '.') play_note(base + POOL[c - '0']);
    }
}

void draw(void) {
    char buf[40];
    cls(CLR_DARK_GREEN);                                  // the iconic TS green
    rectfill(0, 0, 320, 20, CLR_BROWNISH_BLACK);
    print("IBANEZ  TUBE SCREAMER", 8, 7, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("<", 199, 9, CLR_BROWNISH_BLACK);
    print(">", 249, 9, CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);
    sprintf(buf, "OCT C%d", base / 12 - 1);
    print(buf, 206, 7, CLR_LIGHT_GREY);
    print(riff ? "RIFF>" : "RIFF#", 256, 7, riff ? CLR_LIME_GREEN : CLR_DARK_GREY);
    print("?", 310, 7, CLR_LIGHT_GREY);
    font(FONT_SMALL);
    print("OVERDRIVE PRO", 8, 25, CLR_MEDIUM_GREEN);
    font(FONT_NORMAL);

    for (int k = 0; k < NK; k++) {
        circfill(KX[k], KY, KR, CLR_BROWNISH_BLACK);
        circ(KX[k], KY, KR, CLR_LIGHT_PEACH);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], KY, KX[k] + (int)(cosf(a) * (KR - 3)), KY + (int)(sinf(a) * (KR - 3)), CLR_WHITE);
        print(KNAME[k], KX[k] - (int)strlen(KNAME[k]) * 4, KY + KR + 5, CLR_BLACK);
    }

    // mid-hump / voice status box
    rectfill(10, 76, 200, 54, CLR_BROWNISH_BLACK);
    rect(10, 76, 200, 54, CLR_MEDIUM_GREEN);
    print("VOICE", 16, 81, CLR_LIME_GREEN);
    font(FONT_SMALL);                                     // tappable A/B
    print(ts ? "TS MID-HUMP: ON" : "PLAIN CLIP: ON", 114, 82, ts ? CLR_LIME_GREEN : CLR_DARK_GREY);
    font(FONT_NORMAL);
    // a little EQ-curve cartoon: flat clip (line) vs a hump when TS
    for (int x = 0; x < 180; x++) {
        float t = x / 180.0f;
        int y;
        if (ts) { float h = expf(-(t - 0.45f) * (t - 0.45f) * 26.0f); y = 118 - (int)(h * 26.0f); }  // mid hump
        else    { y = 108; }                                                                          // flat
        pset(20 + x, y, CLR_LIME_GREEN);
    }

    // footswitch
    rectfill(220, 76, 90, 54, CLR_BROWNISH_BLACK);
    rect(220, 76, 90, 54, CLR_MEDIUM_GREEN);
    circfill(233, 88, 4, bypass ? CLR_DARKER_GREY : CLR_RED);
    circ(233, 88, 4, CLR_LIGHT_GREY);
    print(bypass ? "BYPASS" : "ON", 244, 84, bypass ? CLR_DARK_GREY : CLR_LIGHT_PEACH);
    circfill(265, 110, 13, CLR_LIGHT_GREY);
    circfill(265, 110, 10, CLR_MEDIUM_GREY);
    font(FONT_SMALL);
    print("STOMP", 250, 108, CLR_BROWNISH_BLACK);
    font(FONT_NORMAL);

    // keybed
    for (int i = 0; i < NWHITE; i++) {
        int x = KBX + i * KBW; bool lit = false;
        for (int k = 0; k < NKMAP; k++) if (key_flash[k] > 0 && KMAP[k].semi == W_SEMI[i]) lit = true;
        rectfill(x, KBY, KBW - 2, KBH, lit ? CLR_YELLOW : CLR_WHITE);
        sprintf(buf, "%c", WLBL[i]); print(buf, x + KBW / 2 - 4, KBY + KBH - 12, CLR_MEDIUM_GREY);
    }
    for (int j = 0; j < NBLACK; j++) {
        int x = KBX + B_AFTER[j] * KBW + KBW - 8; bool lit = false;
        for (int k = 0; k < NKMAP; k++) if (key_flash[k] > 0 && KMAP[k].semi == B_SEMI[j]) lit = true;
        rectfill(x, KBY, 16, 32, lit ? CLR_YELLOW : CLR_BLACK);
        sprintf(buf, "%c", BLBL[j]); print(buf, x + 5, KBY + 22, CLR_MEDIUM_GREY);
    }

    if (show_help) {
        rectfill(30, 26, 260, 150, CLR_BLACK);
        rect(30, 26, 260, 150, CLR_LIGHT_GREY);
        print("TUBE SCREAMER", 104, 34, CLR_YELLOW);
        static const char *HL[] = {
            "KEYS      PLAY THE GUITAR",
            "OVERDRIVE HOW HARD THE MIDS CLIP",
            "TONE      DARK .. BRIGHT",
            "LEVEL     OUTPUT VOLUME",
            "SPACE     STOMP (BYPASS)",
            "B         A/B MID-HUMP vs PLAIN",
            "R         CHUG AUTO-RIFF",
            "Z / X     OCTAVE DOWN / UP",
            "/         CLOSE THIS",
        };
        for (int i = 0; i < 9; i++)
            print(HL[i], 42, 48 + i * 13, i < 4 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
