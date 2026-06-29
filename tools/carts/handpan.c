/* de:meta
{
  "title": "handpan",
  "status": "active",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "radial-symmetry"
  ],
  "lineage": "Sibling to the mallet cart (same INSTR_MALLET engine) but reframed as a physical instrument layout: the zigzag-circle tuning geometry (ascending notes alternate left/right around the ring) is the conceptual core, ensuring physical neighbors are always consonant.",
  "description": "A steel hang drum: one ding, eight tone fields, every strike a gesture. The mallet cart is INSTR_MALLET's tuning rig - this is the same engine as an INSTRUMENT, no knobs at all. Real handpans zigzag the scale across the circle (ascending notes alternate right/left climbing to 12 o'clock) so physical neighbors are consonant: two-handed noodling always sounds composed. WHERE you hit matters - the strike's offset from a field's center drives the timbre macro per hit (center = soft palm thump, edge = bright ping), and taps on the bare steel between fields are the shoulder tok. Four tunings to pick from the rack: kurd, celtic, hijaz, pygmy. Multitouch: every finger is its own pointer - both hands play, like the real thing. Tap/click fields (offset = tone), drag = roll, SPACE ding, A S D F G H J K ring notes ascending, 1..4 pick a pan, M autoplay."
}
de:meta */
// handpan — a steel hang drum: one ding, eight tone fields, every strike a gesture.
//
// The mallet cart is INSTR_MALLET's tuning rig (knobs, presets, a bar row); this is
// the same engine as an INSTRUMENT — no knobs at all, the circular layout does the
// musical thinking. Real handpans zigzag the scale across the circle (ascending
// notes alternate right/left climbing toward 12 o'clock), so PHYSICAL NEIGHBORS ARE
// CONSONANT: two-handed noodling always sounds composed. And WHERE you hit matters —
// the strike's offset from a field's center drives the timbre macro PER HIT
// (center = soft palm thump, edge = bright ping): the engine macros as per-strike
// gesture instead of global sliders. Taps on the bare steel between/around the
// fields are the shoulder "tok" — the percussion half of real handpan playing.
//
// controls: tap/click a field (offset from its center = tone) · drag = roll
//           taps on the steel outside the fields = shoulder tok
//           SPACE ding · A S D F G H J K ring notes, ascending · 1..4 pick a pan
//           M autoplay (label is tappable)
//
// MULTITOUCH: every finger is its own pointer — both hands play, like the real
// thing. The desktop mouse arrives as one synthetic finger, same code path.

#include "studio.h"
#include "pointer.h"     // multi-finger pool: PTR_MAX/PTR_NONE + PTR_CLEAR/PTR_ACQUIRE/PTR_FIND
#include <math.h>
#include <stdio.h>

#define I_PAN 5     // the tone fields + ding
#define I_TOK 6     // the shoulder tok (dry woody tick)

#define NFIELD 8
#define CX     118
#define CY     100
#define PAN_R   86
#define RING_R  56
#define FIELD_R 16
#define DING_R  19

// a pan is a tuning: the ding + eight ring notes, lowest first
typedef struct { const char *name; int ding; int ring[NFIELD]; } Pan;
static const Pan PANS[4] = {
    { "kurd",   50, { 57, 58, 60, 62, 64, 65, 67, 69 } },   // D minor — THE handpan sound
    { "celtic", 50, { 57, 60, 62, 64, 65, 67, 69, 72 } },   // D amara — open, no 2nd
    { "hijaz",  50, { 57, 58, 61, 62, 64, 65, 67, 69 } },   // D phrygian dominant
    { "pygmy",  53, { 55, 56, 60, 63, 65, 67, 68, 72 } },   // F — dark, hollow
};
static int cur_pan = 0;

