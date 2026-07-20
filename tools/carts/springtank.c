/* de:meta
{
  "slug": "springtank",
  "title": "spring tank",
  "status": "active",
  "created": "2026-07-20",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "analog-voice-modeling"
  ],
  "lineage": "Showcase + tuning bench for reverb_spring() — the spring-tank voicing on the engine reverb (dispersion 'boing' + mid band-limit). Sibling of aquapuss (the BBD delay voicing): both take a generic effect and add a gated famous-hardware voice.",
  "homage": "The Accutronics/Fender-style spring reverb tank (surf, dub, drip).",
  "description": "A spring reverb you can hear three ways — one cart, three SCENES (1/2/3 or tap the tabs): KICK the tank — two ways to excite the spring and hear its raw boing: SPACE/keys fire a low mechanical THUNK (like knocking the tank), and N (or the NOISE pad) fires a broadband noise burst — SURF (a clean twangy guitar line drenched in spring — the Dick Dale sound), and DUB (deep off-beat organ skanks into a long spring). All three share the engine's reverb with reverb_spring() on: transients disperse (highs chirp ahead → the boing) and the tone narrows to the metallic spring band. Press B to A/B the spring against a clean digital reverb, drag the SPRING amount + SIZE knobs, R toggles the auto phrase (SURF/DUB), ZX octave, GarageBand keybed (ASDFGHJKL / WETYUO) plays into the current scene, ? for help."
}
de:meta */
#include "studio.h"
#include "pointer.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── SPRING TANK ────────────────────────────────────────────────────────────
// Showcase + tuning bench for reverb_spring(): the spring-tank voicing on the
// engine reverb. Three SCENES so you hear the spring in its three homes:
//   KICK  SPACE/tap fires a broadband thunk into the spring — the raw boing.
//   SURF  a clean twangy guitar line, spring-drenched (the surf/Dick Dale drip).
//   DUB   deep off-beat organ skanks into a long spring.
// B = A/B spring vs clean digital reverb. SPRING/SIZE knobs. R = auto phrase.

enum { SC_KICK, SC_SURF, SC_DUB, NSCENE };
static const char *SCNAME[NSCENE] = { "KICK", "SURF", "DUB" };
#define SLOT_NOISE 6      // KICK: broadband noise-burst exciter
#define SLOT_THUNK 9      // KICK: low mechanical clonk (knock the tank)
#define SLOT_SURF  7
#define SLOT_DUB   8
static const float SC_SIZE[NSCENE] = { 0.45f, 0.60f, 0.88f };   // reverb size default per scene

static int   scene = SC_KICK;
static bool  spring = true;        // B: the spring voice on/off (A/B vs clean reverb)
static bool  auton  = true;        // R: the self-playing phrase (SURF/DUB)
static bool  show_help;
static int   base = 60;            // keybed octave root (C4)
static int   last16 = -1;
static float wob = 0.0f;           // spring vibration (visual), spikes on input (named wob — shake() is a built-in)

// knobs
enum { K_SPRING, K_SIZE, K_BOING, NK };
static const char *KNAME[NK] = { "SPRING", "SIZE", "BOING" };
static int knob[NK] = { 90, 45, 60 };  // SPRING 0.90, SIZE per scene, BOING 60 (~disp 0.62 default)

enum { PTR_IDLE, PTR_KNOB, PTR_KEY };
typedef struct { int id, mode, k, lastY, semi; } Ptr;
static Ptr ptr[PTR_MAX];

static float spring_x(void) { return knob[K_SPRING] / 100.0f; }
static float size_x(void)   { return knob[K_SIZE]   / 100.0f; }
static float boing_x(void)  { return knob[K_BOING]  / 100.0f; }

static void apply_reverb(void) {
    reverb(size_x(), 0.35f);
    reverb_spring(spring ? spring_x() : 0.0f);   // 0 = clean digital reverb (A/B)
    reverb_spring_tone(boing_x());               // the boing character — ride it live
}

