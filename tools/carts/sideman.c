/* de:meta
{
  "slug": "sideman",
  "title": "wurlitzer side man",
  "status": "active",
  "created": "2026-08-19",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "drum-synthesis",
    "analog-voice-modeling",
    "subtractive-synth"
  ],
  "lineage": "Wurlitzer Side Man (1959) homage, the first drum machine ever sold. Where cr78 rebuilt a 1978 solid-state box as a left-to-right grid, this one is twenty years earlier and electro-mechanical, so the sequencer IS a rotating contact disc read by a fixed wiper: the pattern turns, the playhead does not move.",
  "homage": "Wurlitzer Side Man, Rudolph Wurlitzer Company, 1959",
  "description": {
    "summary": "The first drum machine ever sold, rebuilt: ten vacuum-tube voices on a rotating contact disc, four of them struck wood.",
    "detail": "In 1959 Wurlitzer put a motor, a disc of metal contacts and ten tube circuits in a walnut box and sold it to organ players. It is the oldest drum sound in this studio by two decades, and it does not sound like a drum machine: four of its ten voices are struck WOOD (wood block, two temple blocks, claves) and only two are membranes, which is why the era is remembered as a plock rather than as a beat. The sequencer here is the real mechanism instead of a grid: one disc revolution is one bar, the contacts turn clockwise into a fixed wiper arm at twelve o'clock, and the tempo slider is the motor speed. Click any slot on the disc to stamp or lift a contact. The voices come from the shared sideman.h bank, where the fullness is not reverb but a single-ended tube stage on every voice (asymmetric saturation brings even harmonics) plus the machine's own band limit. The Side Man had no speaker of its own: it fed the organ's amplifier and cabinet, so that stage is a rack you can switch out, pinned from outboard.h, and it returns bit-exact when you switch it off. Two knobs the original never had, both labelled as ours: CONTACT WEAR (every contact sits a hair early or late, the same hair every revolution, which is what a stamped disc actually does) and the cabinet A/B.",
    "controls": "SPACE motor on/off. LEFT/RIGHT rhythm (twelve, as on the dial). UP/DOWN tempo. 1-9 and 0 play the ten voices by hand, like the panel buttons. Click a slot on the disc to stamp or lift a contact, drag to paint. C cabinet, V tank, B contact wear."
  }
}
de:meta */
#include "studio.h"
#include "sideman.h"
#include "outboard.h"
#include "ui.h"
#include "cursor.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── WURLITZER SIDE MAN (1959) ─────────────────────────────────────────────
// The first drum machine ever sold, and the oldest drum sound in this studio.
//
// What makes it worth building is that it is not a sequencer with a drum kit
// bolted on. It is a MOTOR. A disc spins, metal contacts across its face pass
// under a wiper, and each closure fires one of ten vacuum-tube circuits. There
// is no pattern memory, no accent, no per-voice knob and no programming: the
// disc IS the pattern, the tube IS the sound, and the tempo slider is the
// motor's speed. So the cart is a disc, not a grid. One revolution is one bar,
// the contacts turn clockwise into a fixed wiper arm at twelve o'clock, and
// the playhead never moves because on the real machine it cannot.
//
// The sound is in runtime/sideman.h (the shared voice bank), which is where to
// read about why four of the ten voices are struck wood and where the fullness
// comes from. Two things this cart owns instead:
//
//   the twelve rhythms — the real dial, in its own order: Beguine, Bolero,
//     Cha Cha, Foxtrot 2 Beat, Foxtrot 4 Beat, March, Rhumba, Samba, Shuffle,
//     Tango, Waltz, Western. Note that they are not all 4/4 in sixteenths:
//     WALTZ is three beats and SHUFFLE is four beats of TRIPLETS, so the disc
//     carries 12 slots for those and 16 for the rest. A different track layout
//     per rhythm is exactly what a stamped disc gives you for free, and it is
//     the reason a shuffle here is a real triplet instead of a swing knob's
//     approximation of one.
//
//   the cabinet — the Side Man had NO SPEAKER. It fed the organ's amplifier
//     and came out of a wooden cabinet, and that stage is a real part of the
//     remembered sound: mid-forward, top rolled off, gently saturated. So it
//     is a rack here (runtime/outboard.h: the console EQ on its WARM curve
//     plus the asymmetric IRON stage) rather than something baked into the
//     voices. MEASURED: switching the cabinet out and back in reconverges to
//     bit-exact 0.304 s later (two renders, plate parked out, sample-diffed).
//     The stages null exactly; re-engaging one waits for the chain's own memory,
//     and that memory is NOT the EQ (measured 0.0 ms both ways: its state is
//     driven by its input, which never differed) but IRON's wet-path DC blocker,
//     which freezes while the stage is out because drive_insert's early-out
//     returns before it. tools/bypass-check.js --rack sideman gates this. The
//     rack's COMP stage is
//     deliberately left OUT: a 1959 organ amplifier had no bus compressor, and
//     using three quarters of a shared table honestly beats using all of it
//     dishonestly.
//
// ── the two knobs the original never had, declared as ours ────────────────
//   CONTACT WEAR — a stamped disc's contacts do not sit on a perfect grid, so
//     each one fires a hair early or late. The important part is that it is
//     the SAME hair every revolution, which is a static property of the disc
//     and not random jitter: the groove gets a consistent lilt rather than a
//     wobble. Hashed from (rhythm, voice, slot) so a replay is identical.
//   the cabinet A/B — see above. The real one was not switchable.

