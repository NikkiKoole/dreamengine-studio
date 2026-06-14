#include "studio.h"
#include "ui.h"
#include <math.h>

// SAY — a prosody probe for the formant voice (INSTR_VOICE). The vox/voxlab carts settled
// the TIMBRE axes (vowel/size/effort). This one probes the FOURTH kind of control the voice
// needs to SPEAK rather than drone: PROSODY — pitch + timing + stress shaped over a whole
// phrase. It uses ONLY today's primitives (no new engine API) so it tells us BY EAR where
// the engine falls short before we add anything (the "evidence before structure" path in
// docs/design/voice-engine.md).
//
// What it exercises with existing calls:
//   • CONNECTED SPEECH on ONE held note — re-firing voice_consonant() on an already-held
//     note resets vox_cons_t, so it re-articulates a consonant MID-NOTE. That turns the
//     "one syllable per note" voice into connected chains without retriggering.
//   • VARIED SYLLABLE SHAPES — RANDOM builds language-like simlish from a mix of V / CV / VC /
//     CVC (codas wired through the existing voice_coda() near each syllable's end) plus the odd
//     diphthong glide (vowel→vowel within a syllable) — not a monotonous da-da-da CV string.
//   • PITCH CONTOUR — note_pitch() driven every frame along a per-syllable shape (flat / rise
//     / fall / peak), plus phrase-level declination (pitch drifts down across a statement).
//   • INTONATION by mode — statement = final fall · question = final rise · exclaim = wider + hard fall.
//   • EMPHASIS — click a syllable: it gets a pitch PEAK (a pitch accent) + longer + a touch louder.
//   • CHARACTER presets (the old terminal say() "vary character" lever) — bundles of
//     size/breath/open-q/tilt + pitch offset + pitch-RANGE + rate: normal/robot/kid/giant/whisper.
//     (robot = zero pitch-range, no glide → monotone. The missing creak/jitter is the gap this exposes.)
//
// CONTROLS: SAY plays the phrase · RANDOM new simlish · click the contour panel to set/clear
// emphasis · STATE./QUEST?/EXCLAIM! pick intonation · character row picks the voice ·
// RANGE/RATE sliders · -/+ root pitch · LOOP auto-repeats.
//
// KNOWN gaps this probe is meant to surface (candidates for real engine API):
//   1. no CREAK / JITTER in the glottal source → robot can't sound truly sterile, no old-man rasp.
//   2. only b d g m n l s sh → no h w r f v / unvoiced stops p t k → limited intelligibility.
//   3. no SCHWA (reduced vowel) → unstressed syllables over-enunciate.
//   4. no DIPHTHONG (vowel-glide within a note) → English vowels sound clipped.
//   5. re-fired voice_consonant works but is an onset morph, not a true inter-vocalic stop —
//      judge by ear whether a dedicated mid-note consonant is worth it.

#define SLOT 5

// note_aux indices (mirror vox.c / sound.h param order)
enum { VP_VOWEL, VP_SIZE, VP_BREATH, VP_OPENQ, VP_TILT, VP_VIBD, VP_VIBR };

// per-syllable pitch shapes
enum { SH_FLAT, SH_RISE, SH_FALL, SH_PEAK };

// consonant ids match sound.h VC_*: b d g m n l s sh. -1 = clean vowel onset.
static const char *CONS_TXT[9] = { "", "b", "d", "g", "m", "n", "l", "s", "sh" }; // index = id+1
static const char *VOW_TXT[5]  = { "oo", "oh", "ah", "eh", "ee" };
// mellow voiced pools for simlish (no hissy s/sh).
static const int ONSET_POOL[] = { 0, 1, 2, 3, 4, 5 };               // b d g m n l
#define NONSET ((int)(sizeof(ONSET_POOL) / sizeof(ONSET_POOL[0])))
static const int CODA_POOL[]  = { 3, 4, 5, 3, 4, 5, 0, 1, 2 };      // m n l (weighted) + b d g
#define NCODA  ((int)(sizeof(CODA_POOL) / sizeof(CODA_POOL[0])))

