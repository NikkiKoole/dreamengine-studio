/* de:meta
{
  "slug": "epianojam",
  "title": "epiano jam",
  "status": "active",
  "created": "2026-08-13",
  "kind": [
    "tech-demo",
    "toy"
  ],
  "teaches": [
    "algorithm-visualization"
  ],
  "lineage": "Made for fun, to point the new MIDI-out seam at something musical: harmony.h generates the progression, midi_output.h sends it, and the sound is whatever instrument you point it at -- an ePiano in GarageBand was the first. The cart the maker asked for after midiout proved the wire works: 'can you create a temp cart that midi creates interesting epiano chord progressions to play them in garageband'.",
  "description": {
    "summary": "Generates jazz/pop chord progressions with the shared harmony brain and plays them OUT over MIDI -- point it at a piano in any DAW and it jams on its own.",
    "detail": "The cart makes no sound itself; harmony.h picks the chords and your DAW's instrument plays them. Thirteen styles are on the number keys -- bossa, cocktail, city pop, minor pop, blues, cinematic, and the modal ones -- each a different set of WEIGHTS over the same roman-numeral vocabulary, so the same brain sounds like a lounge or like a sad-pop record depending on which table it reads. What makes it listenable rather than a chord-quiz is the VOICING: each new pitch is matched to whichever of the previous chord's voices sits nearest it, so shared tones stay exactly where they are and only the rest move a step or two -- that is the difference between 'a computer naming chords' and 'someone playing piano'. Doing it the obvious way instead, tone 1 to voice 1, looks the same in the code and comes out as parallel blocks. Three feels (block, arpeggio, offbeat stabs) and a bass note underneath. The panel shows the roman numeral, the chord name, and the actual MIDI notes going out.",
    "controls": "SPACE plays/stops. 1-9 and 0 pick the style (Q/W/E for three more). LEFT/RIGHT tempo, UP/DOWN transpose the key. F cycles the feel (block / arp / stabs). B toggles the bass note."
  }
}
de:meta */
#include "studio.h"
#include "harmony.h"
#include <stdio.h>

// EPIANO JAM — the musical face of runtime/midi_output.h.
//
// Point a DAW's piano at it: open a software-instrument track, pick an electric piano, select the
// track (only the SELECTED track monitors live MIDI), then press SPACE here.
//
// Everything goes out on ONE channel, deliberately. GarageBand is not multi-timbral — every
// channel lands on the selected track's instrument — so a cart meant to be played THROUGH a piano
// should not scatter itself across channels. (midiout is the cart that demonstrates the channel
// convention; this one is the cart that makes music.)

#define CH       1       // one channel: see above
#define NVOICE   4       // chord tones per chord (harmony.h gives 4)
#define CENTRE   64      // ~E4 — where the right hand sits; voicing gravitates here

// ── the styles, in the order the number keys select them ──
typedef struct { const HbStyle *st; const char *name; } Style;
static const Style STYLES[] = {
    { &HB_BOSSA,        "bossa"     }, { &HB_COCKTAIL,     "lounge"   },
    { &HB_CITYPOP,      "citypop"   }, { &HB_POP,          "pop"      },
    { &HB_MINPOP,       "sadpop"    }, { &HB_BLUES,        "blues"    },
    { &HB_CINEMATIC,    "cinema"    }, { &HB_FOLK,         "folk"     },
    { &HB_JINGLE,       "jingle"    }, { &HB_DORIAN_STYLE, "dorian"   },
    { &HB_MIXO_STYLE,   "mixo"      }, { &HB_PHRYG_STYLE,  "phryg"    },
    { &HB_LYDIAN_STYLE, "lydian"    },
};
#define NSTYLE ((int)(sizeof STYLES / sizeof STYLES[0]))

enum { FEEL_BLOCK, FEEL_ARP, FEEL_STAB, FEEL_N };
static const char *FEEL_NAME[FEEL_N] = { "block", "arp", "stabs" };