// ── layout ────────────────────────────────────────────────────────────────
#define DISC_CX   118
#define DISC_CY   126
#define DISC_R    94
#define HUB_R     40
#define TRK_IN    42        // inner radius of the innermost track
#define TRK_PITCH 5         // radius step between tracks
#define TRK_THICK 4         // a track's own width

#define PX 224              // the control panel
#define PY 28
#define PW 172
#define PH 192

#define MAXSTEPS 16

// ── the twelve rhythms, in dial order ─────────────────────────────────────
// beats = per bar, div = subdivisions per beat (4 = sixteenths, 3 = triplets),
// so the disc carries beats*div slots. Rows are indexed by SM_* voice role; a
// NULL row is a track with no contacts stamped on it.
typedef struct {
    const char *name;
    int         beats, div, tempo;
    const char *row[SM_NV];
} Rhythm;

static const Rhythm RHY[] = {
    { "BEGUINE", 4, 4, 104, {
        [SM_BASS]    = "x.....x...x.....",
        [SM_WOOD]    = "...x...x.....x..",
        [SM_TEMP2]   = "....x.......x...",
        [SM_MARACAS] = "x.x.x.x.x.x.x.x.",
      } },
    { "BOLERO", 4, 4, 84, {
        [SM_BASS]    = "x.......x.......",
        [SM_TEMP1]   = "x...x.x.x...x.x.",
        [SM_MARACAS] = "x.x.x.x.x.x.x.x.",
      } },
    { "CHA CHA", 4, 4, 124, {
        [SM_BASS]    = "x.......x.......",
        [SM_CLAVES]  = "x...x...x...x...",
        [SM_TOM1]    = "............x...",
        [SM_TOM2]    = "..............x.",
        [SM_MARACAS] = "x.x.x.x.x.x.x.x.",
      } },
    { "FOXTROT 2 BEAT", 4, 4, 120, {
        [SM_BASS]    = "x.......x.......",
        [SM_BRUSH]   = "....x.......x...",
        [SM_MARACAS] = "x.x.x.x.x.x.x.x.",
      } },
    { "FOXTROT 4 BEAT", 4, 4, 124, {
        [SM_BASS]    = "x...x...x...x...",
        [SM_BRUSH]   = "....x.......x...",
        [SM_WOOD]    = "..x...x...x...x.",
        [SM_CYMBAL]  = "x...............",
      } },
    { "MARCH", 4, 4, 116, {
        [SM_BASS]    = "x.......x.......",
        [SM_BRUSH]   = "....xx.x....xx.x",
        [SM_WOOD]    = "x.......x.......",
        [SM_CYMBAL]  = "x...............",
      } },
    { "RHUMBA", 4, 4, 112, {
        [SM_BASS]    = "x.....x.x.......",
        [SM_CLAVES]  = "x..x..x...x.x...",
        [SM_TEMP2]   = "....x.......x...",
        [SM_MARACAS] = "x.x.x.x.x.x.x.x.",
      } },
    { "SAMBA", 4, 4, 140, {
        [SM_BASS]    = "x..x..x.x..x..x.",
        [SM_TEMP1]   = "..x.x..x..x.x..x",
        [SM_MARACAS] = "xxxxxxxxxxxxxxxx",
      } },
    { "SHUFFLE", 4, 3, 112, {              // four beats of TRIPLETS: 12 slots
        [SM_BASS]    = "x.....x.....",
        [SM_BRUSH]   = "x.xx.xx.xx.x",
        [SM_WOOD]    = "...x.....x..",
      } },
    { "TANGO", 4, 4, 118, {
        [SM_BASS]    = "x..xx.x.x..xx.x.",
        [SM_CLAVES]  = "x.......x.......",
        [SM_TEMP1]   = "....x.......x...",
      } },
    { "WALTZ", 3, 4, 156, {                // three beats: 12 slots
        [SM_BASS]    = "x...........",
        [SM_BRUSH]   = "....x...x...",
        [SM_TEMP1]   = "....x.......",
        [SM_TEMP2]   = "........x...",
      } },
    { "WESTERN", 4, 4, 132, {
        [SM_BASS]    = "x.......x.......",
        [SM_BRUSH]   = "....x.......x...",
        [SM_WOOD]    = "..x...x...x...x.",
      } },
};
#define NRHY ((int)(sizeof RHY / sizeof RHY[0]))