// ── GarageBand keybed (the studio standard) ─────────────────────────────────
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
static int key_flash[NKMAP];

// ── the three scenes' voices + actions ──────────────────────────────────────
static void kick_thunk(int midi) {             // a low mechanical CLONK — knocking the tank
    hit(midi, SLOT_THUNK, 7, 150);
    wob = 1.0f;
}
static void kick_noise(void) {                 // a broadband NOISE burst — the raw full-range exciter
    hit(48, SLOT_NOISE, 6, 110);
    wob = 1.0f;
}
static void play_surf(int midi) {
    hit(midi, SLOT_SURF, 6, 380);
    wob = 0.7f;
}
static void dub_stab(int root) {               // a short minor stab into the long spring
    hit(root,      SLOT_DUB, 6, 170);
    hit(root + 7,  SLOT_DUB, 6, 170);
    hit(root + 12, SLOT_DUB, 5, 170);
    wob = 0.8f;
}
// play a note into the CURRENT scene (keys / on-screen keybed) — KICK plays a PITCHED thunk
static void play_scene(int midi) {
    if (scene == SC_KICK)      kick_thunk(midi);
    else if (scene == SC_SURF) play_surf(midi);
    else                       dub_stab(midi);
}

// auto phrases (SURF/DUB) — '.' rest; SURF digits index a pentatonic pool, DUB 'X' = stab
static const char SURFP[17] = "0..5.7..5..3.2..";
static const int  SURFPOOL[8] = { 0, 3, 5, 7, 10, 12, 7, 5 };
static const char DUBP[17]  = "..X...X...X..X..";

static void enter_scene(int s) {
    scene = s;
    knob[K_SIZE] = (int)(SC_SIZE[s] * 100.0f);
    apply_reverb();
    last16 = -1;
}

void init(void) {
    bpm(96);
    PTR_CLEAR(ptr);
    instrument(SLOT_NOISE, INSTR_NOISE, 1, 0, 7, 110);            // the noise-burst exciter
    instrument(SLOT_THUNK, INSTR_MEMBRANE, 1, 0, 7, 150);         // the mechanical clonk (knock the tank)
    instrument(SLOT_SURF, INSTR_GUITAR, 1, 0, 7, 700);            // clean surf guitar
    instrument_timbre(SLOT_SURF, 0.85f);
    instrument(SLOT_DUB, INSTR_ORGAN, 1, 0, 7, 180);              // short organ skank
    instrument_reverb(SLOT_NOISE, 0.95f);
    instrument_reverb(SLOT_THUNK, 0.95f);
    instrument_reverb(SLOT_SURF, 0.62f);
    instrument_reverb(SLOT_DUB, 0.72f);
    enter_scene(SC_KICK);
}

// ── keybed geometry ─────────────────────────────────────────────────────────
#define KBX 10
#define KBY 140
#define KBW 33
#define KBH 52
static int key_at(int x, int y) {
    if (y < KBY || y >= KBY + KBH) return -1;
    for (int j = 0; j < NBLACK; j++) {
        int bx = KBX + B_AFTER[j] * KBW + KBW - 8;
        if (y < KBY + 30 && x >= bx && x < bx + 16) return B_SEMI[j];
    }
    if (x >= KBX && x < KBX + NWHITE * KBW) return W_SEMI[(x - KBX) / KBW];
    return -1;
}
static void flash_semi(int s) {
    for (int k = 0; k < NKMAP; k++) if (KMAP[k].semi == s) { key_flash[k] = 6; break; }
}

// knob layout (right of the tank)
static const int KX[NK] = { 200, 240, 280 };
#define KY 40
#define KR 12

