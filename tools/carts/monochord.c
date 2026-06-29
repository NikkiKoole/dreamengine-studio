/* de:meta
{
  "title": "monochord",
  "status": "active",
  "kind": [
    "instrument",
    "toy",
    "probe"
  ],
  "teaches": [
    "waveguide-synth",
    "analog-voice-modeling"
  ],
  "lineage": "Pythagoras's monochord and the Vietnamese dan bau — string-length-as-pitch via PD oscillator; novel in using finger position as movable bridges with multitouch pointer pool.",
  "description": "One string, no frets — your FINGERS are the bridges, and the LENGTH of each segment IS its pitch (a string halved sounds an octave higher). The gesture behind the đàn bầu, the diddley bow, slide guitar, and Pythagoras's monochord. DRAG on the string to place/move a bridge; with two fingers the segment BETWEEN them sounds — spread apart = lower, squeeze together = higher. FLICK across the string (from above/below) to PLUCK the segment you cross — pluck outside the bridges for the short 'wrong-side' overtone, pluck several segments for a chord on one string. A PD oscillator, so a ringing note GLIDES as you slide a bridge (the đàn bầu bend). Faint ticks mark the just-intonation points (1/2 octave, 2/3 fifth, 3/4 fourth). SPACE plucks the open string. Two bridges at once needs a touchscreen/multitouch trackpad; the desktop mouse is one finger. An experiment in fretting-by-finger-position (built on pointer.h)."
}
de:meta */
// monochord — one string; your fingers are movable BRIDGES, and the LENGTH of
// each segment IS its pitch. No frets: pitch is pure position. This is the
// gesture behind the đàn bầu (the Vietnamese one-string), the diddley bow,
// slide guitar, and Pythagoras's monochord — the instrument he used to discover
// that a string halved sounds an octave higher.
//
// EXPERIMENT (multitouch-and-generalization-audit.md): does fretting-by-finger
// feel playable without tactile frets, before we try a whole string fretboard?
//
//   • DRAG on the string   = place / move a BRIDGE (a stop). The fixed nut (left)
//                            and bridge (right) are the other two ends.
//   • Two bridges at once  = the segment BETWEEN them sounds. Spread them apart
//                            (longer) = lower; squeeze together (shorter) = higher.
//   • FLICK across the string (from above or below) = PLUCK the segment you cross.
//                            Pluck a segment OUTSIDE the bridges = the short
//                            "wrong-side" overtone. Pluck several = a chord on one
//                            string. (Landing a bridge also gives it a soft pluck.)
//   • no bridges           = the open string (the lowest note). SPACE plucks it.
//   • the faint ticks       = the just-intonation marks (1/2 octave, 2/3 fifth,
//                            3/4 fourth, 1/3 twelfth) — park a bridge on one and
//                            you're exactly in tune, like a real monochord's inlay.
//
// PD oscillator, so a ringing note GLIDES as you slide a bridge — the đàn bầu bend.
// MULTITOUCH: two bridges at once needs a touchscreen / multitouch trackpad; the
// desktop mouse is one finger (one bridge or one pluck at a time).

#include "studio.h"
#include "pointer.h"     // fingers-as-bridges: a bespoke per-finger pool, not widgets
#include <math.h>

#define I_STR 5

#define NUT_X     24
#define BRIDGE_X  (SCREEN_W - 24)
#define STR_Y     (SCREEN_H / 2)
#define SPAN      (BRIDGE_X - NUT_X)       // full open length, in pixels
#define BAND      14                       // how close to the string counts as "on" it
#define BASE_MIDI 40                       // open-string pitch (E2) at full length

// a finger is either a BRIDGE sitting on the string, or a PICK strumming across it
enum { PM_STOP, PM_PICK };
typedef struct { int id, mode, x, prevY; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

// ringing notes — pluck a segment and it keeps sounding (and gliding) as it decays
#define NVOICE 6
typedef struct { int h; int sx; float vol; float midi; } Voice;   // h < 0 = free
static Voice voice[NVOICE];

static float wig = 0;   // string-vibration animation phase

// desktop helper — no touchscreen? A S D F G each HOLD a bridge at a fixed spot,
// so your mouse becomes the second finger (drag a moving bridge / flick to pluck).
// On a touchscreen just use two real fingers and ignore these.
#define NKBD 5
static const char KBKEY[NKBD] = { 'A', 'S', 'D', 'F', 'G' };
static int kbd_x[NKBD];

// every bridge x (the two fixed ends + each live stop finger + held A-G keys), sorted
static int bridges(int *out) {
    int n = 0;
    out[n++] = NUT_X; out[n++] = BRIDGE_X;
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].mode == PM_STOP)
            out[n++] = clamp(ptr[j].x, NUT_X, BRIDGE_X);
    for (int i = 0; i < NKBD; i++) if (key(KBKEY[i])) out[n++] = kbd_x[i];
    for (int i = 1; i < n; i++) {            // insertion sort — n is tiny
        int v = out[i], k = i - 1;
        while (k >= 0 && out[k] > v) { out[k + 1] = out[k]; k--; }
        out[k + 1] = v;
    }
    return n;
}