// track colours: the wooden family warm (they are this machine's melody), the
// membranes red, the noise voices cool
static const int VCLR[SM_NV] = {
    CLR_RED, CLR_DARK_RED, CLR_PINK,                    // bass, tom I, tom II
    CLR_ORANGE, CLR_PEACH, CLR_DARK_ORANGE, CLR_YELLOW, // wood, temple I+II, claves
    CLR_INDIGO, CLR_LIME_GREEN, CLR_BLUE,               // brush, maracas, cymbal
};
static const char VKEY[SM_NV] = { '1','2','3','4','5','6','7','8','9','0' };

// ── state ─────────────────────────────────────────────────────────────────
static bool  disc[SM_NV][MAXSTEPS];   // the live contacts, loaded from a rhythm
static int   rhy      = 4;            // FOXTROT 4 BEAT, the one everybody used
static int   tempo    = 124;
static bool  motor    = true;
static bool  wear     = true;         // contact-placement tolerance
static int   last_s   = -1;
static int   playhead = 0;
static float rot      = 0.0f;         // the disc's angle, degrees
static int   flash[SM_NV];
static int   wiper_lit;
static bool  paint_val;
static bool  painting;
static float rknob, tknob;            // the panel's two continuous controls
static Outboard rack;
static ObSend   sends[SM_NV];
static int      n_sends;

static int steps_of(int r) { return RHY[r].beats * RHY[r].div; }

static void apply_rack(void) { outboard_apply(&rack, sends, n_sends); }

static void load_rhythm(void) {
    const Rhythm *p = &RHY[rhy];
    int n = steps_of(rhy);
    for (int v = 0; v < SM_NV; v++)
        for (int s = 0; s < MAXSTEPS; s++)
            disc[v][s] = (s < n && p->row[v] && p->row[v][s] == 'x');
    tempo = p->tempo;
    bpm(tempo);
    tknob  = (float)(tempo - 48) / 152.0f;
    rknob  = ((float)rhy + 0.5f) / (float)NRHY;
    last_s = -1;
}

// ── the disc's geometry ───────────────────────────────────────────────────
// Slot s sits at disc angle -360*s/n, and the whole disc is turned by `rot`,
// so its SCREEN angle is that plus rot (0 = right, 90 = down). It passes under
// the wiper at twelve o'clock when the screen angle reaches -90, which happens
// at rot = 360*s/n: one revolution is one bar, so rot is just the bar position
// in degrees.
static float slot_deg(int s, int n, float r) {
    return -90.0f - 360.0f * (float)s / (float)n + r;
}
static int deg_slot(float scr, int n, float r) {
    int s = (int)floorf((r - 90.0f - scr) * (float)n / 360.0f + 0.5f);
    s %= n; if (s < 0) s += n;
    return s;
}
static int track_r(int v) { return TRK_IN + v * TRK_PITCH + TRK_THICK / 2; }

// The sequencer's two pure functions: WHERE the disc has turned to, and WHICH
// slot is under the wiper, both from the beat clock alone. Kept pure and side
// effect free so spec() can assert a whole revolution of them, and so it can
// assert the invariant this whole cart rests on: the picture and the sound
// agree, i.e. the slot that fires is the slot standing under the arm. (It has
// to be done that way. The beat clock is advanced by the audio device, which
// the spec runner never starts, so beat() is frozen at 0 under -DDE_SPEC and
// any assertion about the LIVE clock would pass vacuously.)
static float rot_at(int b, float bp, int r) {
    int bts = RHY[r].beats;
    return 360.0f * ((float)(b % bts) + bp) / (float)bts;
}
static int slot_at(int b, float bp, int r) {
    int n = steps_of(r), s = b * RHY[r].div + (int)(bp * (float)RHY[r].div);
    return ((s % n) + n) % n;
}

// A stamped disc's contacts do not sit on a perfect grid: each fires a hair
// early or late, and it is the SAME hair every revolution. Hashed, not random,
// so the lilt is a property of the disc and a replay is identical.
static int contact_wear(int r, int v, int s) {
    unsigned h = (unsigned)r * 2654435761u ^ (unsigned)v * 40503u ^ (unsigned)s * 2246822519u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (int)(h % 9u) - 4;      // -4..+4 ms
}

static void fire(int v, int delay) {
    sideman_fire(SIDEMAN_BASE, v, 0, delay < 0 ? 0 : delay);
}