// a syllable is one of V / CV / VC / CVC, optionally with a diphthong glide:
//   cons  = onset consonant id, -1 = none (bare-vowel onset)
//   vowel = the (first) vowel 0..4
//   vow2  = diphthong target vowel 0..4, -1 = no glide (pure vowel)
//   coda  = closing consonant id, -1 = none (open syllable)
typedef struct { int cons; int vowel; int vow2; int coda; } Syl;
#define MAXSYL 8
static Syl phrase[MAXSYL];
static int nsyl = 4;

// character preset = a bundle of timbre + prosody scaling (the "vary character" lever)
typedef struct {
    const char *name;
    float size, breath, openq, tilt;   // INSTR_VOICE timbre
    int   vol;                         // 0..7
    int   glide;                       // portamento ms (0 = robotic snap)
    float pitchOff;                    // semitone offset added to root
    float rangeMul;                    // multiplies the prosody contour (0 = monotone)
    float rateMul;                     // multiplies syllable durations
} Character;

static const Character CHARS[] = {
    //  name        size  breath openq  tilt  vol glide pOff range rate
    { "NORMAL",    0.33f, 0.12f, 0.55f, 0.35f, 6,  35,    0,  1.0f, 1.0f  },
    { "ROBOT",     0.40f, 0.00f, 0.12f, 0.06f, 6,   0,    0,  0.0f, 0.95f },  // monotone, pressed, no glide
    { "KID",       0.12f, 0.18f, 0.65f, 0.20f, 6,  28,   12,  1.3f, 1.15f },  // small tract, high, animated
    { "GIANT",     0.85f, 0.10f, 0.72f, 0.62f, 7,  55,  -12,  0.8f, 0.82f },  // long tract, low, slow
    { "WHISPER",   0.36f, 0.92f, 0.85f, 0.30f, 4,  40,    0,  0.9f, 1.0f  },  // breath maxed, glottis nearly off
};
#define NCHAR ((int)(sizeof(CHARS) / sizeof(CHARS[0])))
static const Character *ch = &CHARS[0];
static int preset = 0;

static int   mode   = 0;       // 0 statement · 1 question · 2 exclaim
static int   emph   = -1;      // emphasized syllable index (-1 = none)
static float root   = 50;      // base midi pitch
static float rangeS = 0.5f;    // RANGE slider 0..1 → prange 0..2
static float rateS  = 0.34f;   // RATE slider 0..1 → rate 0.5..2.0
static float prange = 1.0f;
static float rate   = 1.0f;
static int   loop   = 0;
static int   gaptimer = 0;

// playback state machine
static int   playing = 0;
static int   voice   = -1;
static int   si      = 0;      // current syllable
static int   stimer  = 0;      // frames into current syllable
static int   sdur    = 1;      // total frames for current syllable
static float ca, cb, cpk;      // contour relative-semitone endpoints + peak for current syllable
static int   csh = SH_FLAT;
static int   coda_fired = 0;   // has this syllable's coda been triggered yet?
static int   coda_at    = 0;   // frame within the syllable at which to fire the coda

#define PANX 8
#define PANY 16
#define PANW 304
#define PANH 64

// compute the contour plan for syllable i (relative semitones, before root) + its frame duration
static void plan(int i, float *pa, float *pb, float *ppk, int *psh, int *pdur) {
    float N = (float)nsyl;
    float decl = (nsyl > 1) ? lerp(1.5f, -2.0f, (float)i / (N - 1.0f)) : 0.0f;  // gentle declination
    float a = decl, b = decl, pk = 0.0f; int sh = SH_FLAT; float durMul = 1.0f;

    if (i == emph) { pk = 3.0f; sh = SH_PEAK; durMul = 1.6f; }   // pitch accent + lengthen

    if (i == nsyl - 1) {                                          // phrase-final intonation by mode
        if (mode == 0)      { sh = SH_FALL; a = decl + 1.0f; b = decl - 3.0f; }            // statement
        else if (mode == 1) { sh = SH_RISE; a = decl - 1.0f; b = decl + 6.0f; }            // question
        else                { sh = SH_FALL; a = decl + 3.0f; b = decl - 4.0f; durMul *= 1.3f; } // exclaim
        pk = 0.0f;                                                // mode shape owns the final, not the accent
    }

    *pa = a; *pb = b; *ppk = pk; *psh = sh;
    int d = (int)(15.0f * durMul / (rate * ch->rateMul));
    *pdur = d < 5 ? 5 : d;
}

