/* de:meta
{
  "slug": "autorhythm",
  "title": "auto rhythm",
  "status": "active",
  "created": "2026-08-20",
  "kind": [
    "instrument"
  ],
  "teaches": [
    "step-sequencer",
    "drum-synthesis"
  ],
  "lineage": "The AUTO RHYTHM section of a 1960s-70s organ, played from patterns read off the manufacturers' own service manuals rather than invented: Ace Tone Rhythm Ace FR-2L (c.1969), Roland TR-77 (1972) and the SGS M252/M253 rhythm-generator LSIs. Where cr78 is a 1978 box as a grid and sideman is a 1959 box as a rotating disc, this one is the ORGAN's built-in rhythm section, whose interface was never a grid: a dial of preset rhythms and one clock knob.",
  "homage": "Ace Tone Rhythm Ace FR-2L; Roland Rhythm TR-77; SGS M252/M253 rhythm generator",
  "description": {
    "summary": "76 preset rhythms off three generations of organ rhythm hardware, read from the service manuals, with the mechanism on screen instead of a sidx grid.",
    "detail": "Every latin pattern in this studio used to be a plausible reconstruction. These are not: all 76 come off manufacturer documents (docs/design/rhythm-box-patterns.md) and each one is traceable to a page and a figure. The cart is built to show what makes them unlike a sidx grid. ONE FIXED CLOCK, RE-DIVIDED PER RHYTHM: the FR-2L waltz splits the same 24-count bar by three, its slow rock reads all 48 counts as one 12/8 bar. SKIPPED STATES: the TR-77 hatches out counts a rhythm does not use, so its counter steps OVER them, which is what gives 6/8 MARCH its meter, and you can watch it happen. MARKS THAT DO NOT SOUND: on the SGS chips the trigger is the rising edge of a ROM bit, so two marks on consecutive states fire once, and those cells are drawn hollow (66 of the M252's 955 marks are hollow). GATES, NOT HITS: a few lanes are held rather than struck. The tempo control is the machine's own VARIABLE CLOCK in ticks per second, not a BPM, because that is what these boxes actually had; the implied BPM is shown but derived. THE VOICES ARE NOT SOURCED and the cart says so: the FR-2L scan is about 75 dpi, its lane labels are interpolation rather than ink, so the printed label is shown dim and the voice you hear is assigned BY LANE ORDER from the sideman.h tube bank, which you can rotate. Patterns sourced, voicing yours.",
    "controls": "SPACE run/stop. K = the organ key: hold it and the rhythm plays while held, released it stops (Elektor 1976 fig 22: on these instruments the drums began when you played). LEFT/RIGHT rhythm, [ and ] machine, UP/DOWN clock rate. V rotates the voice map. M switches the TR-77 metronome click on (it is a click for the player, not part of the kit, so it is off by default). 1-8 audition a lane. Z and X transpose the held chord a semitone and N flips it minor/major: those two only do anything on the machines that HAVE accompaniment lanes (the Hammond patent and the SGS M255), and the footer lights their row up when they do."
  }
}
de:meta */
#include "studio.h"
#include "rhythmbox.h"
#include "sideman.h"
#include <stdio.h>
#include <string.h>

// ── the four machines, in date order ──────────────────────────────────────
typedef struct { const char *name, *era, *note; const RbRhythm *set; int n; } Machine;
static const Machine MACH[] = {
    { "ACE TONE FR-2L", "c.1969  discrete logic", 0, RB_FR2L, RB_FR2L_N },
    { "ROLAND TR-77",   "1972  diode matrix",     0, RB_TR77, RB_TR77_N },
    { "SGS M252",       "1970s  mask ROM",        "lanes are chip PINS, not named instruments",
      RB_SGS,  RB_SGS_N  },
    { "SGS M253",       "1970s  mask ROM",        "lanes are chip PINS, not named instruments",
      RB_M253, RB_M253_N },
    { "HAMMOND patent", "1969  US 3,567,838",     "CHORD and BASS lanes GATE the held chord",
      RB_HAM,  RB_HAM_N  },
    { "SGS M255",       "1970s  pins NAMED",      "the only chip whose pins the datasheet names",
      RB_M255, RB_M255_N },
    // the M254 pinout sends 8 of its 12 outputs to the M251 chord/bass chip, so most of what this
    // cart plays as a drum here is really accompaniment. Say so rather than let it mislead.
    { "SGS M254",       "1970s  accompaniment",   "8 of 12 pins fed the CHORD/BASS chip: not drums",
      RB_M254, RB_M254_N },
};
#define NMACH ((int)(sizeof MACH / sizeof MACH[0]))

