#include "studio.h"
#include <stdio.h>
#include <math.h>

// ── JANGLE RADIO ──────────────────────────────────────────────────────────
// The afternoon sibling of bossa radio and ambient radio: an endless FM
// station of feel-good mixolydian slacker pop — the Mac DeMarco / One Wayne G
// homage. Lots and lots of very simple songs: a 2-4 chord vamp, NO bridge,
// a wobbly chorused guitar, a drum machine that runs a touch loose against
// the bass, and a lazy slidey lead that wanders in for a few loops and
// wanders off again. Most songs are titled like the tape was dated, not named.
//
// Third chord brain for docs/guides/game-music.md (after bossa's Markov
// functions and ambient's modal drift): the VAMP — pick 2-4 in-mode chords
// once, loop them all song, and let the ARRANGEMENT (layers in/out per loop)
// be the form instead of the harmony.
//
// New tricks probed here:
//   • chorus warble — a constant 5.5Hz pitch LFO on the guitar slot. THE sound.
//   • loose kit — drums scheduled +0..10ms behind the bass grid (groove
//     template from the guide), now audible thanks to sample-exact firing.
//   • tape tempo — bpm() wobbled ±1 at bar lines; the song breathes.
//   • legato lead — ONE held voice, note_pitch + note_glide slides between
//     scale tones; phrases re-attack with a fresh note_on. No retriggers inside
//     a phrase: it's a whistle, not a keyboard.
//
//   SPACE next song   R play it again   [ ] song history   M radio on/off
//   LEFT/RIGHT feel (layers)   UP/DOWN tempo   H or ? help
//
// The seed pins the COMPOSITION (key, vamp, comping style, arrangement,
// tempo, title); the PERFORMANCE (strum lag, kit looseness, the lead's
// actual notes, tempo wobble) re-rolls every playthrough.

#define JANGLE_SEED 0   // pin a favourite song here (0 = free-roaming radio)

// ── instrument slots ──────────────────────────────────────────────────────
#define I_GTR   5   // chorused jangle pluck — the warble is the identity
#define I_BASS  6   // round and simple
#define I_LEAD  7   // thin slidey whistle (one held voice)
#define I_KICK  8
#define I_SNARE 9
#define I_HAT   10

