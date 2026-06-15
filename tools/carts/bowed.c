// bowed — INSTR_BOWED showcase: seven strings you BOW by RUBBING, plus pizzicato. The last
// modeled string engine (§8.5 step 9), played the way a bow actually works — friction.
//
// The bowed string (Smith/McIntyre stick-slip waveguide): a bow drags across the string, the
// rosin grips then slips, and that friction drives a SELF-OSCILLATING loop. STEP-0
// (tools/navkit-bowsweep.c) mapped the Schelleng "wedge" of stable bowing; the three macros are
// pinned inside it:
//   harmonics = bow position (0 sul ponticello, near the bridge, bright/edgy .. 1 sul tasto, soft)
//   timbre    = bow pressure (0 light & clean, a singing leaning-sawtooth .. 1 heavy & scratchy)
//   morph     = bow speed/swell (0 gentle .. 1 digging in, louder and cleaner)
//
// THE FEEL: you don't just press a string — you RUB it. Drag your finger/mouse back and forth
// across a string and it speaks; the harder/faster you rub, the louder and fuller it sings.
// Stop moving and the bow rests on the string (silent). It's friction, like rubbing a drumhead.
// A quick TAP (no rub) instead PLUCKS the string — pizzicato. This is the SAME bowed string: a
// second INSTR_BOWED slot flagged pizz (instrument_mode(slot,0,1)), where the engine seeds the waveguide
// with a pluck and switches the bow friction off, so the very same string + body rings down. Arco
// and pizz differ only in how energy enters — exactly as on a real violin.
//
// controls: touch/mouse — RUB a string back-and-forth to bow it · quick TAP a string = pizzicato
//                          drag a string sideways while bowing bends nothing; drag a slider to set macros
//           keyboard    — HOLD A S D F G H J = bow (steady) · tap Q W E R T Y U = pizzicato
//           1..6 preset (violin/viola/cello/pontic./tasto/tremolo) · LEFT/RIGHT+UP/DOWN knobs
//           Z/X octave · M autoplay arco on/off  ('P' is the runtime pause overlay)
//
// MULTITOUCH: every finger is its own bow — rub a drone with one hand, pluck a melody with the other.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>

#define I_STR 5    // the bowed (arco) engine
#define I_PIZ 6    // pizzicato = INSTR_BOWED in pizz mode — the SAME string + body, plucked not bowed
#define NSTR  7

static const char STRKEY[NSTR]  = { 'A','S','D','F','G','H','J' };
static const char PIZKEY[NSTR]  = { 'Q','W','E','R','T','Y','U' };
static const char *KNOB_NAME[3] = { "position", "pressure", "speed" };

// orchestral presets = baked (harmonics, timbre, morph) triples — position / pressure / speed
#define NPRESET 6
static const char *PRESET_NAME[NPRESET] = { "violin","viola","cello","pontic.","tasto","tremolo" };
static const float PRESET[NPRESET][3] = {
    { 0.45f, 0.30f, 0.70f },   // violin    — mid bow, light clean pressure
    { 0.50f, 0.38f, 0.60f },   // viola     — a touch more weight
    { 0.55f, 0.42f, 0.55f },   // cello     — over the fingerboard, fuller pressure
    { 0.10f, 0.55f, 0.80f },   // ponticello— near the bridge, heavy: glassy, edgy, scratchy
    { 0.90f, 0.18f, 0.45f },   // tasto     — far over the board, feather pressure: flute-soft
    { 0.40f, 0.35f, 0.85f },   // tremolo   — fast, hard bow speed
};

static int   midi_of[NSTR];
static int   hnd[NSTR];        // live held bow handle per string, -1 = not bowing
static float rub[NSTR];        // smoothed rub intensity 0..1 (drives loudness + swell)
static float kbd_bow[NSTR];    // 1 while a keyboard key holds this string (steady bow), else 0
static float amp[NSTR];        // visual string vibration
static float vib_ph[NSTR];
static float knob[3] = { 0.45f, 0.30f, 0.70f };   // the three macros (starts at "violin")
static int   sel = 0;
static int   preset = 0;
static bool  autoplay = true;
static int   apos = 0;
static float atimer = 0, arub = 0;
static int   goct = 3;