void init(void) {
    sideman_build(SIDEMAN_BASE);

    // the plate/tank send per voice: the brush and cymbal carry the room, the
    // wooden family gets a touch, the bass drum gets none (a reverb send on a
    // kick only smears it)
    static const float SEND[SM_NV] = {
        0.00f, 0.18f, 0.18f, 0.30f, 0.30f, 0.30f, 0.25f, 0.55f, 0.35f, 0.65f
    };
    n_sends = 0;
    for (int v = 0; v < SM_NV; v++)
        if (SEND[v] > 0.001f) {
            sends[n_sends].slot = sideman_slot(SIDEMAN_BASE, v);
            sends[n_sends].send = SEND[v];
            n_sends++;
        }

    // the organ cabinet: the console EQ on its WARM curve (weight down low,
    // mids out of the way, a little sheen) into the asymmetric IRON stage.
    // No COMP: a 1959 organ amplifier had no bus compressor.
    rack = outboard_default();
    rack.eq_on   = 1;  rack.eq_amt   = 0.72f;  rack.curve = 0;   // WARM
    rack.iron_on = 1;  rack.iron_amt = 0.38f;
    rack.comp_on = 0;
    rack.plate_on = 1; rack.plate_amt = 0.34f;
    apply_rack();

    load_rhythm();
}

void update(void) {
    // ten panel buttons, played by hand
    for (int v = 0; v < SM_NV; v++)
        if (keyp(VKEY[v])) { fire(v, 0); flash[v] = 6; }

    // the motor switch: SPACE, or a tap on the hub (the disc's centre is inside
    // the innermost track, so this can never be mistaken for stamping a slot)
    if (keyp(KEY_SPACE) || tapp(DISC_CX - 28, DISC_CY - 28, 56, 56)) { motor = !motor; last_s = -1; }
    if (keyp(KEY_LEFT))  { rhy = (rhy + NRHY - 1) % NRHY; load_rhythm(); }
    if (keyp(KEY_RIGHT)) { rhy = (rhy + 1) % NRHY;        load_rhythm(); }
    if (keyp(KEY_UP))    { tempo += 4; if (tempo > 200) tempo = 200; bpm(tempo); tknob = (float)(tempo - 48) / 152.0f; }
    if (keyp(KEY_DOWN))  { tempo -= 4; if (tempo <  48) tempo =  48; bpm(tempo); tknob = (float)(tempo - 48) / 152.0f; }
    if (keyp('B'))       { wear = !wear; }
    if (keyp('C'))       { rack.eq_on = rack.iron_on = !rack.eq_on; apply_rack(); }
    if (keyp('V'))       { rack.plate_on = !rack.plate_on; apply_rack(); }

    // stamp or lift a contact: hit-test in POLAR space, then undo the disc's
    // own rotation to land on a slot
    int n = steps_of(rhy);
    float dx = (float)mouse_x() - DISC_CX, dy = (float)mouse_y() - DISC_CY;
    float rr = sqrtf(dx * dx + dy * dy);
    int   v  = (int)((rr - TRK_IN) / TRK_PITCH);
    bool  on_disc = (rr >= TRK_IN && v >= 0 && v < SM_NV);
    int   slot = 0;
    if (on_disc) slot = deg_slot(de_atan2f(dy, dx) * 57.2957795f, n, rot);

    if (mouse_pressed(MOUSE_LEFT) && on_disc) {
        paint_val = !disc[v][slot];
        disc[v][slot] = paint_val;
        painting = true;
        if (paint_val) { fire(v, 0); flash[v] = 6; }
    } else if (mouse_down(MOUSE_LEFT)) {
        if (painting && on_disc) disc[v][slot] = paint_val;
    } else {
        painting = false;
    }

    if (!motor) return;

    // the motor. rot is the bar position in degrees, taken straight off the
    // synth's own beat counter so the picture and the sound cannot drift.
    int div = RHY[rhy].div;
    rot = rot_at(beat(), beat_pos(), rhy);

    // one slot per contact closure, scheduled ONE SLOT AHEAD with
    // schedule_hit() so hits land sample-accurate and free of frame jitter
    int s = beat() * div + (int)(beat_pos() * (float)div);
    if (s == last_s) return;
    bool first = (last_s < 0);
    last_s   = s;
    playhead = slot_at(beat(), beat_pos(), rhy);

    wiper_lit = 0;
    for (int i = 0; i < SM_NV; i++)
        if (disc[i][playhead]) { flash[i] = 6; wiper_lit = 4; }

    float f = beat_pos() * (float)div; f -= (float)(int)f;
    int step_ms = 60000 / (tempo * div);
    int base_delay = (int)((1.0f - f) * (float)step_ms);
    int nx = ((s + 1) % n + n) % n;
    for (int i = 0; i < SM_NV; i++)
        if (disc[i][nx])
            fire(i, base_delay + (wear ? contact_wear(rhy, i, nx) : 0));

    if (first)                              // fresh start: sound the slot we are on
        for (int i = 0; i < SM_NV; i++)
            if (disc[i][playhead]) fire(i, 0);
}