void update(void) {
    if (keyp('1')) enter_scene(SC_KICK);
    if (keyp('2')) enter_scene(SC_SURF);
    if (keyp('3')) enter_scene(SC_DUB);
    if (keyp('B')) { spring = !spring; apply_reverb(); }
    if (keyp('R')) { auton = !auton; last16 = -1; }
    if (keyp('X')) { base += 12; if (base > 84) base = 84; }
    if (keyp('Z')) { base -= 12; if (base < 36) base = 36; }
    if (keyp(KEY_SPACE)) { if (scene == SC_KICK) kick_thunk(40); else play_scene(base); }  // SPACE: KICK = deep clonk
    if (keyp('N')) kick_noise();                                                            // N: the noise-burst exciter

    for (int i = 0; i < NKMAP; i++)
        if (keyp(KMAP[i].k)) { play_scene(base + KMAP[i].semi); key_flash[i] = 6; }
    for (int i = 0; i < NKMAP; i++) if (key_flash[i] > 0) key_flash[i]--;

    int mx = mouse_x(), my = mouse_y();

    if (show_help) {
        if (keyp('/') || tapp(0, 0, 320, 200)) show_help = false;
        goto clock;
    }
    if (keyp('/') || tapp(304, 0, 16, 18)) show_help = true;

    // scene tabs (tappable) + A/B tap
    for (int s = 0; s < NSCENE; s++)
        if (tapp(96 + s * 40, 0, 38, 18)) enter_scene(s);
    if (tapp(226, 0, 30, 18)) { spring = !spring; apply_reverb(); }
    if (scene == SC_KICK) {                                    // touch: NOISE pad, else tap the tank = THUNK
        if (tapp(140, 30, 44, 14)) kick_noise();
        else if (tapp(8, 46, 180, 78)) kick_thunk(40);
    }

    // touch: knobs (drag) + on-screen keys
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
                if (s >= 0) { play_scene(base + s); flash_semi(s); p->mode = PTR_KEY; p->semi = s; }
            }
        } else if (p->mode == PTR_KNOB) {
            if (ty != p->lastY) {
                knob[p->k] += (p->lastY - ty) * 2;
                if (knob[p->k] < 0) knob[p->k] = 0;
                if (knob[p->k] > 100) knob[p->k] = 100;
                apply_reverb();
            }
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
                apply_reverb();
            }
        }

clock:
    wob *= 0.90f;
    if (!auton || scene == SC_KICK) return;
    int s16 = beat() * 4 + (int)(beat_pos() * 4.0f);
    int s = s16 & 15;
    if (s16 != last16) {
        last16 = s16;
        if (scene == SC_SURF) {
            char c = SURFP[s];
            if (c != '.') play_surf(base + SURFPOOL[c - '0']);
        } else {  // DUB
            if (DUBP[s] == 'X') dub_stab(base - 12);
        }
    }
}