static void build_strings(void) {
    for (int s = 0; s < NSTR; s++) midi_of[s] = degree(SCALE_PENTA, goct, s + 2);
}

static void apply_knobs(void) {
    instrument_harmonics(I_STR, knob[0]);
    instrument_timbre(I_STR, knob[1]);
    instrument_morph(I_STR, knob[2]);
    for (int s = 0; s < NSTR; s++) if (hnd[s] >= 0) {
        note_harmonics(hnd[s], knob[0]);
        note_timbre(hnd[s], knob[1]);
        note_morph(hnd[s], knob[2]);
    }
}

// start a bow voice silent (vol 0) — the RUB brings it up; keeps the bow "resting" until you move
static void bow_on(int s) {
    if (hnd[s] < 0) { hnd[s] = note_on(midi_of[s], I_STR, 0); vib_ph[s] = 0.0f; }
}
static void bow_off(int s) {
    if (hnd[s] >= 0) { note_off(hnd[s]); hnd[s] = -1; }
    rub[s] = 0.0f;
}
static void pizz(int s, int vol) {                 // a plucked note on the guitar-pizz voice
    bow_off(s);                                    // pizz cancels any bow on the same string
    hit(midi_of[s], I_PIZ, vol, 800);              // long gate — the plucked string rings down on its own
    amp[s] = 1.0f; vib_ph[s] = 0.0f;
}

static void load_preset(int p) {
    preset  = p;
    knob[0] = PRESET[p][0]; knob[1] = PRESET[p][1]; knob[2] = PRESET[p][2];
    apply_knobs();
}