// the length (px) of the segment straddling x; writes its [a,b] endpoints
static int seg_len(int sx, int *a, int *b) {
    int br[PTR_MAX + NKBD + 2], n = bridges(br);
    int lo = NUT_X, hi = BRIDGE_X;
    for (int i = 0; i < n - 1; i++)
        if (sx >= br[i] && sx <= br[i + 1]) { lo = br[i]; hi = br[i + 1]; break; }
    if (a) *a = lo; if (b) *b = hi;
    int L = hi - lo;
    return L < 1 ? 1 : L;
}

// a string segment of length L sounds f ∝ 1/L: halving the length = +1 octave
static float len_to_midi(int L) { return BASE_MIDI + 12.0f * log2f((float)SPAN / (float)L); }

// pluck the segment that contains screen-x `sx`
static void pluck_at(int sx) {
    if (sx < NUT_X || sx > BRIDGE_X) return;
    float m = len_to_midi(seg_len(sx, 0, 0));
    int slot = -1, q = 0;
    for (int v = 0; v < NVOICE; v++) {                  // a free voice, else steal the quietest
        if (voice[v].h < 0) { slot = v; break; }
        if (voice[v].vol < voice[q].vol) q = v;
    }
    if (slot < 0) { note_off(voice[q].h); slot = q; }
    voice[slot].h = note_on((int)(m + 0.5f), I_STR, 6);
    note_glide(voice[slot].h, 28);                      // smooth the slide
    note_pitch(voice[slot].h, m);
    voice[slot].sx = sx; voice[slot].vol = 1.0f; voice[slot].midi = m;
}

void init(void) {
    // PD plucked string: instant attack, long decay so it rings and can be slid; sustain 0
    instrument(I_STR, INSTR_PD, 2, 1500, 0, 300);
    instrument_harmonics(I_STR, 0.5f);
    instrument_timbre(I_STR, 0.42f);
    instrument_morph(I_STR, 0.5f);
    PTR_CLEAR(ptr);
    for (int v = 0; v < NVOICE; v++) voice[v].h = -1;
    for (int i = 0; i < NKBD; i++) kbd_x[i] = NUT_X + SPAN * (i + 1) / (NKBD + 1);   // A-G evenly across
    pluck_at(BRIDGE_X - SPAN / 2);                         // a lively first frame (the octave)
}