static const char *PC_NAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

static int playing = 0, tempo = 96, keyPc = 0, styleIx = 0, feel = FEEL_BLOCK, want_bass = 1;
static int fn = HB_I;                    // current harmonic function
static int voiced[NVOICE];               // the MIDI notes of the current chord, voiced
static int bass_note = -1;
static int cur_bar = -1, last_slot = -1;

// pending note-offs, so every note has a real length and nothing is left hanging.
// The lesson from midiout: a zero-length note is legal, balanced, and records WRONG.
#define NPEND 32
static struct { int note, left; } pend[NPEND];

static void play_note(int note, int vel, int frames) {
    if (note < 0 || note > 127) return;
    midi_send_note(CH, note, vel, 1);
    for (int i = 0; i < NPEND; i++)
        if (pend[i].left <= 0) { pend[i].note = note; pend[i].left = frames; return; }
    midi_send_note(CH, note, 0, 0);      // table full: release now rather than hang it
}
static void pend_tick(void) {
    for (int i = 0; i < NPEND; i++)
        if (pend[i].left > 0 && --pend[i].left == 0) midi_send_note(CH, pend[i].note, 0, 0);
}
static void pend_flush(void) {
    for (int i = 0; i < NPEND; i++)
        if (pend[i].left > 0) { midi_send_note(CH, pend[i].note, 0, 0); pend[i].left = 0; }
}

static const HbVocab *vocab_of(const HbStyle *st) { return st->vocab ? st->vocab : &HB_MAJOR; }

// ── VOICE LEADING — the thing that makes this sound played rather than spelled ──
// harmony.h hands back four PITCH CLASSES (0..11). Dropping them into a fixed octave gives
// parallel block chords that lurch around the keyboard: correct, and immediately robotic. Good
// voicing holds whatever tones the two chords share and moves the rest by a step or two, which is
// what a pianist's hand actually does. Cheap version of what rad_lead_to does for the radio carts,
// kept local because that one is wired to radio.h's voice model and this cart owns nothing but
// MIDI note numbers.

// Nearest MIDI note of pitch class `pc` to `near`, kept inside the right-hand register.
static int nearest_pc(int pc, int near) {
    int base = near - (near % 12) + pc, best = base, bestd = 999;
    for (int o = -24; o <= 24; o += 12) {
        int cand = base + o, d = cand > near ? cand - near : near - cand;
        if (cand < 48 || cand > 84) continue;
        if (d < bestd) { bestd = d; best = cand; }
    }
    while (best < 48) best += 12;      // every octave was out of register: fold back in
    while (best > 84) best -= 12;
    return best;
}

static void voice_chord(int f) {
    int pcs[NVOICE];
    hb_vocab_pcs(vocab_of(STYLES[styleIx].st), keyPc, f, pcs);

    int prev[NVOICE];
    for (int i = 0; i < NVOICE; i++) prev[i] = voiced[i] > 0 ? voiced[i] : CENTRE + (i - 1) * 4;

    // ⚠ The assignment is the whole trick, and getting it wrong is silent. The first version put
    // chord tone i into voice i — root→voice0, 3rd→voice1, and so on. That still picks a nearby
    // octave, so it LOOKS like voice leading, but when the chord changes every tone rotates by the
    // same interval and so does every voice: measured on the wire, Am7→Dm7 moved all four voices
    // +5 semitones in parallel, which is the exact block-chord lurch the voicing exists to avoid.
    // Real voice leading matches each new PITCH to its nearest PREVIOUS VOICE regardless of which
    // chord tone it is — so the common tones stay put and only the rest move a step or two.
    int costs[NVOICE][NVOICE], place[NVOICE][NVOICE];
    for (int i = 0; i < NVOICE; i++)
        for (int j = 0; j < NVOICE; j++) {
            place[i][j] = nearest_pc(pcs[j], prev[i]);
            int d = place[i][j] - prev[i];
            costs[i][j] = d < 0 ? -d : d;
        }

    int usedV[NVOICE] = {0}, usedT[NVOICE] = {0};
    for (int k = 0; k < NVOICE; k++) {          // greedy: repeatedly take the cheapest free pairing
        int bi = -1, bj = -1, bc = 9999;
        for (int i = 0; i < NVOICE; i++) if (!usedV[i])
            for (int j = 0; j < NVOICE; j++) if (!usedT[j])
                if (costs[i][j] < bc) { bc = costs[i][j]; bi = i; bj = j; }
        if (bi < 0) break;
        usedV[bi] = usedT[bj] = 1;
        voiced[bi] = place[bi][bj];
    }

    // root in the bass, well below the voicing so it reads as a left hand, not a fifth voice
    bass_note = 36 + ((keyPc + vocab_of(STYLES[styleIx].st)->off[f]) % 12);
}