// Lane -> voice, and this is the one judgement call in the cart.
//
// The lane labels in the sources are PROVISIONAL, not unknown: the FR-2L scan is about
// 75 dpi, so its letterforms are interpolation rather than ink (doc §6.1). That makes a
// label WEAK EVIDENCE, which is worth more than ignoring it and less than trusting it.
// So the default map reads the printed label as a HINT, and V rotates the whole map when
// the hint is wrong or you simply want a different kit. Purely order-based defaults were
// tried first and put the bass drum on the backbeat of every latin rhythm, which sounds
// wrong for a reason that has nothing to do with the data.
//
// The Side Man bank is also a decade EARLY for these charts (1959 tubes against a 1969
// transistor box and a 1970s chip) and has no snare at all, so a wire brush stands in.
// Nothing here claims to be the machine's own voice: the patterns are sourced, the
// voicing is the closest thing on the shelf.
static const int VORDER[SM_NV] = {
    SM_BASS, SM_BRUSH, SM_MARACAS, SM_CLAVES, SM_TOM1, SM_WOOD, SM_TEMP1, SM_CYMBAL,
    SM_TOM2, SM_TEMP2,
};
static int voice_hint(const char *lab, int lane) {
    char t[8]; int n = 0;
    for (const char *p = lab; *p && n < 7; p++) {                 // first token of "Cb+Hb"
        if (*p == '+' || *p == ' ' || *p == '-') break;
        t[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
    }
    t[n] = 0;
    bool primed = false;
    for (const char *p = lab; *p; p++) if (*p == '\'') primed = true;
    // two-letter tokens first: "cy" must not be read as "c" (claves)
    if (n >= 2) {
        if (t[0]=='b' && t[1]=='d') return SM_BASS;
        if (t[0]=='s' && t[1]=='d') return primed ? SM_TEMP1 : SM_BRUSH;   // no snare in this bank
        if (t[0]=='c' && t[1]=='y') return primed ? SM_BRUSH  : SM_CYMBAL;
        if (t[0]=='h' && t[1]=='h') return SM_MARACAS;
        if (t[0]=='c' && t[1]=='b') return SM_WOOD;                        // cowbell -> wood block
        if (t[0]=='h' && t[1]=='b') return SM_TOM1;                        // high bongo
        if (t[0]=='l' && t[1]=='b') return SM_TOM2;                        // low bongo
        if (t[0]=='h' && t[1]=='c') return SM_TOM1;                        // high conga
        if (t[0]=='l' && t[1]=='c') return SM_TOM2;                        // low conga
        if (t[0]=='r' && t[1]=='s') return SM_CLAVES;                      // rim shot
        if (t[0]=='t' && t[1]=='b') return SM_MARACAS;                     // tambourine
        if (t[0]=='g' && t[1]=='u') return SM_MARACAS;                     // guiro
        if (t[0]=='o' && t[1]=='u') return VORDER[lane % SM_NV];           // "OUT n": a chip pin
        if (t[0]=='m' && t[1]=='e') return SM_WOOD;                        // metronome: a click
    }
    if (n == 1) {
        if (t[0]=='m') return SM_MARACAS;
        if (t[0]=='c') return SM_CLAVES;
        if (t[0]=='b') return SM_BRUSH;
    }
    return VORDER[lane % SM_NV];
}

#define MAXC 48

// ── the accompaniment half ───────────────────────────────────────────────
// The Hammond patent (doc §4d) charts CHORD, HIGH BASS and LOW BASS as GATE lanes: the pattern
// decides WHEN the chord the player is holding sounds, and the bass root and fifth are derived
// from it "by frequency division". So the cart holds a chord and the lanes gate it. That is the
// mechanism, not a substitute for it: no note data is needed or invented.
#define SL_ORGAN 12
#define SL_BASS  13
static int  root  = 45;        // A2, transposable
static bool minor = true;

static int   mach = 0, rhy = 0;
static bool  running = false, keyheld = false;
static float clock_hz = 8.0f;          // the machine's own variable clock, ticks/second
static int   order[MAXC], nord = 0;    // the counts this rhythm actually uses, in play order
static int   sidx = 0;                 // index into order[]
static float acc = 0.0f;               // seconds toward the next tick
static int   vrot = 0;                 // voice-map rotation
static bool  metro = false;            // the metronome lane: a click for the player, switchable
static int   flash[SM_NV];
static int   lastfire[SM_NV];

static const RbRhythm *cur(void) { return &MACH[mach].set[rhy]; }

// Does this rhythm drive the accompaniment at all? Only the Hammond patent and the M255
// chart CHORD/BASS lanes, so on every other machine Z/X/N are inert and the UI says so.
static bool has_acc(void) {
    const RbRhythm *r = cur();
    for (int l = 0; l < r->nlanes; l++)
        if (r->lane[l].role & (RB_ROLE_CHORD | RB_ROLE_BASSLO | RB_ROLE_BASSHI)) return true;
    return false;
}

// The TR-77 prints a METRONOME lane beside each rhythm. It is a click for the player,
// not part of the kit, and the machine could switch it off, so this cart does too
// (default off). Playing it as a drum doubles the hits and is simply wrong.
static bool is_metro(const char *lab) {
    return (lab[0] == 'M' || lab[0] == 'm') && (lab[1] == 'e' || lab[1] == 'E');
}

// the hinted voice, then rotated by V. vrot == 0 is the hint itself.
static int lane_voice(int lane) {
    const RbRhythm *r = cur();
    int v = voice_hint(r->lane[lane].label, lane);
    return vrot ? VORDER[(v + vrot) % SM_NV] : v;
}

// A lane FIRES on a count if the machine would actually trigger it there. Two rules,
// both from the sources: an RB_GATE lane is held, so it fires only where its run
// STARTS; and on an RB_EDGE_ONLY rhythm a mark whose predecessor is also marked is
// already sounding and cannot retrigger.
static int fires(const RbRhythm *r, int lane, int c) {
    if (!rb_hit(r, lane, c)) return 0;
    if (r->lane[lane].kind == RB_GATE)
        return !rb_hit(r, lane, (c + r->counts - 1) % r->counts);
    return rb_trigger(r, lane, c);
}

static void load_rhythm(void) {
    const RbRhythm *r = cur();
    nord = 0;
    for (int c = 0; c < r->counts && nord < MAXC; c++)
        if (rb_used(r, c)) order[nord++] = c;
    if (nord == 0) { order[0] = 0; nord = 1; }   // defensive: a rhythm is never all-skipped
    sidx = 0; acc = 0.0f;
}

static void audition(int lane) {
    const RbRhythm *r = cur();
    if (lane >= r->nlanes) return;
    if (r->lane[lane].flags & RB_RESETCOL) return;    // a counter, not a drum
    int v = lane_voice(lane);
    sideman_fire(SIDEMAN_BASE, v, 0, 0);
    flash[lane] = 6;
}

void init(void) {
    sideman_build(SIDEMAN_BASE);
    instrument(SL_ORGAN, INSTR_SQUARE, 4, 40, 5, 90);    // a thin drawbar-ish chord
    instrument(SL_BASS,  INSTR_TRI,    2, 60, 4, 120);   // the divided bass
    instrument_level(SL_ORGAN, 3);   // three notes at once, so keep each one modest
    instrument_level(SL_BASS,  4);   // measured: 4 keeps the mix near -6 dBFS peak
    reverb(0.45f, 0.6f);
    load_rhythm();
}

void update(void) {
    for (int i = 0; i < SM_NV; i++) { if (flash[i]) flash[i]--; if (lastfire[i]) lastfire[i]--; }

    if (keyp(KEY_SPACE)) { running = !running; if (running) { sidx = 0; acc = 0.0f; } }
    // the organ key: these machines started when you PLAYED, not when you pressed run
    bool k = key('K');
    if (k && !keyheld) { keyheld = true; sidx = 0; acc = 0.0f; }
    if (!k) keyheld = false;

    if (keyp(KEY_LEFT))  { rhy = (rhy + MACH[mach].n - 1) % MACH[mach].n; load_rhythm(); }
    if (keyp(KEY_RIGHT)) { rhy = (rhy + 1) % MACH[mach].n;                load_rhythm(); }
    if (keyp('[')) { mach = (mach + NMACH - 1) % NMACH; rhy = 0; load_rhythm(); }
    if (keyp(']')) { mach = (mach + 1) % NMACH;         rhy = 0; load_rhythm(); }
    if (keyp(KEY_UP))   { clock_hz += 0.5f; if (clock_hz > 24.0f) clock_hz = 24.0f; }
    if (keyp(KEY_DOWN)) { clock_hz -= 0.5f; if (clock_hz <  1.0f) clock_hz =  1.0f; }
    if (keyp('V')) vrot = (vrot + 1) % SM_NV;
    if (keyp('M')) metro = !metro;
    if (keyp('Z')) { root -= 1; if (root < 33) root = 33; }
    if (keyp('X')) { root += 1; if (root > 57) root = 57; }
    if (keyp('N')) minor = !minor;
    for (int i = 0; i < 8; i++) if (keyp('1' + i)) audition(i);

    if (!(running || keyheld)) return;

    // the clock. One tick per USED count, so a rhythm that skips states (the TR-77
    // hatches them) really is shorter in time, which is where its meter comes from.
    const RbRhythm *r = cur();
    acc += 1.0f / 60.0f;
    float period = 1.0f / clock_hz;
    while (acc >= period) {
        acc -= period;
        sidx = (sidx + 1) % nord;
        int c = order[sidx];
        for (int l = 0; l < r->nlanes && l < SM_NV; l++) {
            if (r->lane[l].flags & RB_RESETCOL) continue;
            if (is_metro(r->lane[l].label) && !metro) continue;
            if (!fires(r, l, c)) continue;
            int ro = r->lane[l].role;
            if (ro & RB_ROLE_CHORD) {                       // gate the held chord
                hit(root + 12,                    SL_ORGAN, 3, 150);
                hit(root + 12 + (minor ? 3 : 4),  SL_ORGAN, 3, 150);
                hit(root + 12 + 7,                SL_ORGAN, 3, 150);
            }
            if (ro & RB_ROLE_BASSLO) hit(root,     SL_BASS, 5, 170);   // the root
            if (ro & RB_ROLE_BASSHI) hit(root + 7, SL_BASS, 5, 170);   // the fifth, divided
            if (ro & RB_ROLE_DRUM)   sideman_fire(SIDEMAN_BASE, lane_voice(l), 0, 0);
            flash[l] = 5; lastfire[l] = 5;
        }
    }
}

// ── drawing ──────────────────────────────────────────────────────────────
#define GX 76   // wide enough for a 7-char printed label AND a 6-char voice name before it
#define GY 62
#define CW 5
#define RH 11   // nominal; draw_grid() shrinks it so every lane fits (the M254 has 12)

static void draw_grid(void) {
    const RbRhythm *r = cur();
    int cw = (r->counts <= 16) ? 14 : (r->counts <= 32) ? 7 : CW;
    int playc = order[sidx];
    int rh = (r->nlanes > 10) ? 8 : RH;          // 12 lanes still clear the 3-line footer

    for (int l = 0; l < r->nlanes; l++) {
        int y = GY + l * rh;
        bool resetcol = (r->lane[l].flags & RB_RESETCOL) != 0;

        // the printed label DIM (provisional, see de:meta) then the voice BRIGHT. The label is
        // TRUNCATED to the 6 characters the column holds: the Hammond chart's labels run to 20
        // ("CHORD & SNARE DRUM") and used to overprint the voice name.
        char lb[8];
        if (r->lane[l].label[0] == 'O' && r->lane[l].label[1] == 'U') {
            // "OUTPUT 11" truncates to "OUTPUT " on every lane, which tells you nothing: keep the pin
            snprintf(lb, sizeof lb, "OUT %s", r->lane[l].label + 7);
        } else {
            for (int i = 0; i < 7; i++) { char c = r->lane[l].label[i]; lb[i] = c ? c : 0; if (!c) break; }
            lb[7] = 0;
        }
        print(lb, 2, y + 2, CLR_DARKER_GREY);
        bool met = is_metro(r->lane[l].label);
        if (resetcol) {
            print("rst", 40, y + 2, CLR_DARK_GREY);
        } else if (r->lane[l].role & (RB_ROLE_CHORD | RB_ROLE_BASSLO | RB_ROLE_BASSHI)) {
            int ro = r->lane[l].role;
            const char *w = (ro & RB_ROLE_CHORD) ? "CHORD" : (ro & RB_ROLE_BASSLO) ? "BASSlo" : "BASShi";
            print(w, 40, y + 2, flash[l] ? CLR_WHITE : CLR_LIME_GREEN);
        } else if (met) {
            print(metro ? "click" : "off", 40, y + 2, metro ? CLR_PEACH : CLR_DARKER_GREY);
        } else {
            int v = lane_voice(l);
            print(SIDEMAN_SHORT[v], 40, y + 2, flash[l] ? CLR_WHITE : CLR_MEDIUM_GREY);
        }

        for (int c = 0; c < r->counts; c++) {
            int x = GX + c * cw;
            if (!rb_used(r, c)) {                       // a state this rhythm steps over
                line(x + 1, y + 7, x + cw - 2, y + 1, CLR_DARKER_GREY);
                continue;
            }
            if (rb_hit(r, l, c)) {
                bool snd = fires(r, l, c) != 0;
                bool unc = rb_uncertain(r, l, c) != 0;
                int col = unc ? CLR_INDIGO : (r->lane[l].kind == RB_GATE ? CLR_ORANGE : CLR_YELLOW);
                if (met && !metro) col = CLR_DARK_GREY;
                if (snd) rectfill(x + 1, y + 2, cw - 2, 6, col);       // fires
                else     rect(x + 1, y + 2, cw - 2, 6, CLR_DARK_GREY); // already sounding: hollow
            } else if (c % r->per_beat == 0) {
                pset(x + cw / 2, y + 5, CLR_DARK_GREY);                // beat tick
            }
        }
    }
    // the playhead
    int px = GX + playc * cw;
    int gh = r->nlanes * rh;
    rectfill(px, GY - 3, cw, 2, (running || keyheld) ? CLR_RED : CLR_DARK_GREY);
    line(px + cw / 2, GY - 1, px + cw / 2, GY + gh, CLR_DARK_RED);
}

static void draw_panel(void) {
    const RbRhythm *r = cur();
    print(MACH[mach].name, 2, 4, CLR_WHITE);
    print(MACH[mach].era, 2, 12, CLR_DARK_GREY);
    // the standing disclaimer, moved out of the footer to free a legend line for the keys
    print("patterns from the service manuals", 150, 4, CLR_DARKER_GREY);
    print("the voicing is this cart's guess", 150, 12, CLR_DARKER_GREY);
    print(r->name, 2, 24, CLR_LIME_GREEN);
    // the provenance line, but only when it says more than the name already does
    if (strcmp(r->source, r->name) != 0 && !MACH[mach].note)
        print(r->source, 2, 54, CLR_DARKER_GREY);

    // what the machine's clock MEANS for this rhythm, all read off the data
    char buf[96];
    snprintf(buf, sizeof buf, "%d counts  %d/beat  %d bar%s", r->counts, r->per_beat,
             r->bars, r->bars == 1 ? "" : "s");
    print(buf, 2, 34, CLR_MEDIUM_GREY);
    snprintf(buf, sizeof buf, "clock %.1f/s = %d bpm", clock_hz,
             (int)(clock_hz * 60.0f / (float)(r->per_beat ? r->per_beat : 1)));
    print(buf, 2, 44, CLR_PEACH);
    if (nord != r->counts) {
        snprintf(buf, sizeof buf, "steps %d of %d (hatched)", nord, r->counts);
        print(buf, 150, 44, CLR_ORANGE);
    }
    if (r->flags & RB_EDGE_ONLY) print("edge-trig", 150, 34, CLR_DARK_GREY);
    if (MACH[mach].note) print(MACH[mach].note, 2, 54, CLR_DARK_ORANGE);

    // Three legend lines, each written to say what a key DOES rather than abbreviate its
    // name — the old single line abbreviated so hard ("ZX=key") that the chord transpose was
    // unlearnable from the screen, and it ran 6px off a 320px canvas besides. FONT_SMALL
    // advances 5px/char, so a line starting at x=2 holds 63 characters and no more.
    int y = 171;
    print((running || keyheld) ? "RUN" : "STOP", 2, y, (running || keyheld) ? CLR_RED : CLR_DARK_GREY);
    print("SPACE run/stop   K = organ key: plays while held", 26, y, CLR_MEDIUM_GREY);
    print("< > rhythm   [ ] machine   ^ v clock   1-8 hear one lane", 2, y + 9, CLR_DARKER_GREY);
    print("V voice map   M metronome   ", 2, y + 18, CLR_DARKER_GREY);
    // The chord row carries the LIVE chord and lights up only on the machines that have
    // accompaniment lanes, so it answers "what are these keys", "do they do anything here"
    // and "what is held right now" in one place. It sits next to its own keys rather than
    // up beside the rhythm name, where the longest name (M255 COUNTRY WESTERN, 157px) would
    // have run straight under it.
    {
        static const char *NOTE[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        char b2[48];
        if (has_acc()) snprintf(b2, sizeof b2, "chord %s%s: Z X transpose  N min/maj",
                                NOTE[root % 12], minor ? "m" : "");
        else           snprintf(b2, sizeof b2, "chord: Z X transpose  N min/maj");
        print(b2, 2 + 28 * 5, y + 18, has_acc() ? CLR_LIME_GREEN : CLR_DARKER_GREY);
    }
}

void draw(void) {
    cls(CLR_BLACK);
    font(FONT_SMALL);
    draw_panel();
    draw_grid();
}

#ifdef DE_SPEC
#include "spec.h"
// The sequencing rests on four properties, all PURE (no audio clock, which is frozen
// under -DDE_SPEC). Each is asserted once per machine over every rhythm, lane and
// count, counting violations rather than emitting an assertion per cell: 76 rhythms
// would otherwise drown the report, and a count of 0 says the same thing.
void spec(void) {
    for (int m = 0; m < NMACH; m++) {
        int bad_order = 0, bad_len = 0, fire_skipped = 0, double_fire = 0, played_reset = 0;
        int total_fires = 0, gate_lanes = 0;
        for (int i = 0; i < MACH[m].n; i++) {
            const RbRhythm *r = &MACH[m].set[i];
            int used = 0;
            for (int c = 0; c < r->counts; c++) used += rb_used(r, c);
            mach = m; rhy = i; load_rhythm();
            if (nord != used) bad_len++;
            for (int s = 0; s < nord; s++) if (!rb_used(r, order[s])) bad_order++;
            for (int l = 0; l < r->nlanes && l < SM_NV; l++) {
                if (r->lane[l].kind == RB_GATE) gate_lanes++;
                for (int c = 0; c < r->counts; c++) {
                    int f = fires(r, l, c);
                    total_fires += f;
                    if (f && !rb_used(r, c)) fire_skipped++;
                    if ((r->flags & RB_EDGE_ONLY) && f && fires(r, l, (c + 1) % r->counts)) double_fire++;
                    if ((r->lane[l].flags & RB_RESETCOL) && rb_hit(r, l, c)) played_reset++;
                }
            }
        }
        // 1. the play order is exactly the states the rhythm uses, in order
        expect(bad_len == 0,   "play order length equals the used-count count");
        expect(bad_order == 0, "play order never contains a skipped state");
        // 2. nothing fires on a state the machine steps over
        expect(fire_skipped == 0, "no lane fires on a hatched/skipped state");
        // 3. an edge-triggered machine never fires a lane on two consecutive states
        expect(double_fire == 0, "edge-only rhythms never retrigger on adjacent states");
        // 4. a reset column is a counter, so it can carry no mark at all
        expect(played_reset == 0, "a reset-column lane is never a drum");
        // liveness: a machine that fires nothing would pass all of the above vacuously
        expect(total_fires > 20, "this machine actually fires something (not a vacuous pass)");
    }
    // the FR-2L corpus invariant, from the chart's own ruling: counts congruent to
    // 1 or 5 (mod 6) are UNRULED on the page and can never carry a mark
    int unruled = 0;
    for (int i = 0; i < RB_FR2L_N; i++) {
        const RbRhythm *r = &RB_FR2L[i];
        for (int l = 0; l < r->nlanes; l++)
            for (int c = 0; c < r->counts; c++)
                if (rb_hit(r, l, c) && (c % 6 == 1 || c % 6 == 5)) unruled++;
    }
    expect(unruled == 0, "no FR-2L mark falls on a count the chart never ruled");
    mach = 0; rhy = 0; load_rhythm();
}
#endif