void update(void) {
    if (keyp(KEY_SPACE)) pluck_at(SCREEN_W / 2);
    for (int i = 0; i < NKBD; i++) if (keyp(KBKEY[i])) pluck_at(kbd_x[i]);   // press A-G = drop+pluck a bridge

    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                               // pool full
        if (fresh) {
            int on = (ty >= STR_Y - BAND && ty <= STR_Y + BAND && tx >= NUT_X && tx <= BRIDGE_X);
            *p = (Ptr){ id, on ? PM_STOP : PM_PICK, tx, ty };
            if (on) pluck_at(tx);                          // touching a bridge plucks it (feedback)
        } else if (p->mode == PM_STOP) {
            p->x = clamp(tx, NUT_X, BRIDGE_X);          // slide the bridge
        } else {                                        // PICK: pluck when it crosses the string
            if ((p->prevY < STR_Y && ty >= STR_Y) || (p->prevY > STR_Y && ty <= STR_Y)) pluck_at(tx);
        }
        p->prevY = ty;
    }
    for (int i = 0; i < touch_ended_count(); i++) {
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // ringing voices follow their segment's CURRENT length (slide a bridge → it glides)
    for (int v = 0; v < NVOICE; v++) if (voice[v].h >= 0) {
        voice[v].midi = len_to_midi(seg_len(voice[v].sx, 0, 0));
        note_pitch(voice[v].h, voice[v].midi);
        voice[v].vol *= 0.965f;
        if (voice[v].vol < 0.03f) { note_off(voice[v].h); voice[v].h = -1; }
    }
    wig += 0.6f;

#ifdef DE_TRACE
    int ns = 0, nv = 0;
    for (int j = 0; j < PTR_MAX; j++) if (ptr[j].id != PTR_NONE && ptr[j].mode == PM_STOP) ns++;
    for (int v = 0; v < NVOICE; v++) if (voice[v].h >= 0) nv++;
    watch("stops", "%d", ns);
    watch("voices", "%d", nv);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);
    print("MONOCHORD", 8, 6, CLR_PEACH);
    font(FONT_SMALL);
    print("one string - your fingers are the bridges", 78, 8, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // just-intonation marks — where the RIGHT segment is a pure fraction of the whole
    static const float RAT[4] = { 0.5f, 2.0f / 3.0f, 3.0f / 4.0f, 1.0f / 3.0f };
    static const char *RLB[4] = { "8ve", "5th", "4th", "12th" };
    for (int i = 0; i < 4; i++) {
        int x = BRIDGE_X - (int)(SPAN * RAT[i]);
        for (int y = STR_Y - 16; y < STR_Y + 16; y += 4) pset(x, y, CLR_DARKER_GREY);
        font(FONT_TINY); print(RLB[i], x - 5, STR_Y + 19, CLR_DARK_GREY); font(FONT_NORMAL);
    }

    // the string, bowing where voices ring (a standing half-wave per sounding segment)
    int px = NUT_X, py = STR_Y;
    for (int x = NUT_X + 5; x <= BRIDGE_X; x += 5) {
        float d = 0;
        for (int v = 0; v < NVOICE; v++) if (voice[v].h >= 0) {
            int a, b; seg_len(voice[v].sx, &a, &b);
            if (x >= a && x <= b) d += voice[v].vol * 11.0f * sinf((float)(x - a) / (b - a) * 3.14159f) * sinf(wig);
        }
        int y = STR_Y + (int)d;
        line(px, py, x, y, d > 0.6f || d < -0.6f ? CLR_LIGHT_YELLOW : CLR_LIGHT_GREY);
        px = x; py = y;
    }

    // the fixed nut (left) and bridge (right) posts
    rectfill(NUT_X - 2, STR_Y - 11, 3, 22, CLR_BROWN);
    rectfill(BRIDGE_X, STR_Y - 11, 3, 22, CLR_BROWN);

    // the movable bridges (stop fingers) — a little wedge under the string
    for (int j = 0; j < PTR_MAX; j++)
        if (ptr[j].id != PTR_NONE && ptr[j].mode == PM_STOP) {
            int x = clamp(ptr[j].x, NUT_X, BRIDGE_X);
            trifill(x - 4, STR_Y + 11, x + 4, STR_Y + 11, x, STR_Y, CLR_ORANGE);
            line(x, STR_Y - 8, x, STR_Y + 7, CLR_LIGHT_PEACH);
        }

    // the A-G keyboard bridges: a green wedge while held, a faint key label always
    for (int i = 0; i < NKBD; i++) {
        int x = kbd_x[i];
        bool down = key(KBKEY[i]);
        if (down) {
            trifill(x - 4, STR_Y + 11, x + 4, STR_Y + 11, x, STR_Y, CLR_LIME_GREEN);
            line(x, STR_Y - 8, x, STR_Y + 7, CLR_LIGHT_GREY);
        }
        font(FONT_TINY);
        print(str("%c", KBKEY[i]), x - 2, STR_Y - 24, down ? CLR_LIME_GREEN : CLR_DARKER_GREY);
        font(FONT_NORMAL);
    }

    // readout: the loudest ringing note, as a name
    static const char *NN[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int best = -1;
    for (int v = 0; v < NVOICE; v++) if (voice[v].h >= 0 && (best < 0 || voice[v].vol > voice[best].vol)) best = v;
    if (best >= 0) {
        int mi = (int)(voice[best].midi + 0.5f);
        print(str("%s%d", NN[(((mi) % 12) + 12) % 12], mi / 12 - 1), SCREEN_W / 2 - 12, STR_Y - 42, CLR_LIGHT_YELLOW);
    }

    font(FONT_TINY);
    print("DRAG on the string = bridge    FLICK across = pluck    no touch? hold A S D F G for bridges + mouse = 2nd finger", 8, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