enum { PTR_IDLE, PTR_KNOB, PTR_STR };
typedef struct { int id, mode, k, s, prevX, prevY; float held_t, moved; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

#define KNOB_W    96
#define KNOB_TOP  (SCREEN_H - 28)
#define KNOB_X(k) (16 + (k) * 104)

#define STR_X0 40
#define STR_GAP ((SCREEN_W - 80) / (NSTR - 1))
#define STR_Y0  48
#define STR_Y1  (SCREEN_H - 46)
static int str_x(int s) { return STR_X0 + s * STR_GAP; }

void init(void) {
    instrument(I_STR, INSTR_BOWED, 80, 0, 7, 300);    // quick-ish swell, short release (rub controls level)
    // PIZZICATO = the SAME bowed string, plucked instead of bowed: INSTR_BOWED with the pizz flag
    // (eng_p[0]>=0.5). The engine seeds the waveguide with a pluck and bypasses the bow friction, so
    // it rings down through the same string + body — not a separate instrument. Near-instant attack
    // (the pluck IS the attack); the string decays on its own, so give it a long gate.
    instrument(I_PIZ, INSTR_BOWED, 1, 0, 7, 300);
    instrument_harmonics(I_PIZ, 0.30f);               // pluck position (sets the pluck's spectrum)
    instrument_timbre(I_PIZ, 0.42f);                  // pluck brightness — warm finger, not a bright pick
    instrument_mode(I_PIZ, MODE_BOW_PIZZ, 1.0f);                         // FLAG this slot pizzicato
    for (int s = 0; s < NSTR; s++) hnd[s] = -1;
    PTR_CLEAR(ptr);
    build_strings();
    apply_knobs();
    bpm(84);
    atimer = 1.2f;                         // fire the first arco note on frame 1, no dead air
    amp[1] = 0.7f; amp[3] = 1.0f; amp[5] = 0.5f;   // a lively first frame for the thumbnail
}

void update(void) {
    // keyboard bow (steady — no rub motion, so drive it from the speed knob) + pizz row
    for (int s = 0; s < NSTR; s++) {
        if (keyp(STRKEY[s])) { bow_on(s); kbd_bow[s] = 1.0f; }
        if (keyr(STRKEY[s])) { kbd_bow[s] = 0.0f; bow_off(s); }
        if (keyp(PIZKEY[s])) pizz(s, 6);
    }

    for (int p = 0; p < NPRESET; p++)
        if (keyp('1' + p)) { load_preset(p); pizz(3, 6); }

    if (keyp(KEY_LEFT))  sel = (sel + 2) % 3;
    if (keyp(KEY_RIGHT)) sel = (sel + 1) % 3;
    if (key(KEY_UP) || key(KEY_DOWN)) {
        knob[sel] = clamp(knob[sel] + (key(KEY_UP) ? 0.012f : -0.012f), 0.0f, 1.0f);
        apply_knobs();
    }
    if (keyp('M')) autoplay = !autoplay;
    if (keyp('Z') && goct > 1) { goct--; build_strings(); }
    if (keyp('X') && goct < 6) { goct++; build_strings(); }

    // touch: each finger rubs a string (bow), taps a string (pizz), or drags a slider
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                              // pool full (>PTR_MAX fingers)
        if (fresh) {                                   // finger just landed
            *p = (Ptr){ id, PTR_IDLE, -1, -1, tx, ty, 0.0f, 0.0f };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) { autoplay = !autoplay; continue; }
            for (int k = 0; k < 3; k++)
                if (point_in_box(tx, ty, KNOB_X(k) - 2, KNOB_TOP - 6, KNOB_W + 4, 18)) {
                    p->mode = PTR_KNOB; p->k = k; sel = k; break;
                }
            if (p->mode == PTR_IDLE) {                 // land on the nearest string → start a bow voice
                int best = -1, bd = 99999;
                for (int s = 0; s < NSTR; s++) { int dx = tx - str_x(s); if (dx < 0) dx = -dx; if (dx < bd) { bd = dx; best = s; } }
                if (best >= 0 && bd < STR_GAP / 2 + 8 && ty > STR_Y0 - 8 && ty < STR_Y1 + 8) {
                    p->mode = PTR_STR; p->s = best; bow_on(best);
                }
            }
        } else {                                       // finger held
            float mv = (float)(abs(tx - p->prevX) + abs(ty - p->prevY));
            p->held_t += dt();
            p->moved  += mv;
            if (p->mode == PTR_KNOB) {
                knob[p->k] = clamp((tx - KNOB_X(p->k)) / (float)KNOB_W, 0.0f, 1.0f);
                apply_knobs();
            } else if (p->mode == PTR_STR && p->s >= 0 && hnd[p->s] >= 0) {
                rub[p->s] += mv * 0.009f;   // RUB: each stroke ADDS energy — keep rubbing and it builds
            }
        }
        p->prevX = tx; p->prevY = ty;
    }
    // fingers that lifted: a quick low-movement press = a TAP → pizzicato; otherwise lift the bow
    for (int j = 0; j < PTR_MAX; j++) if (ptr[j].id != PTR_NONE) {
        bool live = false;
        for (int i = 0; i < touch_count(); i++) if (touch_id(i) == ptr[j].id) { live = true; break; }
        if (!live) {
            if (ptr[j].mode == PTR_STR && ptr[j].s >= 0) {
                bool tap = (ptr[j].held_t < 0.22f && ptr[j].moved < 14.0f);
                int s = ptr[j].s;
                bow_off(s);
                if (tap) pizz(s, 6);                   // a flick = pizzicato pluck
            }
            ptr[j].id = PTR_NONE;
        }
    }

    // autoplay: a slow arco line — each note swells in (a steady auto-rub feeds energy), then lifts
    if (autoplay) {
        atimer += dt();
        arub += (0.9f - arub) * 0.05f;                 // simulated steady bow rub, ramps up per note
        if (atimer > 1.1f) {
            atimer = 0; arub = 0.0f;
            int s = apos % NSTR;
            for (int k = 0; k < NSTR; k++) if (k != s && kbd_bow[k] == 0.0f) bow_off(k);
            bow_on(s); apos++;
        }
        int s = apos == 0 ? 0 : (apos - 1) % NSTR;
        if (hnd[s] >= 0 && kbd_bow[s] == 0.0f) rub[s] += arub * 0.07f;   // feed the swell
    }

    // a keyboard-held string feeds a steady rub (you can't wiggle a key, so the speed knob sets it)
    for (int s = 0; s < NSTR; s++) if (kbd_bow[s] > 0.0f && hnd[s] >= 0)
        rub[s] += (0.4f + knob[2] * 0.4f) * 0.07f;

    // ENERGY: every bowed string BLEEDS energy each frame — the sound only stays up while you keep
    // feeding it (rub / hold / auto). Feeding adds faster than the bleed, so continuous rubbing
    // climbs; past full it "digs in" — more pressure + faster bow — the crescendo into grit.
    for (int s = 0; s < NSTR; s++) if (hnd[s] >= 0) {
        rub[s] *= 0.94f;
        if (rub[s] > 1.3f) rub[s] = 1.3f;
        float e = clamp(rub[s], 0.0f, 1.0f);
        note_vol(hnd[s], e * 7.0f);
        float dig = clamp((rub[s] - 0.9f) / 0.4f, 0.0f, 1.0f);   // over-rubbing → over-bowing
        note_timbre(hnd[s], clamp(knob[1] + dig * 0.45f, 0.0f, 1.0f));
        note_morph (hnd[s], clamp(knob[2] + dig * 0.30f, 0.0f, 1.0f));
    }

    // visuals: bowed swell rises with rub; plucked strings decay
    for (int s = 0; s < NSTR; s++) {
        float tgt = (hnd[s] >= 0) ? (0.25f + rub[s] * 0.75f) : 0.0f;
        if (hnd[s] >= 0) { amp[s] += (tgt - amp[s]) * 0.15f; vib_ph[s] += 0.4f + rub[s] * 0.6f; }
        else             { amp[s] *= 0.92f; vib_ph[s] += 0.9f; }   // pizz ring-down
    }
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("BOWED", 8, 6, CLR_WHITE);
    print("rub=bow  tap=pluck", 60, 7, CLR_MEDIUM_GREY);
    print(autoplay ? "[arco: auto]" : "[arco: off ]", SCREEN_W - 110, 4,
          autoplay ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);

    for (int s = 0; s < NSTR; s++) {
        int x = str_x(s);
        bool bowing = hnd[s] >= 0;
        int col = bowing ? (rub[s] > 0.15f ? CLR_YELLOW : CLR_DARK_GREEN) : CLR_DARK_GREY;
        float a = amp[s];
        int prevx = x, prevy = STR_Y0;
        for (int yy = STR_Y0; yy <= STR_Y1; yy += 6) {
            float t = (yy - STR_Y0) / (float)(STR_Y1 - STR_Y0);
            float env = sinf(t * 3.14159f);
            int dx = (int)(sinf(vib_ph[s] + t * 9.0f) * env * a * 8.0f);
            line(prevx, prevy, x + dx, yy, col);
            prevx = x + dx; prevy = yy;
        }
        // bow-position marker (harmonics) on a bowed string
        if (bowing) {
            int by = STR_Y0 + (int)((STR_Y1 - STR_Y0) * (0.12f + knob[0] * 0.72f));
            line(x - 9, by, x + 9, by, rub[s] > 0.15f ? CLR_PEACH : CLR_BROWN);
        }
        if (a > 0.05f) circfill(x, STR_Y0 - 6, 2 + (int)(a * 3), col);
    }

    for (int k = 0; k < 3; k++) {
        int x = KNOB_X(k), y = KNOB_TOP;
        rect(x, y, KNOB_W, 8, k == sel ? CLR_WHITE : CLR_DARK_GREY);
        rectfill(x, y, (int)(knob[k] * KNOB_W), 8, k == sel ? CLR_LIME_GREEN : CLR_MEDIUM_GREY);
        print(KNOB_NAME[k], x, y - 10, k == sel ? CLR_WHITE : CLR_LIGHT_GREY);
    }
    print("preset:", 8, 24, CLR_MEDIUM_GREY);
    print(PRESET_NAME[preset], 56, 24, CLR_PEACH);
    print("1-6 presets  Z/X oct  Q-U pizz", SCREEN_W - 210, 24, CLR_DARK_GREY);
}