// the zigzag: ring note n (ascending) sits at this angle (0=right 90=down).
// lowest at 6 o'clock, then alternating right/left climbing to 12 — neighbors consonant
static const int FANG[NFIELD] = { 90, 45, 135, 0, 180, 315, 225, 270 };
static const char STRKEY[NFIELD] = { 'A','S','D','F','G','H','J','K' };
static int fx[NFIELD], fy[NFIELD];   // field centers, computed in init

static const char *PCNAME[12] =
    { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

static float glow[NFIELD];     // strike flash per field, decays each frame
static float dglow;            // ding flash
static float sglow;            // shoulder flash (rim shimmer)
static int   hitx = -1, hity;  // last strike point, drawn briefly
static int   hitt;             // frames left for the strike dot
static bool  autoplay = true;
static int   apos = 0;

// per-finger pointer table — each finger lands, rolls across fields, lifts,
// all independently (the desktop mouse = one synthetic finger)
enum { Z_NONE = -3, Z_SHOULDER = -2, Z_DING = -1 };   // .cur >= 0 → ring field
typedef struct { int id, cur; } Ptr;   // id MUST be first (pointer.h)
static Ptr ptr[PTR_MAX];

static int gate_ms(void) { return 4500; }   // long ring; release keeps it unchopped

// one strike: which surface, how far off its center (0 = dead center, 1 = the rim
// of the field), how hard. The offset IS the timbre — set per hit, heard per hit
// (MALLET's timbre macro samples at note start; ringing notes keep theirs).
static void strike(int zone, float off01, int vol) {
    const Pan *p = &PANS[cur_pan];
    if (zone == Z_DING) {
        instrument_timbre(I_PAN, 0.10f + 0.45f * off01);
        hit(p->ding, I_PAN, vol, gate_ms());
        dglow = 1.0f;
    } else if (zone >= 0) {
        instrument_timbre(I_PAN, 0.14f + 0.58f * off01);
        hit(p->ring[zone], I_PAN, vol, gate_ms());
        glow[zone] = 1.0f;
    } else if (zone == Z_SHOULDER) {
        hit(p->ding - 12, I_TOK, vol, 90);
        sglow = 1.0f;
    }
}

// which surface is this point on? returns the zone and writes the 0..1 offset
static int classify(int tx, int ty, float *off01) {
    float ox = (float)(tx - CX), oy = (float)(ty - CY);
    float d = sqrtf(ox * ox + oy * oy);
    if (d < DING_R + 3) { *off01 = clamp(d / DING_R, 0.0f, 1.0f); return Z_DING; }
    for (int i = 0; i < NFIELD; i++) {
        float gx = (float)(tx - fx[i]), gy = (float)(ty - fy[i]);
        float g = sqrtf(gx * gx + gy * gy);
        if (g < FIELD_R + 4) { *off01 = clamp(g / FIELD_R, 0.0f, 1.0f); return i; }
    }
    if (d < PAN_R + 4) { *off01 = 0.5f; return Z_SHOULDER; }
    return Z_NONE;
}

static void pick_pan(int p) {
    cur_pan = p;
    // audition: the ding, then the zigzag walked ascending — hear the tuning
    const Pan *pn = &PANS[p];
    instrument_timbre(I_PAN, 0.3f);
    schedule_hit(0, pn->ding, I_PAN, 5, gate_ms());
    for (int i = 0; i < NFIELD; i++)
        schedule_hit(140 + i * 55, pn->ring[i], I_PAN, 4, gate_ms());
    dglow = 1.0f;
}

void init(void) {
    // steel, struck by hands: metal-but-warm bar, long ring (morph below the motor)
    instrument(I_PAN, INSTR_MALLET, 1, 0, 7, 1400);
    instrument_harmonics(I_PAN, 0.55f);
    instrument_morph(I_PAN, 0.72f);
    // the shoulder: bone-dry woody tick
    instrument(I_TOK, INSTR_MALLET, 1, 0, 5, 200);
    instrument_harmonics(I_TOK, 0.32f);
    instrument_timbre(I_TOK, 0.85f);
    instrument_morph(I_TOK, 0.05f);

    for (int i = 0; i < NFIELD; i++) {
        fx[i] = CX + (int)(cosf((float)FANG[i] * 3.14159f / 180.0f) * RING_R);
        fy[i] = CY + (int)(sinf((float)FANG[i] * 3.14159f / 180.0f) * RING_R);
    }
    PTR_CLEAR(ptr);
    bpm(84);
    glow[1] = 0.7f; glow[4] = 0.9f; dglow = 0.5f;   // a lively first frame
}

void update(void) {
    if (keyp(KEY_SPACE)) strike(Z_DING, 0.3f, 6);
    for (int i = 0; i < NFIELD; i++)
        if (keyp(STRKEY[i])) strike(i, 0.35f, 6);
    for (int p = 0; p < 4; p++)
        if (keyp('1' + p)) pick_pan(p);
    if (keyp('M')) autoplay = !autoplay;

    // touch: land = strike (offset = tone) · roll into another field = strike it ·
    // the right panel rows pick pans · the top-right label toggles autoplay
    for (int i = 0; i < touch_count(); i++) {
        int id = touch_id(i), tx = touch_x(i), ty = touch_y(i);
        bool fresh;
        Ptr *p = PTR_ACQUIRE(ptr, id, &fresh);
        if (!p) continue;                           // pool full (>PTR_MAX fingers)
        float off;
        if (fresh) {                                // finger just landed
            *p = (Ptr){ id, Z_NONE };
            if (point_in_box(tx, ty, SCREEN_W - 112, 2, 108, 12)) {
                autoplay = !autoplay;
                continue;
            }
            if (tx >= 222) {                        // the pan rack rows
                for (int q = 0; q < 4; q++)
                    if (ty >= 34 + q * 24 && ty < 34 + q * 24 + 20) pick_pan(q);
                continue;
            }
            int z = classify(tx, ty, &off);
            if (z != Z_NONE) strike(z, off, 6);
            if (z == Z_DING || z >= 0) { hitx = tx; hity = ty; hitt = 10; }
            p->cur = z;
        } else {                                    // finger moving: the roll
            int z = classify(tx, ty, &off);
            if (z != p->cur && (z == Z_DING || z >= 0)) {
                strike(z, off, 4);
                hitx = tx; hity = ty; hitt = 8;
            }
            if (z != Z_SHOULDER || p->cur != Z_SHOULDER)  // no shoulder re-toks on drag
                p->cur = z;
        }
    }
    for (int i = 0; i < touch_ended_count(); i++) { // lifted fingers free their slot
        Ptr *p = PTR_FIND(ptr, touch_ended_id(i));
        if (p) p->id = PTR_NONE;
    }

    // autoplay: a two-handed noodle around the zigzag — neighbors are consonant,
    // so a wandering pattern with grace doublets sounds played, not random
    if (autoplay && every(1)) {
        static const int seq[16] = { 0, 2, 4, 1, -1, 3, 2, 5, 0, 4, 6, 2, -1, 7, 4, 1 };
        int z = seq[apos % 16];
        strike(z, chance(50) ? 0.25f : 0.55f, 3 + (apos % 4 == 0 ? 2 : 0));
        if (z >= 0 && chance(30)) {                 // the grace doublet: a near neighbor
            int nb = (z + (chance(50) ? 1 : 2)) % NFIELD;
            instrument_timbre(I_PAN, 0.3f);
            schedule_hit(160, PANS[cur_pan].ring[nb], I_PAN, 3, gate_ms());
        }
        if (chance(12)) schedule_hit(330, PANS[cur_pan].ding - 12, I_TOK, 3, 90);
        apos++;
    }

    for (int i = 0; i < NFIELD; i++) glow[i] *= 0.88f;
    dglow *= 0.88f; sglow *= 0.82f;
    if (hitt > 0) hitt--;

#ifdef DE_TRACE
    watch("pan", "%d", cur_pan);
    watch("auto", "%d", autoplay ? 1 : 0);
#endif
}

void draw(void) {
    cls(CLR_BROWNISH_BLACK);

    // the steel: domed body, a few shading rings, the rim
    circfill(CX, CY, PAN_R, CLR_DARK_GREY);
    circ(CX, CY, PAN_R, CLR_BLACK);
    circ(CX, CY, PAN_R - 3, CLR_DARKER_GREY);
    circ(CX, CY, RING_R + FIELD_R + 6, CLR_DARKER_GREY);
    if (sglow > 0.05f) circ(CX, CY, PAN_R - 1, sglow > 0.5f ? CLR_LIGHT_PEACH : CLR_MEDIUM_GREY);

    const Pan *pn = &PANS[cur_pan];

    // the ding — center dome
    {
        int c = dglow > 0.5f ? CLR_LIGHT_GREY : CLR_MEDIUM_GREY;
        circfill(CX, CY, DING_R, c);
        circ(CX, CY, DING_R, CLR_DARKER_GREY);
        circfill(CX, CY - 2, 6, dglow > 0.3f ? CLR_WHITE : CLR_LIGHT_GREY);
        if (dglow > 0.08f)
            circ(CX, CY, DING_R + (int)((1.0f - dglow) * 12.0f), CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        print(PCNAME[pn->ding % 12], CX - 3, CY + 7, CLR_DARKER_GREY);
        font(FONT_NORMAL);
    }

    // the eight tone fields — dimples with their note names
    for (int i = 0; i < NFIELD; i++) {
        int c = glow[i] > 0.5f ? CLR_LIGHT_GREY : (glow[i] > 0.15f ? CLR_MEDIUM_GREY : CLR_DARK_GREY);
        circfill(fx[i], fy[i], FIELD_R, c);
        circ(fx[i], fy[i], FIELD_R, CLR_DARKER_GREY);
        circ(fx[i], fy[i], 5, glow[i] > 0.3f ? CLR_WHITE : CLR_DARKER_GREY);
        if (glow[i] > 0.08f)
            circ(fx[i], fy[i], FIELD_R + (int)((1.0f - glow[i]) * 12.0f), CLR_LIGHT_PEACH);
        font(FONT_SMALL);
        const char *nm = PCNAME[pn->ring[i] % 12];
        print(nm, fx[i] - text_width(nm) / 2, fy[i] + FIELD_R + 3, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
    }

    // where the last strike landed
    if (hitt > 0 && hitx >= 0) pset(hitx, hity, CLR_WHITE);

    // the pan rack — pick a tuning like picking up a different pan
    print("HANDPAN", 226, 12, CLR_PEACH);
    font(FONT_SMALL);
    for (int p = 0; p < 4; p++) {
        int ry = 34 + p * 24;
        if (p == cur_pan) {
            rectfill(224, ry - 2, 88, 20, CLR_PEACH);
            print(PANS[p].name, 230, ry, CLR_BLACK);
            char dn[12];
            snprintf(dn, sizeof dn, "ding %s%d", PCNAME[PANS[p].ding % 12], PANS[p].ding / 12 - 1);
            print(dn, 230, ry + 9, CLR_DARKER_GREY);
        } else {
            print(PANS[p].name, 230, ry, CLR_LIGHT_GREY);
            print(p == 0 ? "1" : p == 1 ? "2" : p == 2 ? "3" : "4", 304, ry, CLR_DARK_GREY);
        }
    }
    print(autoplay ? "M autoplay: on" : "M autoplay: off",
          SCREEN_W - 4 - text_width(autoplay ? "M autoplay: on" : "M autoplay: off"),
          4, autoplay ? CLR_PEACH : CLR_DARK_GREY);
    print("tap the fields - center is soft, the edge pings", 8, SCREEN_H - 18, CLR_DARK_GREY);
    print("drag = roll / bare steel = tok / SPACE ding / A-K / 1-4 pans", 8, SCREEN_H - 9, CLR_DARK_GREY);
    font(FONT_NORMAL);
}