// ── drawing ───────────────────────────────────────────────────────────────
// FONT_SMALL advances 5px per glyph (4 wide plus a spacing column), FONT_NORMAL 8.
#define CW_SMALL 5
#define CW_NORM  8
static void print_mid(const char *s, int cx, int y, int cw, int c) {
    print(s, cx - (int)strlen(s) * cw / 2, y, c);
}

// a contact is a stamped metal strip lying along its track, so it is a rect
// rotated to the tangent. rectfill_rot is GPU geometry, which matters: a
// sector fill would rescan the whole disc's bounding box per contact.
static void contact(int v, int s, int n, int w, int h, int c) {
    float a  = slot_deg(s, n, rot);
    float ra = a * 0.0174532925f;
    int   r  = track_r(v);
    int   px = DISC_CX + (int)(de_cosf(ra) * (float)r);
    int   py = DISC_CY + (int)(de_sinf(ra) * (float)r);
    rectfill_rot(px, py, w, h, a + 90.0f, c);
}

static void draw_disc(void) {
    int n = steps_of(rhy);

    // hover: which track is the pointer over? Naming it is the only way a
    // stranger learns that the rings are the ten voices.
    float hdx = (float)mouse_x() - DISC_CX, hdy = (float)mouse_y() - DISC_CY;
    float hrr = sqrtf(hdx * hdx + hdy * hdy);
    int   hov = (hrr >= TRK_IN && hrr < TRK_IN + SM_NV * TRK_PITCH)
                ? (int)((hrr - TRK_IN) / TRK_PITCH) : -1;

    circfill(DISC_CX, DISC_CY, DISC_R, CLR_BLACK);                // the well
    circfill(DISC_CX, DISC_CY, DISC_R - 2, CLR_BROWNISH_BLACK);   // black bakelite
    circ(DISC_CX, DISC_CY, DISC_R - 2, CLR_DARK_GREY);
    circ(DISC_CX, DISC_CY, DISC_R - 5, CLR_DARKER_GREY);

    for (int v = 0; v < SM_NV; v++) {                              // the ten tracks
        circ(DISC_CX, DISC_CY, track_r(v), v == hov ? VCLR[v] : CLR_DARKER_GREY);
        for (int s = 0; s < n; s++) {
            bool on   = disc[v][s];
            bool beat = (s % RHY[rhy].div) == 0;
            // an empty slot still has to be VISIBLE, or nobody discovers that
            // the disc is stampable
            int  c    = on ? ((flash[v] > 0 && s == playhead && motor) ? CLR_WHITE : VCLR[v])
                           : (beat ? CLR_DARK_GREY : CLR_DARKER_GREY);
            // a stamped contact is a fat brass strip; an empty slot is just the
            // place one could go, so it stays a hairline
            contact(v, s, n, on ? 11 : (beat ? 5 : 4), on ? TRK_THICK : (beat ? 2 : 1), c);
        }
    }

    // the hub: the plate the dial's selection is written on
    circfill(DISC_CX, DISC_CY, HUB_R, CLR_BLACK);
    circ(DISC_CX, DISC_CY, HUB_R, CLR_DARK_GREY);
    circ(DISC_CX, DISC_CY, HUB_R - 3, CLR_DARKER_GREY);
    char buf[28];
    font(FONT_SMALL);
    print_mid(RHY[rhy].name, DISC_CX, DISC_CY - 26, CW_SMALL, CLR_LIGHT_PEACH);
    font(FONT_NORMAL);
    sprintf(buf, "%3d", tempo);
    print_mid(buf, DISC_CX, DISC_CY - 17, CW_NORM, motor ? CLR_YELLOW : CLR_DARK_GREY);
    font(FONT_SMALL);
    print_mid(motor ? "RUNNING" : "MOTOR OFF", DISC_CX, DISC_CY + 14, CW_SMALL,
              motor ? CLR_LIME_GREEN : CLR_DARK_RED);
    if (hov >= 0) print_mid(SIDEMAN_NAME[hov], DISC_CX, DISC_CY + 24, CW_SMALL, VCLR[hov]);
    else          print_mid("TAP TO STOP", DISC_CX, DISC_CY + 24, CW_SMALL, CLR_DARK_GREY);
    font(FONT_NORMAL);

    circfill(DISC_CX, DISC_CY, 4, CLR_MEDIUM_GREY);               // the spindle
    circ(DISC_CX, DISC_CY, 4, CLR_DARK_GREY);

    // the wiper arm: hinged at the rim, fixed at twelve o'clock, because the
    // playhead on this machine cannot move
    int lit = wiper_lit > 0;
    int arm = lit ? CLR_WHITE : CLR_LIGHT_GREY;
    int top = DISC_CY - DISC_R - 3, len = DISC_R - HUB_R + 8;
    rectfill(DISC_CX - 2, top, 5, len, CLR_BLACK);
    rectfill(DISC_CX - 1, top, 3, len, CLR_DARK_GREY);
    rectfill(DISC_CX,     top, 1, len, arm);
    circfill(DISC_CX, top + 2, 4, CLR_DARK_GREY);                 // the pivot post
    circ(DISC_CX, top + 2, 4, CLR_MEDIUM_GREY);
    // and the COMB: one contact finger per track, because the real machine reads
    // all ten at once. A finger lights in its own voice's colour the moment its
    // contact closes, which is the whole mechanism visible in one glance.
    for (int v = 0; v < SM_NV; v++) {
        int fy = DISC_CY - track_r(v) - 1;
        bool closed = motor && flash[v] > 0 && disc[v][playhead];
        rectfill(DISC_CX - 3, fy, 7, 3, closed ? VCLR[v] : arm);
        if (closed) rectfill(DISC_CX - 5, fy - 1, 11, 5, VCLR[v]);
    }
}