static void next_chord(void) {
    const HbStyle *st = STYLES[styleIx].st;
    if (fn >= st->nfunc) fn = 0;                      // style speaks less of the vocab than the last one
    fn = hb_pick(st, fn, rnd(hb_nopts(st, fn)));
    voice_chord(fn);
}

static double beats_now(void) { return frame() / 60.0 * (tempo / 60.0); }

void update(void) {
    pend_tick();

    if (keyp(' ')) {
        playing = !playing;
        if (playing) { midi_send_start(1); cur_bar = -1; last_slot = -1; }
        else         { pend_flush(); midi_send_stop(); }
    }
    if (keyp(KEY_RIGHT) && tempo < 200) tempo += 4;
    if (keyp(KEY_LEFT)  && tempo > 40)  tempo -= 4;
    if (keyp(KEY_UP))   { pend_flush(); keyPc = (keyPc + 1) % 12; }
    if (keyp(KEY_DOWN)) { pend_flush(); keyPc = (keyPc + 11) % 12; }
    if (keyp('F')) feel = (feel + 1) % FEEL_N;
    if (keyp('B')) want_bass = !want_bass;
    for (int i = 0; i < 10 && i < NSTYLE; i++)
        if (keyp('0' + ((i + 1) % 10))) { styleIx = i; fn = HB_I; }
    if (keyp('Q') && NSTYLE > 10) styleIx = 10;
    if (keyp('W') && NSTYLE > 11) styleIx = 11;
    if (keyp('E') && NSTYLE > 12) styleIx = 12;

    if (!playing) return;

    double beats = beats_now();
    int    b     = (int)(beats / 4.0);          // one chord per cur_bar of 4
    int    slot  = (int)(beats * 2.0);          // eighth-note grid
    int    ineighth = slot % 8;                 // position within the cur_bar

    if (b != cur_bar) {                              // new cur_bar → new chord
        cur_bar = b;
        next_chord();
        if (want_bass) play_note(bass_note, 78, 26);   // left hand, held most of the cur_bar
        if (feel == FEEL_BLOCK)
            for (int i = 0; i < NVOICE; i++) play_note(voiced[i], 72, 26);
    }

    if (slot != last_slot) {
        last_slot = slot;
        if (feel == FEEL_ARP) {
            // roll through the voices, one per eighth, wrapping — a gentle broken chord
            int i = ineighth % NVOICE;
            play_note(voiced[i], ineighth == 0 ? 82 : 64, 14);
        } else if (feel == FEEL_BLOCK && ineighth == 4) {
            // re-strike on beat 3: one hit per bar reads as a chord CHART, not as playing
            for (int i = 0; i < NVOICE; i++) play_note(voiced[i], 62, 24);
        } else if (feel == FEEL_STAB && (ineighth == 1 || ineighth == 3 || ineighth == 6)) {
            for (int i = 0; i < NVOICE; i++) play_note(voiced[i], 70, 8);
        }
    }

#ifdef DE_TRACE
    watch("fn",   "%d", fn);
    watch("bar",  "%d", cur_bar);
    watch("v0",   "%d", voiced[0]);
    watch("bass", "%d", bass_note);
#endif
}