// relative semitone (incl. character pitch offset + range scaling) at progress p (0..1)
static float contour_rel(float a, float b, float pk, int sh, float p) {
    float s = (sh == SH_PEAK) ? lerp(a, b, p) + pk * sinf(p * 3.14159265f)
                              : lerp(a, b, p);
    return ch->pitchOff + s * prange * ch->rangeMul;
}
static float contour_pitch(float a, float b, float pk, int sh, float p) {
    return root + contour_rel(a, b, pk, sh, p);
}

static void trigger_syllable(int i) {              // fire the i-th syllable on the HELD note
    plan(i, &ca, &cb, &cpk, &csh, &sdur);
    voice_coda(voice, -1);                         // clear the previous syllable's coda, if any
    voice_consonant(voice, phrase[i].cons);        // re-articulate mid-note (resets vox_cons_t)
    note_aux(voice, VP_VOWEL, phrase[i].vowel * 0.25f);
    coda_fired = 0;
    coda_at = (int)(sdur * 0.62f);                 // close on the coda in the syllable's back third
    stimer = 0;
}

static void start_say(void) {
    si = 0;
    plan(0, &ca, &cb, &cpk, &csh, &sdur);
    float p0 = contour_pitch(ca, cb, cpk, csh, 0.0f);
    if (voice >= 0) note_off(voice);
    voice = note_on((int)(p0 + 0.5f), SLOT, ch->vol);
    note_glide(voice, ch->glide);
    note_aux(voice, VP_SIZE,   ch->size);
    note_aux(voice, VP_BREATH, ch->breath);
    note_aux(voice, VP_OPENQ,  ch->openq);
    note_aux(voice, VP_TILT,   ch->tilt);
    note_aux(voice, VP_VIBD,   0.0f);
    note_aux(voice, VP_VOWEL,  phrase[0].vowel * 0.25f);
    voice_coda(voice, -1);
    voice_consonant(voice, phrase[0].cons);
    note_pitch(voice, p0);
    coda_fired = 0;
    coda_at = (int)(sdur * 0.62f);
    stimer = 0;
    playing = 1;
}

static void gen_phrase(void) {
    nsyl = 3 + rnd(4);                                  // 3..6 syllables
    for (int i = 0; i < nsyl; i++) {
        // ONSET: first syllable always opens on a consonant; others 75% CV, 25% bare vowel
        int hasOnset   = (i == 0) || (rnd(100) < 75);
        phrase[i].cons = hasOnset ? ONSET_POOL[rnd(NONSET)] : -1;
        phrase[i].vowel = rnd(5);
        // DIPHTHONG: 25% glide to a different vowel within the syllable ("ai", "ou")
        phrase[i].vow2 = (rnd(100) < 25) ? rnd(5) : -1;
        if (phrase[i].vow2 == phrase[i].vowel) phrase[i].vow2 = -1;
        // CODA: 32% close on a consonant → VC / CVC (the rest stay open, V / CV)
        phrase[i].coda = (rnd(100) < 32) ? CODA_POOL[rnd(NCODA)] : -1;
    }
    if (emph >= nsyl) emph = -1;
}

void init(void) {
    instrument(SLOT, INSTR_VOICE, 35, 60, 7, 160);
    ch = &CHARS[0];
    // a pleasant default phrase showing the shapes: "bam loo-i nee dol"
    phrase[0] = (Syl){ 0, 2, -1,  3 };   // b + ah + m   (CVC)
    phrase[1] = (Syl){ 5, 0,  4, -1 };   // l + oo→ee    (CV diphthong)
    phrase[2] = (Syl){ 4, 4, -1, -1 };   // n + ee       (CV)
    phrase[3] = (Syl){ 1, 1, -1,  5 };   // d + oh + l   (CVC)
    nsyl = 4;
}