// ── mixolydian world ──────────────────────────────────────────────────────
static const int MIXO[7] = { 0, 2, 4, 5, 7, 9, 10 };   // the whole song lives here
// vamp chord pool: scale degree + minor? (mixolydian: I IV bVII major; ii v vi minor)
static const int POOLDEG[6] = { 0, 6, 3, 4, 1, 5 };    // I, bVII, IV, v, ii, vi
static const int POOLMIN[6] = { 0, 0,  0, 1, 1, 1 };
static const int POOLW[10]  = { 1, 1, 1, 2, 2, 2, 3, 3, 4, 5 };  // weighted picks into POOLDEG
static const char *PCNAME[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

// guitar comping masks — one bar (16 steps), lazy downstrokes
static const int COMPS[3][6] = {
    { 0, 4, 8, 12, -1, -1 },   // flat quarters (the dumbest, often the best)
    { 0, 6, 8, 12, -1, -1 },   // push on the and-of-2
    { 0, 4, 8, 12, 14, -1 },   // + pickup on the and-of-4
};

// ── the generated song ────────────────────────────────────────────────────
typedef struct {
    int  keyPc;
    int  vampDeg[4], vampMin[4], vampN;  // the chords (2..4 of them)
    int  barsPer;                        // 1 or 2 bars per chord
    int  comp;                           // comping mask
    int  arp;                            // 1 = arpeggiate instead of strum
    int  color7;                         // 1 = add the b7 to major chords
    int  introL, grooveL, soloL, outroL; // arrangement, in vamp-loops
    char title[24];
    float freq;
    unsigned seed;
} Song;

static Song   sng;
static int    tempo     = 96;
static int    intensity = 2;     // 0 gtr+bass · 1 +drums · 2 +lead solos · 3 denser
static bool   radioOn   = true;
static bool   showHelp  = false;
static long   scheduled = -1;
static long   songBase  = 0;
static int    songCount = 0;
static double stepMs    = 156.0;
static int    gv[3]     = { 64, 67, 71 };   // guitar voices, voice-led
static bool   gvInit    = false;
static int    kitLag    = 5;     // ms the kit runs behind the bass (performance)
static float  vu        = 0;
static char   nowChord[4][8];    // the whole vamp, labeled

// lead solo state (driven from update(), legato)
static int    leadH = -1;        // held voice handle, -1 = silent
static int    leadPitch = 81;
static long   leadStep = -1;     // last 8th we considered

static int iabs(int v) { return v < 0 ? -v : v; }

// composition PRNG (xorshift32) — same contract as the other radios
static unsigned rngState = 1;
static unsigned srnd_u(void) {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return rngState;
}
static int srnd(int n) { return (int)(srnd_u() % (unsigned)n); }

// ── song generation ───────────────────────────────────────────────────────
static const char *TW[] = { "Patio", "Motorbike", "Sunny Boy", "Crybaby",
    "Slow Honey", "Porch Light", "Gone Swimming", "Old Dog", "Soft Serve",
    "Lazy Susan", "Screen Door", "Couch Nap" };

static void new_song(double pos, unsigned seed) {
    if (!seed) seed = ((unsigned)rnd(0x10000) << 16) ^ (unsigned)rnd(0x10000)
                      ^ (unsigned)frame() * 2654435761u;
    if (!seed) seed = 1;
    rngState = sng.seed = seed;

    sng.keyPc = srnd(12);
    sng.vampN = (srnd(10) < 4) ? 2 : 4;                 // 2 or 4 chords, that's it
    sng.vampDeg[0] = 0; sng.vampMin[0] = 0;             // always open on I
    for (int i = 1; i < sng.vampN; i++) {
        int p;
        do { p = POOLW[srnd(10)]; } while (POOLDEG[p] == sng.vampDeg[i - 1] && POOLMIN[p] == sng.vampMin[i - 1]);
        sng.vampDeg[i] = POOLDEG[p];
        sng.vampMin[i] = POOLMIN[p];
    }
    sng.barsPer = (srnd(10) < 6) ? 1 : 2;
    sng.comp    = srnd(3);
    sng.arp     = (srnd(10) < 3);                       // some songs arpeggiate
    sng.color7  = (srnd(10) < 4);                       // some songs add the b7

    sng.introL  = 2;
    sng.grooveL = 3 + srnd(3);
    sng.soloL   = 4 + srnd(3);
    sng.outroL  = 2;

    // One Wayne G move: most songs are just dated, not named
    if (srnd(100) < 70)
        snprintf(sng.title, sizeof sng.title, "%04d%02d%02d",
                 2017 + srnd(5), 1 + srnd(12), 1 + srnd(28));
    else
        snprintf(sng.title, sizeof sng.title, "%s", TW[srnd(12)]);
    sng.freq = 88.0f + srnd(190) * 0.1f;

    tempo = 86 + srnd(24);                              // 86..109
    bpm(tempo);
    songBase = (long)pos + 8;
    gvInit   = false;
    kitLag   = rnd_between(2, 10);                      // performance: today's drummer
    if (leadH >= 0) { note_off(leadH); leadH = -1; }
    songCount++;
}

// session history — [ and ] walk back through everything the radio played
static unsigned hist[64];
static int histN = 0, histPos = -1;

static void fresh_song(double pos) {
    new_song(pos, 0);
    if (histN == 64) { for (int i = 1; i < 64; i++) hist[i - 1] = hist[i]; histN--; }
    hist[histN++] = sng.seed;
    histPos = histN - 1;
}

// ── harmony ───────────────────────────────────────────────────────────────
static int vamp_bars(void)      { return sng.vampN * sng.barsPer; }
static int total_loops(void)    { return sng.introL + sng.grooveL + sng.soloL + sng.outroL; }
static int chord_of_bar(long b) { return (int)((b % vamp_bars()) / sng.barsPer); }

static int root_pc(int ci)  { return (sng.keyPc + MIXO[sng.vampDeg[ci]]) % 12; }

static void chord_pcs(int ci, int out[3]) {
    out[0] = root_pc(ci);
    out[1] = (out[0] + (sng.vampMin[ci] ? 3 : 4)) % 12;   // third
    out[2] = (out[0] + 7) % 12;                            // fifth
    if (sng.color7 && !sng.vampMin[ci]) out[2] = (out[0] + 10) % 12;  // b7 color
}

static void chord_label(char *out, int n, int ci) {
    snprintf(out, n, "%s%s%s", PCNAME[root_pc(ci)],
             sng.vampMin[ci] ? "m" : "",
             (sng.color7 && !sng.vampMin[ci]) ? "7" : "");
}

// the same nearest-tone voice leading as bossa.c / ambient.c — block #3
static void lead_voices(int ci) {
    int pcs[3];
    chord_pcs(ci, pcs);
    if (!gvInit) {
        for (int k = 0; k < 3; k++) {
            int target = 60 + k * 5;
            int dd = ((pcs[k] - target) % 12 + 18) % 12 - 6;
            gv[k] = target + dd;
        }
        gvInit = true;
    } else {
        bool used[3] = { false, false, false };
        for (int v = 0; v < 3; v++) {
            int bestJ = -1, bestC = 0, bestD = 99;
            for (int j = 0; j < 3; j++) {
                if (used[j]) continue;
                int dd = ((pcs[j] - gv[v]) % 12 + 18) % 12 - 6;
                if (iabs(dd) < bestD) { bestD = iabs(dd); bestJ = j; bestC = gv[v] + dd; }
            }
            used[bestJ] = true;
            gv[v] = bestC;
        }
    }
    for (int k = 0; k < 3; k++) {
        while (gv[k] < 55) gv[k] += 12;
        while (gv[k] > 79) gv[k] -= 12;
    }
}

// ── arrangement: which layers play in vamp-loop L ─────────────────────────
static bool drums_on(long L) {
    return intensity >= 1 && L >= sng.introL && L < total_loops() - sng.outroL;
}
static bool solo_on(long L) {
    return intensity >= 2 && L >= sng.introL + sng.grooveL
                          && L < sng.introL + sng.grooveL + sng.soloL;
}

// ── the step player ───────────────────────────────────────────────────────
static void play_step(long abs, double pos) {
    long s = abs - songBase;
    if (s < 0) return;
    int dly = (int)((abs - pos) * stepMs);
    if (dly < 1) dly = 1;
    int  step = (int)(s % 16);
    long bar  = s / 16;
    long L    = bar / vamp_bars();                      // which vamp loop
    int  ci   = chord_of_bar(bar);

    // tape tempo: breathe ±1 bpm at 4-bar lines (performance, not seed)
    if (step == 0 && bar % 4 == 0 && bar > 0) bpm(tempo + rnd(3) - 1);

    // BASS — root on 1 and 3, the and-of-4 walks to the next chord's root
    if (step == 0 || step == 8 || (step == 14 && chance(40))) {
        int b = 36 + root_pc(ci);
        int n = b, vol = 5;
        if (step == 8 && chance(35)) n = b + (sng.vampMin[ci] ? 3 : 4);   // lazy third
        if (step == 14) { n = 36 + root_pc(chord_of_bar(bar + 1)); vol = 3; }
        schedule_hit(dly, n, I_BASS, vol, (int)(stepMs * 5));
        vu += vol * 0.7f;
    }

    // GUITAR — strummed or arpeggiated, always with the warble
    if (sng.arp) {
        if (step % 2 == 0) {                            // 8th-note arpeggio
            lead_voices(ci);
            int v = gv[(step / 2) % 3];
            schedule_hit(dly + rnd(6), v, I_GTR, 3, (int)(stepMs * 1.8));
            vu += 1.2f;
        }
    } else {
        for (int i = 0; i < 6; i++) {
            if (COMPS[sng.comp][i] != step) continue;
            lead_voices(ci);
            int vol = (step == 0) ? 4 : 3;
            for (int k = 0; k < 3; k++)                 // lazy 12ms downstroke
                schedule_hit(dly + k * 12 + rnd(5), gv[k], I_GTR, vol, (int)(stepMs * 3.2));
            vu += vol;
        }
    }

    // DRUM MACHINE — kicks/snare/hats run kitLag ms behind the bass (loose!)
    if (drums_on(L)) {
        int lag = dly + kitLag + rnd(4);
        if (step == 0 || step == 8 || (step == 10 && chance(25)))
            { schedule_hit(lag, 40, I_KICK, 5, 110); vu += 2; }
        if (step == 4 || step == 12)
            { schedule_hit(lag, 60, I_SNARE, 4, 60); vu += 2; }
        int hatEvery = (intensity >= 3) ? 2 : 4;        // 16ths when it's cooking
        if (step % hatEvery == 0)
            schedule_hit(lag, 88, I_HAT, (step % 8 == 4) ? 3 : 2, 28);
    }
}

// ── the lead — legato whistle, driven per-frame (no attack to expose jitter)
static void lead_tick(double pos) {
    long s = (long)pos - songBase;                      // current 16th in song
    if (s < 0) { return; }
    long bar = s / 16;
    long L   = bar / vamp_bars();
    bool want = radioOn && solo_on(L);

    if (!want) {
        if (leadH >= 0 && chance(8)) { note_off(leadH); leadH = -1; }   // trail off
        return;
    }
    long e = s / 2;                                     // current 8th
    if (e == leadStep) return;
    leadStep = e;
    if (e % 2 == 0 ? chance(70) : chance(35)) {         // lazy phrasing, more on beats
        int ci = chord_of_bar(bar);
        int pcs[3]; chord_pcs(ci, pcs);
        // wander the mixolydian scale near the walker; land on chord tones on the beat
        int bestM = leadPitch, bestScore = -999;
        for (int d = 0; d < 7; d++) {
            int pc = (sng.keyPc + MIXO[d]) % 12;
            for (int oct = 6; oct <= 7; oct++) {
                int m = oct * 12 + pc;
                if (m < 74 || m > 93) continue;
                int score = 10 - iabs(m - leadPitch) + rnd(4);
                if ((e % 4 == 0) && (pc == pcs[0] || pc == pcs[1])) score += 4;
                if (m == leadPitch) score -= 3;
                if (score > bestScore) { bestScore = score; bestM = m; }
            }
        }
        leadPitch = bestM;
        if (leadH < 0) {                                // new phrase: fresh attack
            leadH = note_on(leadPitch, I_LEAD, 3);
            note_glide(leadH, rnd_between(50, 130));    // THE slide
            note_lfo(leadH, 0, LFO_PITCH, 5.8f, 0.16f); // warbly vibrato
        } else {
            note_pitch(leadH, leadPitch);               // legato slide
            if (chance(12)) { note_off(leadH); leadH = -1; }   // take a breath
        }
        vu += 1.5f;
    }
}

// ── setup ─────────────────────────────────────────────────────────────────

// the guitar chair has two candidates — A/B them live with G. This is the evidence-
// gathering step for the per-song timbre roll (game-music.md "the same band every
// night"): once both sound right, the song seed rolls WHICH guitar shows up.
static int gtrPluck = 0;   // 0 = chorus TRI (shipped sound), 1 = INSTR_PLUCK real string

static void setup_gtr(void) {
    if (!gtrPluck) {
        instrument(I_GTR, INSTR_TRI, 1, 350, 2, 180);        // jangle pluck (the TRI fake)
        instrument_filter(I_GTR, FILTER_LOW, 2400, 2);
        instrument_lfo(I_GTR, 0, LFO_PITCH, 5.5f, 0.12f);    // chorus warble — THE sound
        instrument_lfo(I_GTR, 1, LFO_VOLUME, 4.7f, 0.08f);   // soft tremolo under it
    } else {
        instrument(I_GTR, INSTR_PLUCK, 1, 0, 7, 180);        // a real string (KS engine)
        instrument_filter(I_GTR, FILTER_LOW, 2600, 2);       // keep the jangle top-end cap
        instrument_harmonics(I_GTR, 0.5f);                   // ~1.2s ring — strummy, not washy
        instrument_timbre(I_GTR, 0.75f);                     // bright pick — it IS jangle
        instrument_morph(I_GTR, 0.2f);                       // near the bridge, full harmonics
        instrument_lfo(I_GTR, 0, LFO_PITCH, 5.5f, 0.12f);    // chorus warble — the KS read tap is
                                                             //   fractional, so the SAME recipe
                                                             //   bends the real string too
        instrument_lfo(I_GTR, 1, LFO_VOLUME, 4.7f, 0.08f);   // soft tremolo under it
    }
}

static void setup_instruments(void) {
    setup_gtr();

    instrument(I_BASS, INSTR_SINE, 2, 250, 4, 90);
    instrument_filter(I_BASS, FILTER_LOW, 600, 1);

    instrument(I_LEAD, INSTR_SQUARE, 18, 150, 5, 160);       // thin whistle
    instrument_duty(I_LEAD, 0.22f);
    instrument_filter(I_LEAD, FILTER_LOW, 1900, 2);

    instrument(I_KICK, INSTR_SINE, 1, 90, 0, 40);            // CR-78-ish thud
    instrument_env(I_KICK, 0, ENV_PITCH, 0, 45, 14);

    instrument(I_SNARE, INSTR_NOISE, 0, 70, 0, 30);          // boxy little snare
    instrument_filter(I_SNARE, FILTER_BAND, 1100, 7);

    instrument(I_HAT, INSTR_NOISE, 0, 24, 0, 16);
    instrument_filter(I_HAT, FILTER_HIGH, 6500, 3);
}

// ── update ────────────────────────────────────────────────────────────────
void update(void) {
    static bool booted = false;
    double pos = (double)beat() * 4.0 + beat_pos() * 4.0;
    stepMs = 60000.0 / (tempo * 4);

    if (!booted) {
        setup_instruments();
        if (JANGLE_SEED) { new_song(pos, JANGLE_SEED); hist[histN++] = sng.seed; histPos = 0; }
        else fresh_song(pos);
        scheduled = (long)pos;
        booted = true;
    }

    if (keyp(KEY_SPACE)) fresh_song(pos);
    if (keyp('R')) new_song(pos, sng.seed);
    if (keyp('[') && histPos > 0)         new_song(pos, hist[--histPos]);
    if (keyp(']') && histPos < histN - 1) new_song(pos, hist[++histPos]);
    if (keyp(KEY_RIGHT) && intensity < 3) intensity++;
    if (keyp(KEY_LEFT)  && intensity > 0) intensity--;
    if (keyp(KEY_UP)   && tempo < 116) { tempo += 2; bpm(tempo); }
    if (keyp(KEY_DOWN) && tempo > 78)  { tempo -= 2; bpm(tempo); }
    if (keyp('M')) {
        radioOn = !radioOn;
        if (!radioOn) { note_off_all(); leadH = -1; }
        else scheduled = (long)pos;
    }
    if (keyp('G')) { gtrPluck = !gtrPluck; setup_gtr(); }   // A/B the guitar chair, mid-song
    if (keyp('H')) showHelp = !showHelp;
    if (mouse_pressed(MOUSE_LEFT)) {
        int hx = mouse_x() - 288, hy = mouse_y() - 172;
        if (hx * hx + hy * hy < 81) showHelp = !showHelp;
    }

    if (radioOn) {
        long target = (long)pos + 1;
        while (scheduled < target) { scheduled++; play_step(scheduled, pos); }

        long songStep = scheduled - songBase;
        if (songStep >= (long)total_loops() * vamp_bars() * 16) fresh_song(pos);

        lead_tick(pos);                                 // legato solo voice

        for (int i = 0; i < sng.vampN; i++) chord_label(nowChord[i], 8, i);
    }

    vu *= 0.86f;
    if (vu > 12) vu = 12;

#ifdef DE_TRACE
    long ss = scheduled - songBase;
    long bar = ss >= 0 ? ss / 16 : 0;
    watch("song", "%d", songCount);
    watch("loop", "%ld", bar / vamp_bars());
    watch("chord", "%s", nowChord[chord_of_bar(bar)]);
    watch("vampN", "%d", sng.vampN);
#endif
}

// ── draw — the radio on the porch ─────────────────────────────────────────
static void knob(int x, int y, int r, float t, const char *label, int col) {
    circfill(x, y, r, CLR_DARK_BROWN);
    circ(x, y, r, CLR_BROWNISH_BLACK);
    float a = (-0.75f + t * 1.5f) * 3.14159f;
    line(x, y, x + (int)(sinf(a) * (r - 2)), y - (int)(cosf(a) * (r - 2)), col);
    print(label, x - text_width(label) / 2, y + r + 3, CLR_DARK_BROWN);
}

void draw(void) {
    cls(CLR_PEACH);                                     // late-afternoon air
    float t = timer();
    long songStep = scheduled - songBase;
    long bar = songStep >= 0 ? songStep / 16 : 0;
    long L = bar / vamp_bars();

    // body — sun-faded cream plastic
    rectfill(20, 16, 280, 168, CLR_DARK_ORANGE);
    rectfill(24, 20, 272, 160, CLR_LIGHT_PEACH);

    // FM dial strip
    rectfill(32, 26, 256, 18, CLR_WHITE);
    rect(32, 26, 256, 18, CLR_DARK_ORANGE);
    for (int fq = 88; fq <= 107; fq++) {
        int x = 36 + (fq - 88) * 13;
        line(x, 38, x, 42, CLR_BROWN);
        if (fq % 4 == 0) {
            char tx[8]; snprintf(tx, 8, "%d", fq);
            print(tx, x - 6, 29, CLR_BROWN);
        }
    }
    int nx = 36 + (int)((sng.freq - 88.0f) * 13.0f);
    line(nx, 27, nx, 43, CLR_RED);

    // the window — sun, clouds, a wire with little birds
    rectfill(34, 52, 102, 116, CLR_BLUE);
    rect(34, 52, 102, 116, CLR_DARK_ORANGE);
    circfill(112, 70, 10, CLR_YELLOW);                  // afternoon sun
    circfill(112, 70, 7, CLR_LIGHT_YELLOW);
    for (int c = 0; c < 3; c++) {                       // slow clouds
        int cx = 36 + (int)fmodf(t * (3.0f + c * 1.7f) + c * 37, 110.0f);
        int cy = 62 + c * 13;
        if (cx > 30 && cx < 132) {
            ovalfill(cx, cy, 9, 3, CLR_WHITE);
            ovalfill(cx + 6, cy + 2, 6, 2, CLR_WHITE);
        }
    }
    line(34, 130, 135, 122, CLR_DARK_BROWN);            // telephone wire
    for (int b = 0; b < 3; b++) {                       // birds; one hops per chord
        int bx = 52 + b * 26;
        int by = 130 - (b * 26 + 18) * 8 / 101 - 2;
        int hop = (chord_of_bar(bar) % 3 == b && (songStep % 16) < 3) ? -2 : 0;
        circfill(bx, by + hop - 2, 2, CLR_DARK_BROWN);
        pset(bx + 2, by + hop - 3, CLR_DARK_ORANGE);    // beak
    }
    rectfill(34, 152, 102, 16, CLR_MEDIUM_GREEN);       // the lawn

    // display
    rectfill(148, 52, 142, 44, CLR_BROWNISH_BLACK);
    rect(148, 52, 142, 44, CLR_BROWN);
    if (radioOn) {
        print(sng.title, 154, 58, CLR_ORANGE);
        char l2[32];
        snprintf(l2, 32, "%.1f FM  %s mixo", sng.freq, PCNAME[sng.keyPc]);
        print(l2, 154, 70, CLR_DARK_ORANGE);
        snprintf(l2, 32, "%d bpm #%08X", tempo, sng.seed);
        print(l2, 154, 82, CLR_DARK_ORANGE);
    } else
        print("- radio off -", 170, 70, CLR_DARK_GREY);

    // the vamp, all of it — current chord boxed (there is no bridge to show)
    if (radioOn) {
        int ci = chord_of_bar(bar);
        int x = 152;
        for (int i = 0; i < sng.vampN; i++) {
            int cw = text_width(nowChord[i]);
            if (i == ci) {
                rectfill(x - 2, 102, cw + 4, 12, CLR_ORANGE);
                print(nowChord[i], x, 104, CLR_BROWNISH_BLACK);
            } else
                print(nowChord[i], x, 104, CLR_BROWN);
            x += cw + 14;
        }
        // section + loop dots
        const char *sect = L < sng.introL ? "intro"
                         : solo_on(L) ? "solo!"
                         : L >= total_loops() - sng.outroL ? "outro" : "groove";
        print(sect, 152, 120, CLR_DARK_ORANGE);
        int T = total_loops();
        for (int i = 0; i < T && i < 16; i++)
            circfill(200 + i * 6, 124, 1, i <= L ? CLR_ORANGE : CLR_MEDIUM_GREY);
    }

    // knobs + power LED
    static const char *FEEL[4] = { "naptime", "porch", "cruisin", "heatwave" };
    knob(168, 148, 9, intensity / 3.0f, FEEL[intensity], CLR_ORANGE);
    knob(218, 148, 9, (tempo - 78) / 38.0f, "tempo", CLR_ORANGE);
    float vt = vu / 12.0f;
    knob(262, 148, 11, vt > 1 ? 1 : vt, "wow", CLR_RED);
    circfill(282, 28, 2, radioOn && beat_pos() < 0.25f ? CLR_RED : CLR_DARK_RED);

    // help button + hint
    circfill(288, 172, 6, CLR_DARK_ORANGE);
    circ(288, 172, 6, CLR_BROWNISH_BLACK);
    print("?", 285, 169, CLR_LIGHT_PEACH);
    print(str("SPACE next song   G gtr:%s   H help", gtrPluck ? "PLUCK" : "TRI"), 8, 190, CLR_BROWN);

    if (showHelp) {
        rectfill(44, 40, 232, 122, CLR_BROWNISH_BLACK);
        rect(44, 40, 232, 122, CLR_ORANGE);
        print("JANGLE RADIO", 52, 46, CLR_ORANGE);
        font(FONT_SMALL);
        static const char *HELP[8][2] = {
            { "SPACE",      "next song (rolls a new seed)" },
            { "R",          "same song again - a fresh take" },
            { "[ / ]",      "back / forward through history" },
            { "LEFT/RIGHT", "feel - how many layers play" },
            { "UP/DOWN",    "tempo of this tune" },
            { "M",          "radio power on / off" },
            { "G",          "guitar: chorus tri / real string" },
            { "H or ?",     "show / hide this help" },
        };
        for (int i = 0; i < 8; i++) {
            print(HELP[i][0], 52, 60 + i * 9, CLR_YELLOW);
            print(HELP[i][1], 106, 60 + i * 9, CLR_WHITE);
        }
        print("the #number on the display IS the song.", 52, 137, CLR_PEACH);
        print("pin it for good: #define JANGLE_SEED 0x...", 52, 146, CLR_PEACH);
        print("seeded composition, played fresh every time", 52, 155, CLR_PEACH);
        font(FONT_NORMAL);
    }
}