void draw(void) {
    cls(CLR_DARK_BLUE);
    char b[80];
    const HbStyle *st = STYLES[styleIx].st;
    const HbVocab *vb = vocab_of(st);

    print("EPIANO JAM", 8, 6, CLR_WHITE);
    // ALWAYS on screen, both lines. This cart is silent by design, and a silent cart reads as a
    // broken one — so it has to say, on its own face, that the sound is supposed to come out of
    // something else. Written for whoever opens this cold in a year with no idea why it does
    // nothing. (The screen is 320px on an 8px font = 40 characters; every line here is counted.)
    print("makes NO sound - it sends MIDI OUT", 8, 16, CLR_INDIGO);
    print("to a DAW: GarageBand, Logic, Ableton", 8, 26, CLR_INDIGO);
    bool live = midi_out_ready();
    print(live ? "out: dreamengine (pick me as the input)"
               : "out: not sending (run it natively)", 8, 38, live ? CLR_GREEN : CLR_MEDIUM_GREY);

    snprintf(b, sizeof b, "%s   key %s   %d bpm", STYLES[styleIx].name, PC_NAME[keyPc], tempo);
    print(b, 8, 52, CLR_YELLOW);
    snprintf(b, sizeof b, "feel %s   bass %s", FEEL_NAME[feel], want_bass ? "on" : "off");
    print(b, 8, 62, CLR_LIGHT_GREY);

    // the chord, big-ish: roman numeral + real name
    if (playing) {
        int root = (keyPc + vb->off[fn]) % 12;
        snprintf(b, sizeof b, "%s   %s%s", vb->fname[fn], PC_NAME[root], hb_qname[vb->qual[fn]]);
        print(b, 8, 78, CLR_ORANGE);

        snprintf(b, sizeof b, "notes  %d %d %d %d   bass %d",
                 voiced[0], voiced[1], voiced[2], voiced[3], want_bass ? bass_note : -1);
        print(b, 8, 90, CLR_MEDIUM_GREY);

        // a little keyboard: which of the 12 pitch classes are sounding
        for (int i = 0; i < 12; i++) {
            int on = 0;
            for (int v = 0; v < NVOICE; v++) if (voiced[v] % 12 == i) on = 1;
            rectfill(8 + i * 14, 102, 12, 12, on ? CLR_WHITE : CLR_DARK_GREY);
            print(PC_NAME[i], 9 + i * 14, 116, on ? CLR_WHITE : CLR_DARKER_GREY);
        }
    } else {
        print("1. add a software instrument track", 8, 78, CLR_WHITE);
        print("2. select it (only that one listens)", 8, 88, CLR_WHITE);
        print("3. press SPACE here", 8, 98, CLR_WHITE);
    }

    print("styles:", 8, 126, CLR_MEDIUM_GREY);
    for (int i = 0; i < NSTYLE; i++) {
        int x = 8 + (i % 4) * 78, y = 136 + (i / 4) * 10;
        // 1..9 then 0 for the tenth — matching the keyp() bindings above. Writing '1'+i for all
        // ten runs past '9' and labels the tenth style ':', which is not a key anyone can press.
        char k = i < 9 ? (char)('1' + i) : i == 9 ? '0' : "QWE"[i - 10];
        snprintf(b, sizeof b, "%c %s", k, STYLES[i].name);
        print(b, x, y, i == styleIx ? CLR_WHITE : CLR_DARK_GREY);
    }

    print("SPACE play   1-9/QWE style", 8, SCREEN_H - 20, CLR_DARK_GREY);
    print("F feel   B bass   <> bpm   ^v key", 8, SCREEN_H - 10, CLR_DARK_GREY);
}