void update(void) {
    prange = rangeS * 2.0f;
    rate   = 0.5f + rateS * 1.5f;

    // keyboard shortcuts (also make the cart scriptable for the debug/WAV harness)
    if (keyp(KEY_SPACE)) start_say();
    if (keyp('R')) gen_phrase();
    if (keyp('1')) mode = 0;
    if (keyp('2')) mode = 1;
    if (keyp('3')) mode = 2;
    if (keyp('L')) loop = !loop;
    if (keyp('Z')) { preset = (preset + NCHAR - 1) % NCHAR; ch = &CHARS[preset]; }
    if (keyp('X')) { preset = (preset + 1) % NCHAR; ch = &CHARS[preset]; }

    // click the contour panel to set / clear the emphasized syllable
    int mx = mouse_x(), my = mouse_y();
    if (mouse_pressed(MOUSE_LEFT) && mx >= PANX && mx < PANX + PANW && my >= PANY && my < PANY + PANH) {
        int slot = (mx - PANX) * nsyl / PANW;
        if (slot < 0) slot = 0; if (slot >= nsyl) slot = nsyl - 1;
        emph = (emph == slot) ? -1 : slot;
    }

    // drive the held note's pitch along the current syllable's contour, advance the machine
    if (playing && voice >= 0) {
        stimer++;
        float p = (sdur > 0) ? (float)stimer / (float)sdur : 1.0f;
        if (p > 1.0f) p = 1.0f;
        note_pitch(voice, contour_pitch(ca, cb, cpk, csh, p));
        // diphthong: glide the vowel across the syllable toward vow2
        float vt = (phrase[si].vow2 >= 0)
                 ? lerp((float)phrase[si].vowel, (float)phrase[si].vow2, p)
                 : (float)phrase[si].vowel;
        note_aux(voice, VP_VOWEL, vt * 0.25f);
        // coda: close the syllable on a consonant in its back third
        if (phrase[si].coda >= 0 && !coda_fired && stimer >= coda_at) {
            voice_coda(voice, phrase[si].coda);
            coda_fired = 1;
        }
        if (stimer >= sdur) {
            si++;
            if (si >= nsyl) {
                note_off(voice); voice = -1; playing = 0;
                if (loop) gaptimer = 30;
            } else {
                trigger_syllable(si);
            }
        }
    } else if (loop) {                       // auto-repeat after a short gap
        if (gaptimer > 0) gaptimer--;
        if (gaptimer <= 0) start_say();
    }
}

static int semi_y(float semi) {
    float top = PANY + 2, bot = PANY + PANH - 6, span = bot - top;
    float v = (semi + 10.0f) / 24.0f;        // map -10..+14 semitones into the panel
    if (v < 0) v = 0; if (v > 1) v = 1;
    return (int)(bot - v * span);
}