static void draw_panel(void) {
    rectfill(PX, PY, PW, PH, CLR_BLACK);
    rect(PX, PY, PW, PH, CLR_DARK_GREY);

    font(FONT_SMALL);
    print("RHYTHM", PX + 8, PY + 6, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the rotary selector: twelve detents, as on the lid
    int kx = PX + 28, ky = PY + 34;
    for (int i = 0; i < NRHY; i++) {
        float a = (-90.0f + 360.0f * (float)i / (float)NRHY) * 0.0174532925f;
        int   tx = kx + (int)(de_cosf(a) * 21.0f), ty = ky + (int)(de_sinf(a) * 21.0f);
        if (i == rhy) rectfill(tx - 1, ty - 1, 3, 3, CLR_YELLOW);
        else          rectfill(tx, ty, 2, 2, CLR_DARK_GREY);
    }
    if (ui_knob_at(&rknob, kx, ky, 15, NULL)) {
        int want = (int)(rknob * (float)NRHY);
        if (want >= NRHY) want = NRHY - 1;
        if (want < 0) want = 0;
        if (want != rhy) { rhy = want; load_rhythm(); }
    }

    font(FONT_SMALL);
    int lx = PX + 52, ly = PY + 20;
    rectfill(lx, ly, PW - 60, 28, CLR_DARKER_GREY);
    rect(lx, ly, PW - 60, 28, CLR_DARK_GREY);
    print(RHY[rhy].name, lx + 4, ly + 4, CLR_LIGHT_PEACH);
    char buf[32];
    sprintf(buf, "%d OF %d ON THE DIAL", rhy + 1, NRHY);
    print(buf, lx + 4, ly + 12, CLR_MEDIUM_GREY);
    sprintf(buf, "%d SLOTS  %d/%s", steps_of(rhy), RHY[rhy].beats,
            RHY[rhy].div == 3 ? "TRIPLET" : "16TH");
    print(buf, lx + 4, ly + 20, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // tempo: the motor's speed
    if (ui_slider(&tknob, PX + 8, PY + 62, PW - 16, "TEMPO")) {
        tempo = 48 + (int)(tknob * 152.0f);
        bpm(tempo);
    }
    font(FONT_SMALL);
    sprintf(buf, "%d BPM   MOTOR %d RPM", tempo, tempo * 60 / (RHY[rhy].beats * 60));
    print(buf, PX + 8, PY + 76, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    // the cabinet the machine did not have a speaker for, and our one knob
    if (ui_button(PX + 8, PY + 86, 50, 15, "CABINET")) {
        rack.eq_on = rack.iron_on = !rack.eq_on;
        apply_rack();
    }
    if (ui_button(PX + 62, PY + 86, 44, 15, "TANK")) {
        rack.plate_on = !rack.plate_on;
        apply_rack();
    }
    if (ui_button(PX + 110, PY + 86, 54, 15, "WEAR")) wear = !wear;
    rectfill(PX + 8,   PY + 103, 50, 3, rack.eq_on    ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rectfill(PX + 62,  PY + 103, 44, 3, rack.plate_on ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    rectfill(PX + 110, PY + 103, 54, 3, wear          ? CLR_LIME_GREEN : CLR_DARKER_GREY);

    // the ten voice buttons, in panel order, coloured like their tracks
    for (int v = 0; v < SM_NV; v++) {
        int bx = PX + 8 + (v / 5) * 82;
        int by = PY + 112 + (v % 5) * 16;
        int c  = flash[v] > 0 ? CLR_WHITE : VCLR[v];
        rectfill(bx, by, 78, 14, flash[v] > 0 ? CLR_DARK_GREY : CLR_DARKER_GREY);
        rectfill(bx, by, 3, 14, c);
        font(FONT_SMALL);
        print(SIDEMAN_SHORT[v], bx + 7, by + 4, flash[v] > 0 ? CLR_WHITE : CLR_LIGHT_GREY);
        char k[2] = { VKEY[v], 0 };
        print(k, bx + 70, by + 4, CLR_MEDIUM_GREY);
        font(FONT_NORMAL);
        // the visual is compact, the touch target is inflated (fat fingers)
        if (tapp(bx - 2, by - 1, 82, 16)) { fire(v, 0); flash[v] = 6; }
    }
}

void draw(void) {
    cls(CLR_DARK_BROWN);
    for (int y = 3; y < 240; y += 7) line(0, y, 399, y, CLR_BROWNISH_BLACK);

    rectfill(6, 4, 388, 19, CLR_BLACK);
    rect(6, 4, 388, 19, CLR_DARK_GREY);
    print("WURLITZER SIDE MAN", 12, 9, CLR_LIGHT_PEACH);
    font(FONT_SMALL);
    print("RHYTHM ATTACHMENT   1959   THE FIRST ONE", 168, 11, CLR_DARK_GREY);
    font(FONT_NORMAL);

    ui_begin();
    draw_disc();
    draw_panel();
    ui_end();

    font(FONT_SMALL);
    print("SPACE MOTOR   ARROWS RHYTHM+TEMPO   1-0 PLAY   CLICK A SLOT",
          8, 230, CLR_MEDIUM_GREY);
    font(FONT_NORMAL);

    for (int v = 0; v < SM_NV; v++) if (flash[v] > 0) flash[v]--;
    if (wiper_lit > 0) wiper_lit--;

    float dx = (float)mouse_x() - DISC_CX, dy = (float)mouse_y() - DISC_CY;
    float rr = sqrtf(dx * dx + dy * dy);
    cursor_draw(rr >= TRK_IN && rr <= TRK_IN + SM_NV * TRK_PITCH ? CUR_HAND : CUR_ARROW);
}

#ifdef DE_SPEC
#include "spec.h"
void spec(void) {
    step(1);                                  // init(): build the bank, load the dial

    // ── the dial ──────────────────────────────────────────────────────────
    expect_eq(NRHY, 12, "the dial carries twelve rhythms");
    for (int r = 0; r < NRHY; r++) {
        int n = steps_of(r);
        expect(n > 0 && n <= MAXSTEPS, "every rhythm fits the disc");
        expect_eq(n, RHY[r].beats * RHY[r].div, "slots = beats * subdivision");
        int hits = 0, over = 0;
        for (int v = 0; v < SM_NV; v++) {
            if (!RHY[r].row[v]) continue;
            int len = (int)strlen(RHY[r].row[v]);
            expect_eq(len, n, "a track's string is exactly one revolution long");
            for (int s = 0; s < len; s++) {
                if (RHY[r].row[v][s] == 'x') { hits++; if (s >= n) over++; }
                else expect(RHY[r].row[v][s] == '.', "a track is only x and .");
            }
        }
        expect(hits >= 3, "every rhythm stamps at least three contacts");
        expect_eq(over, 0, "no contact is stamped past the last slot");
    }
    // not all 4/4: the layout changes with the rhythm, which is the whole
    // point of a stamped disc
    expect_eq(steps_of(10), 12, "WALTZ is three beats, so twelve slots");
    expect_eq(RHY[8].div, 3, "SHUFFLE is real triplets, not a swung sixteenth");

    // ── the disc's geometry round-trips ────────────────────────────────────
    // Every slot must map to an angle and back to itself, at any rotation:
    // this is what the click-to-stamp hit test depends on.
    for (int n = 12; n <= 16; n += 4)
        for (float r = 0.0f; r < 360.0f; r += 37.0f)
            for (int s = 0; s < n; s++)
                expect_eq(deg_slot(slot_deg(s, n, r), n, r), s,
                          "slot -> angle -> slot round-trips");
    // and slot s passes the wiper (screen angle -90) at rot = 360*s/n
    for (int s = 0; s < 16; s++) {
        float at = 360.0f * (float)s / 16.0f;
        expect(spec_close(slot_deg(s, 16, at), -90.0f, 0.01f),
               "a contact reaches the wiper at its own bar position");
    }
    // tracks do not overlap and stay inside the plate
    for (int v = 1; v < SM_NV; v++)
        expect(track_r(v) > track_r(v - 1), "tracks are ordered outward");
    expect(track_r(SM_NV - 1) + TRK_THICK / 2 < DISC_R - 2, "the outer track is on the plate");
    expect(track_r(0) - TRK_THICK / 2 > HUB_R, "the inner track clears the hub");

    // ── contact wear is a property of the disc, not noise ──────────────────
    for (int v = 0; v < SM_NV; v++)
        for (int s = 0; s < MAXSTEPS; s++) {
            int a = contact_wear(rhy, v, s), b = contact_wear(rhy, v, s);
            expect_eq(a, b, "the same contact is always off by the same hair");
            expect(a >= -4 && a <= 4, "wear stays inside a few ms");
        }
    int spread = 0;
    for (int s = 0; s < 16; s++) if (contact_wear(3, SM_WOOD, s) != 0) spread++;
    expect(spread >= 8, "wear actually varies across a track");

    // ── the motor, and THE invariant this cart rests on ───────────────────
    // The picture and the sound must agree: the slot that fires has to be the
    // slot standing under the wiper arm. Asserted on the pure functions across
    // every rhythm and every slot, because the live beat clock is frozen under
    // the spec runner (see rot_at) and would pass vacuously.
    expect(motor, "the motor starts running");
    for (int r = 0; r < NRHY; r++) {
        int n = steps_of(r), bts = RHY[r].beats;
        for (int sl = 0; sl < n; sl++) {
            float at = (float)bts * (float)sl / (float)n + 0.0005f;  // bar position of slot sl
            int   b  = (int)at;
            float bp = at - (float)b;
            expect_eq(slot_at(b, bp, r), sl, "the slot under the arm is the slot that fires");
            expect(spec_close(slot_deg(sl, n, rot_at(b, bp, r)), -90.0f, 0.6f),
                   "and it is standing at twelve o'clock when it does");
        }
        expect(spec_close(rot_at(0, 0.0f, r), 0.0f, 0.001f), "a bar starts with the disc at zero");
        expect(spec_close(rot_at(bts - 1, 0.999f, r), 360.0f, 1.0f), "and ends one full turn later");
        expect_eq(slot_at(bts, 0.0f, r), 0, "the next bar comes back to the first slot");
    }
    spec_tap(KEY_SPACE);
    expect(!motor, "space stops the motor");
    float held = rot;
    step(60);
    expect(spec_close(rot, held, 0.001f), "a stopped disc holds still");
    spec_tap(KEY_SPACE);
    expect(motor, "space starts it again");

    // ── the dial reloads the disc ─────────────────────────────────────────
    int was = rhy;
    spec_tap(KEY_RIGHT);
    expect_eq(rhy, (was + 1) % NRHY, "right steps the dial");
    expect_eq(tempo, RHY[rhy].tempo, "a rhythm brings its own tempo");
    int stamped = 0;
    for (int v = 0; v < SM_NV; v++)
        for (int s = 0; s < steps_of(rhy); s++) if (disc[v][s]) stamped++;
    expect(stamped >= 3, "selecting a rhythm stamps its contacts");
    int past = 0;
    for (int v = 0; v < SM_NV; v++)
        for (int s = steps_of(rhy); s < MAXSTEPS; s++) if (disc[v][s]) past++;
    expect_eq(past, 0, "no contact is left on a slot this rhythm does not have");

    // ── tempo limits ──────────────────────────────────────────────────────
    for (int i = 0; i < 80; i++) spec_tap(KEY_UP);
    expect_eq(tempo, 200, "the motor tops out");
    for (int i = 0; i < 120; i++) spec_tap(KEY_DOWN);
    expect_eq(tempo, 48, "and bottoms out");

    // ── the cabinet is a real A/B ─────────────────────────────────────────
    expect(rack.eq_on && rack.iron_on, "the cabinet starts in circuit");
    expect(!rack.comp_on, "no bus compressor: a 1959 organ amp had none");
    spec_tap('C');
    expect(!rack.eq_on && !rack.iron_on, "C takes the whole cabinet out");
    spec_tap('C');
    expect(rack.eq_on && rack.iron_on, "and puts it back");
    spec_tap('B');
    expect(!wear, "B lifts contact wear, so the disc becomes perfect");
    spec_tap('B');
    expect(wear, "and stamps it back");

    // every voice must be reachable by hand, and the sends must name real slots
    expect_eq(n_sends, 9, "nine voices feed the tank, the bass drum does not");
    for (int i = 0; i < n_sends; i++)
        expect(sends[i].slot >= SIDEMAN_BASE &&
               sends[i].slot < SIDEMAN_BASE + SIDEMAN_NSLOT, "a send names a slot in the bank");
}
#endif