void draw(void) {
    char buf[40];
    cls(CLR_DARKER_BLUE);
    rectfill(0, 0, 320, 20, CLR_BROWNISH_BLACK);
    print("SPRING TANK", 6, 7, CLR_LIGHT_PEACH);
    // scene tabs
    for (int s = 0; s < NSCENE; s++) {
        bool on = (s == scene);
        rectfill(96 + s * 40, 2, 36, 15, on ? CLR_BLUE : CLR_DARKER_BLUE);
        print(SCNAME[s], 100 + s * 40, 6, on ? CLR_WHITE : CLR_DARK_GREY);
    }
    print(spring ? "SPR>" : "SPR#", 228, 7, spring ? CLR_GREEN : CLR_DARK_GREY);
    print("?", 310, 7, CLR_LIGHT_GREY);

    // knobs
    for (int k = 0; k < NK; k++) {
        circfill(KX[k], KY, KR, CLR_BROWNISH_BLACK);
        circ(KX[k], KY, KR, CLR_LIGHT_GREY);
        float a = (-135.0f + 270.0f * knob[k] / 100.0f) * 0.0174533f - 1.5708f;
        line(KX[k], KY, KX[k] + (int)(cosf(a) * (KR - 3)), KY + (int)(sinf(a) * (KR - 3)), CLR_WHITE);
        font(FONT_SMALL);
        print(KNAME[k], KX[k] - (int)strlen(KNAME[k]) * 2, KY + KR + 4, CLR_LIGHT_PEACH);
        font(FONT_NORMAL);
    }
    for (int k = 0; k < NK; k++) { sprintf(buf, "%d", knob[k]); print(buf, KX[k] - (int)strlen(buf) * 2, KY + KR + 13, CLR_DARK_GREY); }

    // ── the tank: three springs that wob with recent input ────────────────
    rectfill(8, 28, 180, 96, CLR_BROWNISH_BLACK);
    rect(8, 28, 180, 96, CLR_BLUE_GREEN);
    for (int sp = 0; sp < 3; sp++) {
        int cy = 50 + sp * 28;
        float amp = 2.0f + wob * 10.0f;
        int px = 20, py = cy;
        for (int x = 0; x <= 150; x += 3) {
            int y = cy + (int)(sinf(x * 0.6f + sp * 1.3f) * amp * (spring ? 1.0f : 0.35f));
            if (x > 0) line(px, py, 20 + x, y, wob > 0.4f ? CLR_LIGHT_GREY : CLR_DARK_GREY);
            px = 20 + x; py = y;
        }
    }
    // scene hint
    const char *hint = scene == SC_KICK ? "SPACE/KEYS=THUNK   N=NOISE"
                     : scene == SC_SURF ? "SURF LINE  (R=AUTO, KEYS PLAY)"
                     :                     "DUB SKANK  (R=AUTO, KEYS PLAY)";
    print(hint, 14, 112, CLR_BLUE);
    if (scene == SC_KICK) {                                    // the NOISE-burst pad (tap)
        rectfill(140, 30, 44, 14, CLR_BROWNISH_BLACK);
        rect(140, 30, 44, 14, CLR_BLUE_GREEN);
        print("NOISE", 148, 33, CLR_LIGHT_GREY);
    }

    // ── keybed ───────────────────────────────────────────────────────────────
    for (int i = 0; i < NWHITE; i++) {
        int x = KBX + i * KBW; bool lit = false;
        for (int k = 0; k < NKMAP; k++) if (key_flash[k] > 0 && KMAP[k].semi == W_SEMI[i]) lit = true;
        rectfill(x, KBY, KBW - 2, KBH, lit ? CLR_YELLOW : CLR_WHITE);
        sprintf(buf, "%c", WLBL[i]); print(buf, x + KBW / 2 - 4, KBY + KBH - 11, CLR_MEDIUM_GREY);
    }
    for (int j = 0; j < NBLACK; j++) {
        int x = KBX + B_AFTER[j] * KBW + KBW - 8; bool lit = false;
        for (int k = 0; k < NKMAP; k++) if (key_flash[k] > 0 && KMAP[k].semi == B_SEMI[j]) lit = true;
        rectfill(x, KBY, 16, 30, lit ? CLR_YELLOW : CLR_BLACK);
        sprintf(buf, "%c", BLBL[j]); print(buf, x + 5, KBY + 20, CLR_MEDIUM_GREY);
    }

    if (show_help) {
        rectfill(28, 24, 264, 154, CLR_BLACK);
        rect(28, 24, 264, 154, CLR_LIGHT_GREY);
        print("SPRING TANK", 112, 32, CLR_YELLOW);
        static const char *HL[] = {
            "1 2 3     KICK / SURF / DUB SCENE",
            "SPACE     SCENE ACTION (KICK=THUNK)",
            "N         NOISE BURST INTO THE TANK",
            "KEYS      PLAY INTO THE SCENE",
            "SPRING    wet amount (0=clean reverb)",
            "SIZE      reverb decay length",
            "BOING     dispersion character (twang)",
            "B         A/B SPRING vs CLEAN REVERB",
            "R         AUTO PHRASE (SURF/DUB)",
            "Z / X     OCTAVE DOWN / UP",
            "/         CLOSE THIS",
        };
        for (int i = 0; i < 11; i++)
            print(HL[i], 40, 42 + i * 12, i < 4 ? CLR_WHITE : CLR_LIGHT_PEACH);
    }
}