void draw(void) {
    cls(CLR_DARKER_BLUE);
    print("SAY", 8, 4, CLR_PEACH);
    print("prosody probe - INSTR_VOICE", 40, 4, CLR_MEDIUM_GREY);
    // status line (active mode + character, since ui_button has no toggle state)
    const char *modes[3] = { "statement.", "question?", "exclaim!" };
    char st[40]; int n = 0;
    const char *mm = modes[mode]; while (*mm) st[n++] = *mm++;
    st[n++] = ' ';
    const char *cc = ch->name;  while (*cc) st[n++] = *cc++;
    st[n] = 0;
    print(st, 196, 4, CLR_DARK_GREY);

    // ---- contour panel: the prosody melody drawn across the phrase ----
    rectfill(PANX, PANY, PANW, PANH, CLR_DARKER_GREY);
    int ybase = semi_y(0);
    line(PANX, ybase, PANX + PANW, ybase, CLR_DARK_GREY);     // root reference
    for (int i = 0; i < nsyl; i++) {
        int sx = PANX + i * PANW / nsyl;
        int sw = PANW / nsyl;
        if (i == emph) rectfill(sx + 1, PANY + 1, sw - 1, PANH - 2, CLR_DARKER_PURPLE);
        if (i) line(sx, PANY, sx, PANY + PANH, CLR_DARKER_BLUE);

        float a, b, pk; int sh, dur; plan(i, &a, &b, &pk, &sh, &dur);
        int cur = (playing && i == si);
        int col = cur ? CLR_YELLOW : CLR_TRUE_BLUE;
        int prevx = 0, prevy = 0;
        for (int k = 0; k <= 8; k++) {
            float p = k / 8.0f;
            int x = sx + (int)(p * sw);
            int y = semi_y(contour_rel(a, b, pk, sh, p));
            if (k) line(prevx, prevy, x, y, col);
            prevx = x; prevy = y;
        }
        // syllable label: onset + vowel (+ diphthong target) + coda
        char lbl[8]; int ln = 0;
        const char *c = CONS_TXT[phrase[i].cons + 1];
        if (c[0]) { lbl[ln++] = c[0]; if (c[1]) lbl[ln++] = c[1]; }
        const char *vw = VOW_TXT[phrase[i].vowel];
        lbl[ln++] = vw[0]; if (vw[1]) lbl[ln++] = vw[1];
        if (phrase[i].vow2 >= 0) lbl[ln++] = VOW_TXT[phrase[i].vow2][0];   // glide target
        const char *cd = CONS_TXT[phrase[i].coda + 1];
        if (cd[0]) { lbl[ln++] = cd[0]; if (cd[1]) lbl[ln++] = cd[1]; }
        lbl[ln] = 0;
        print_centered(lbl, sx + sw / 2, PANY + PANH - 9, cur ? CLR_WHITE : CLR_MEDIUM_GREY);
    }
    rect(PANX, PANY, PANW, PANH, CLR_DARK_GREY);
    print("click a syllable to emphasize it", PANX + 3, PANY + 2, CLR_DARK_GREY);

    ui_begin();
    // ---- intonation mode + transport ----
    if (ui_button(8,   86, 58, 14, "STATE."))   mode = 0;
    if (ui_button(68,  86, 58, 14, "QUEST?"))   mode = 1;
    if (ui_button(128, 86, 62, 14, "EXCLAIM!")) mode = 2;
    if (ui_button(200, 86, 50, 14, playing ? "..." : "SAY")) start_say();
    if (ui_button(254, 86, 58, 14, loop ? "LOOP on" : "loop")) loop = !loop;

    // ---- character presets (the "vary character" row) ----
    { int x = 8; for (int c = 0; c < NCHAR; c++) {
        int w = 57;
        if (ui_button(x, 104, w, 14, CHARS[c].name)) { preset = c; ch = &CHARS[c]; }
        x += w + 2;
    } }

    // ---- prosody knobs ----
    print("RANGE", 8,   128, CLR_LIGHT_PEACH); ui_slider(&rangeS, 52,  126, 86, 0);
    print("RATE",  146, 128, CLR_LIGHT_PEACH); ui_slider(&rateS,  180, 126, 86, 0);

    // ---- root pitch + new phrase ----
    if (ui_button(8,  144, 18, 14, "-")) root = clamp(root - 1, 30, 72);
    if (ui_button(50, 144, 18, 14, "+")) root = clamp(root + 1, 30, 72);
    if (ui_button(74, 144, 70, 14, "RANDOM")) gen_phrase();
    ui_end();

    char pb[6]; int m = (int)(root + 0.5f);
    pb[0] = 'm'; pb[1] = '0' + (m / 10) % 10; pb[2] = '0' + m % 10; pb[3] = 0;
    print_centered(pb, 38, 148, CLR_YELLOW);

    print("SPACE=say  R=random  1/2/3=mode  Z/X=voice  L=loop  click panel=emphasis", 8, 168, CLR_DARK_GREY);
    print("statement falls . question rises . exclaim falls hard . robot=monotone", 8, 180, CLR_DARK_GREY);
    print("one held note, consonants re-fired mid-note = connected speech", 8, 190, CLR_DARKER_GREY);
}
