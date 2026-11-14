// lyric.h — sung WORDS for INSTR_VOICE: type a lyric, every note sings the next syllable.
//
// Languages: LYR_NL (Dutch, by spelling rules), LYR_MIPOMI (the playtime Mipos' CV language), and by
// simpler rule tables LYR_JA (Japanese romaji), LYR_ES (Spanish), LYR_SW (Swahili), LYR_HAW (Hawaiian).
//
// Two halves:
//   1. READING: lyr_set(&lyr, LYR_NL, "ik hou van jou") turns text into syllables (onset consonants,
//      a vowel, coda consonants), by spelling RULES. Dutch first (its spelling is regular enough to read
//      without a dictionary); the language is a parameter so more can follow.
//   2. SINGING: a Singer plays one melody line on one INSTR_VOICE slot. singer_on(&s, midi, vol) starts
//      the next syllable: its onset consonants fire at note-on (a cluster "str" one after another), the
//      vowel holds while the key is down; singer_off(&s) sings the rest: a diphthong's second half
//      ("ij", "ui", "ou"), then the coda consonants chained ("nacht" = x then t), and only THEN releases
//      the note, so a final consonant is never faded out by the envelope. Call singer_update(&s) every
//      frame (it sequences the consonants in time).
//
// Idea credit: JIANT FM's VOICE/WORDS (M-VAVE FM-1 firmware, GPL-3.0) showed the shape: per-language
// letter rules, syllables advanced by the key gate, codas sung at release. Ideas only, no code; Dutch
// is ours. Design + the Dutch rules: docs/design/sung-lyrics.md.
//
// USAGE
//   #include "lyric.h"
//   static Lyric lyr; static Singer sing;
//   void init(void) {
//       instrument(5, INSTR_VOICE, 5, 60, 7, 190);
//       lyr_set(&lyr, LYR_NL, "ik hou van jou");
//       singer_init(&sing, &lyr, 5);              // (to switch lyrics later: singer_stop first, then init)
//       singer_buzz(&sing, 0.5f);        // optional: 0 relaxed, round .. 1 pressed, bright
//   }
//   void update(void) {
//       if (keyp('Z')) singer_on(&sing, 60, 6);
//       if (keyr('Z')) singer_off(&sing);
//       singer_update(&sing);
//   }
//
// TEXT: words are letters (case ignored; é è ê ë ï ö ü understood, ë/ï split a vowel pair: "geëerd").
// A lone "-" (or a word "-") holds the previous vowel over one more note (a melisma); the coda waits
// for the last of them. The lyric loops.
//
// DUTCH, what the rules do (lyr__nl_*): digraphs (aa ee oo uu ie oe eu ui ij/ei au/ou, ch ng sch nk),
// long vs short from OPEN vs CLOSED syllables (ma-ken / man-nen), schwa for an unstressed single e
// (de, ge-luk, be-ter's -ter, -en), -ig and -lijk reduced, final devoicing (hond → t), sch → s+x
// (school) but -isch → s, a dropped final -n after schwa (lopen → lope, as sung; lyr.keep_en = 1 keeps it).
// Stress = the first syllable after an unstressed prefix (be- ge- ver- ont- her- te-), guessed without
// a dictionary, so a few words come out wrong (gebed). The g/ch is the engine's x consonant; Dutch w
// uses the w glide, r the approximant.
//
// The vowels are formant TARGETS (F1/F2, Dutch adult male, in the range of Pols, Tromp & Plomp 1973
// and Adank et al. 2004), placed with INSTR_VOICE's openness/frontness levers on the nearest base
// vowel, so uu/eu/ui (front rounded) exist even though the engine's own table has no such row.
//
// SELFCHECK: lyr_selfcheck() (under DE_SPEC) reads known words and compares the phonemes; the zingen
// cart's spec() runs it (node tools/spec.js zingen).
#ifndef LYRIC_H
#define LYRIC_H

#include "studio.h"
#include <math.h>
#include <string.h>

enum { LYR_NL = 0, LYR_MIPOMI = 1, LYR_JA = 2, LYR_ES = 3, LYR_SW = 4, LYR_HAW = 5, LYR_NLANG };   // Dutch · Mipomi · Japanese · Spanish · Swahili · Hawaiian
static const char *const lyr_lang_name[LYR_NLANG] = { "Nederlands", "Mipomi", "Nihongo", "Espanol", "Kiswahili", "Hawaii" };

// consonant ids: mirror INSTR_VOICE's voice_consonant()/voice_coda() ids (sound.h VC_*, append-only)
enum { LC_B, LC_D, LC_G, LC_M, LC_N, LC_L, LC_S, LC_SH, LC_NG, LC_R, LC_W, LC_Y, LC_DH, LC_F, LC_V,
       LC_Z, LC_ZH, LC_TH, LC_P, LC_T, LC_K, LC_CH, LC_H, LC_X, LC_COUNT };
static const char *const lyr__cname[LC_COUNT] = { "b", "d", "g", "m", "n", "l", "s", "sh", "ng", "r",
    "w", "j", "dh", "f", "v", "z", "zh", "th", "p", "t", "k", "ch", "h", "x" };
// the engine's consonant durations (s), mirrored for timing a cluster (sound.h vox_cons_dur)
static const float lyr__cdur[LC_COUNT] = { 0.055f, 0.055f, 0.055f, 0.090f, 0.090f, 0.070f, 0.110f, 0.110f,
    0.090f, 0.070f, 0.070f, 0.070f, 0.090f, 0.110f, 0.100f, 0.110f, 0.110f, 0.110f, 0.055f, 0.055f, 0.055f,
    0.090f, 0.080f, 0.110f };
static const unsigned char lyr__cplos[LC_COUNT] = { 1,1,1,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0, 1,1,1,1, 0,0 };

// vowels
enum { LV_A, LV_AA, LV_E, LV_EE, LV_I, LV_IE, LV_O, LV_OO, LV_U, LV_UU, LV_OE, LV_EU, LV_SCHWA,
       LV_EI, LV_UI, LV_AU, LV_OP, LV_EP, LV_UW, LV_AI, LV_OI, LV_COUNT };
static const char *const lyr__vname[LV_COUNT] = { "A", "aa", "E", "ee", "I", "ie", "O", "oo", "U", "uu",
    "oe", "eu", "@", "ei", "ui", "au", "o", "e", "u'", "ai", "oi" };
// F1, F2 (Hz) of each vowel, then where it GLIDES on release (0 = a pure vowel). ee/oo glide a little,
// as Dutch long e/o do; ei/ui/au are the real diphthongs.
static const float lyr__vf[LV_COUNT][4] = {
    { 680, 1080,   0,    0 },   // A   ɑ   (man)
    { 800, 1300,   0,    0 },   // aa  aː  (maan)
    { 590, 1700,   0,    0 },   // E   ɛ   (bed)
    { 420, 2100, 340, 2250 },   // ee  eː  (been)  → toward i
    { 400, 1950,   0,    0 },   // I   ɪ   (pit)
    { 290, 2200,   0,    0 },   // ie  i   (piet)
    { 520,  870,   0,    0 },   // O   ɔ   (pot)
    { 470,  900, 370,  780 },   // oo  oː  (boot)  → toward u
    { 460, 1550,   0,    0 },   // U   ʏ   (put)
    { 300, 1750,   0,    0 },   // uu  y   (fuut)
    { 340,  800,   0,    0 },   // oe  u   (voet)
    { 420, 1480,   0,    0 },   // eu  øː  (neus)
    { 500, 1450,   0,    0 },   // @   ə   (de)
    { 600, 1750, 350, 2150 },   // ei  ɛi  (mijn)
    { 560, 1550, 330, 1720 },   // ui  œy  (huis)
    { 680, 1100, 400,  850 },   // au  ɑu  (hou)
    { 450,  850,   0,    0 },   // o   a PURE round o, no glide (Mipomi's o)
    { 460, 1900,   0,    0 },   // e   a PURE mid e, no glide (Spanish, Swahili, Japanese e)
    { 350, 1250,   0,    0 },   // u'  the UNROUNDED Japanese u
    { 780, 1300, 350, 2150 },   // ai  (baile)
    { 470,  880, 350, 2100 },   // oi  (hoy)
};
// INSTR_VOICE's own vowel path (U O A E I = note_aux 0 at 0, .25, .5, .75, 1): F1/F2 (sound.h vox_vowel_f)
static const float lyr__base[5][2] = { {300, 870}, {570, 840}, {730, 1090}, {530, 1840}, {270, 2290} };

#define LYR_MAXSYL 160
#define LYR_MAXC   4
typedef struct {
    signed char on[LYR_MAXC], co[LYR_MAXC];   // onset / coda consonant ids (LC_*)
    unsigned char non, nco;
    unsigned char v;                          // LV_*
    unsigned char hold;                       // a melisma "-": the previous vowel again, no onset
    unsigned char word0;                      // first syllable of a word
    char txt[12];                             // its letters (for display)
} LyrSyl;
typedef struct {
    LyrSyl syl[LYR_MAXSYL];
    int n;
    int lang;
    int keep_en;                              // 1 = sing the final n of -en (default 0: "lopen" → "lope")
} Lyric;

// ───────────────────────────────── reading Dutch ─────────────────────────────────
// A word becomes TOKENS (a consonant or a vowel, each knowing its letters), then syllables.
enum { LYR__TC, LYR__TV };
typedef struct {
    unsigned char kind, id;
    unsigned char single;    // vowel written with ONE letter (a e i o u): length decided by the syllable
    unsigned char shortf;    // forced short (followed by ch / ng / sch / x, which spelling never doubles)
    unsigned char src;       // index of its first letter in the word
    unsigned char acc;       // written with an accent (é á ó ú í): the stressed syllable
} LyrTok;

static int lyr__isv(int c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }
static int lyr__pre(const char *w, int n, int i, const char *s) {   // does w[i..] start with s?
    for (int k = 0; s[k]; k++) if (i + k >= n || w[i + k] != s[k]) return 0;
    return 1;
}

// letters → tokens. '|' in w is a hiatus mark (from ë ï ö ü): it splits a vowel pair; '^' marks an
// accented (stressed) vowel (é á ó ú í).
static int lyr__nl_tok(const char *w, int n, LyrTok *t, int max) {
    int m = 0, acc = 0;
#define LYR__T(k, x, sg, at) do { if (m < max) { t[m].kind = (k); t[m].id = (x); t[m].single = (sg); \
                                  t[m].shortf = 0; t[m].src = (unsigned char)(at); \
                                  t[m].acc = (unsigned char)((k) == LYR__TV ? acc : 0); \
                                  if ((k) == LYR__TV) acc = 0; m++; } } while (0)
    for (int i = 0; i < n; ) {
        int c = w[i];
        if (c == '|') { i++; continue; }
        if (c == '^') { acc = 1; i++; continue; }
        // vowels, longest spelling first
        if (lyr__pre(w, n, i, "ieuw")) { LYR__T(LYR__TV, LV_IE, 0, i); LYR__T(LYR__TC, LC_W, 0, i + 3); i += 4; continue; }
        if (lyr__pre(w, n, i, "eeuw")) { LYR__T(LYR__TV, LV_EE, 0, i); LYR__T(LYR__TC, LC_W, 0, i + 3); i += 4; continue; }
        if (lyr__pre(w, n, i, "aai"))  { LYR__T(LYR__TV, LV_AA, 0, i); LYR__T(LYR__TC, LC_Y, 0, i + 2); i += 3; continue; }
        if (lyr__pre(w, n, i, "ooi"))  { LYR__T(LYR__TV, LV_OO, 0, i); LYR__T(LYR__TC, LC_Y, 0, i + 2); i += 3; continue; }
        if (lyr__pre(w, n, i, "oei"))  { LYR__T(LYR__TV, LV_OE, 0, i); LYR__T(LYR__TC, LC_Y, 0, i + 2); i += 3; continue; }
        {
            static const struct { const char *s; unsigned char v; } D[] = {
                {"ij", LV_EI}, {"ei", LV_EI}, {"ui", LV_UI}, {"au", LV_AU}, {"ou", LV_AU}, {"eu", LV_EU},
                {"oe", LV_OE}, {"ie", LV_IE}, {"aa", LV_AA}, {"ee", LV_EE}, {"oo", LV_OO}, {"uu", LV_UU},
            };
            int hit = 0;
            for (int k = 0; k < (int)(sizeof D / sizeof D[0]); k++)
                if (lyr__pre(w, n, i, D[k].s)) { LYR__T(LYR__TV, D[k].v, 0, i); i += 2; hit = 1; break; }
            if (hit) continue;
        }
        if (lyr__isv(c)) {
            int v = c == 'a' ? LV_A : c == 'e' ? LV_E : c == 'i' ? LV_I : c == 'o' ? LV_O : LV_U;
            LYR__T(LYR__TV, v, (unsigned char)c, i);
            if (lyr__pre(w, n, i + 1, "ch") || lyr__pre(w, n, i + 1, "ng") || lyr__pre(w, n, i + 1, "nk") ||
                (i + 1 < n && w[i + 1] == 'x'))
                t[m - 1].shortf = 1;
            i++;
            continue;
        }
        if (c == 'y') {                                   // y: j before a vowel, else ie
            if (i + 1 < n && lyr__isv(w[i + 1])) LYR__T(LYR__TC, LC_Y, 0, i);
            else LYR__T(LYR__TV, LV_IE, 0, i);
            i++;
            continue;
        }
        // consonants
        if (lyr__pre(w, n, i, "sch")) {
            if (i + 3 == n || (i + 4 == n && w[i + 3] == 'e')) {   // -isch / -ische: just s
                LYR__T(LYR__TC, LC_S, 0, i); i += 3; continue;
            }
            LYR__T(LYR__TC, LC_S, 0, i); LYR__T(LYR__TC, LC_X, 0, i + 1); i += 3; continue;
        }
        if (lyr__pre(w, n, i, "ch")) { LYR__T(LYR__TC, LC_X, 0, i); i += 2; continue; }
        if (lyr__pre(w, n, i, "ng")) { LYR__T(LYR__TC, LC_NG, 0, i); i += 2; continue; }
        if (lyr__pre(w, n, i, "nk")) { LYR__T(LYR__TC, LC_NG, 0, i); LYR__T(LYR__TC, LC_K, 0, i + 1); i += 2; continue; }
        if (lyr__pre(w, n, i, "th")) { LYR__T(LYR__TC, LC_T, 0, i); i += 2; continue; }
        if (lyr__pre(w, n, i, "ph")) { LYR__T(LYR__TC, LC_F, 0, i); i += 2; continue; }
        if (lyr__pre(w, n, i, "qu")) { LYR__T(LYR__TC, LC_K, 0, i); LYR__T(LYR__TC, LC_W, 0, i + 1); i += 2; continue; }
        if (lyr__pre(w, n, i, "sj")) { LYR__T(LYR__TC, LC_SH, 0, i); i += 2; continue; }
        if (lyr__pre(w, n, i, "tj")) { LYR__T(LYR__TC, LC_CH, 0, i); i += 2; continue; }
        {
            int id = -1;
            switch (c) {
            case 'b': id = LC_B; break;  case 'd': id = LC_D; break;  case 'f': id = LC_F; break;
            case 'g': id = LC_X; break;  case 'h': id = LC_H; break;  case 'j': id = LC_Y; break;
            case 'k': id = LC_K; break;  case 'l': id = LC_L; break;  case 'm': id = LC_M; break;
            case 'n': id = LC_N; break;  case 'p': id = LC_P; break;  case 'r': id = LC_R; break;
            case 's': id = LC_S; break;  case 't': id = LC_T; break;  case 'v': id = LC_V; break;
            case 'w': id = LC_W; break;  case 'z': id = LC_Z; break;
            case 'c': id = (i + 1 < n && (w[i + 1] == 'e' || w[i + 1] == 'i' || w[i + 1] == 'y')) ? LC_S : LC_K; break;
            case 'x': LYR__T(LYR__TC, LC_K, 0, i); id = LC_S; break;
            default: break;
            }
            if (id >= 0) LYR__T(LYR__TC, id, 0, i);
            i++;
        }
    }
#undef LYR__T
    return m;
}

// may consonants a then b open a syllable together? (obstruent + l/r: "br", "kl", "vr"; "kw" "tw" "zw" "dw")
static int lyr__nl_cluster(int a, int b) {
    int obs = a == LC_P || a == LC_B || a == LC_T || a == LC_D || a == LC_K || a == LC_X || a == LC_F || a == LC_V;
    if (obs && (b == LC_L || b == LC_R) && !((a == LC_T || a == LC_D) && b == LC_L)) return 1;
    if (b == LC_W && (a == LC_K || a == LC_T || a == LC_Z || a == LC_D)) return 1;
    return 0;
}
static int lyr__voiced_obs(int c) { return c == LC_B || c == LC_D || c == LC_V || c == LC_Z || c == LC_G || c == LC_ZH; }
static int lyr__devoice(int c) {
    return c == LC_B ? LC_P : c == LC_D ? LC_T : c == LC_V ? LC_F : c == LC_Z ? LC_S : c == LC_G ? LC_K :
           c == LC_ZH ? LC_SH : c;
}

// one Dutch word (lower case letters + '|' hiatus marks) → syllables appended to L
static void lyr__nl_word(Lyric *L, const char *w, int n) {
    LyrTok t[48];
    int m = lyr__nl_tok(w, n, t, 48);
    int nu[24], nn = 0;
    for (int i = 0; i < m && nn < 24; i++) if (t[i].kind == LYR__TV) nu[nn++] = i;
    if (!nn) return;                                      // no vowel (an initial, "'s"): skip
    // syllable boundaries: start token of each syllable
    int st[24];
    st[0] = 0;
    for (int k = 1; k < nn; k++) {
        int a = nu[k - 1] + 1, b = nu[k];                  // consonants a..b-1 between two vowels
        int c = b - a, s;
        if (c == 0) s = b;
        else if (c == 1) s = (t[a].id == LC_NG) ? b : a;  // one goes to the next (ng never opens one)
        else if (c == 2) s = lyr__nl_cluster(t[a].id, t[a + 1].id) ? a : a + 1;
        else s = lyr__nl_cluster(t[b - 2].id, t[b - 1].id) ? b - 2 : b - 1;
        st[k] = s;
    }
    // stress: the first syllable after an unstressed prefix. be-/ge-/ver-/te-/her- count as prefixes when
    // the word is long (3+ syllables) or its last vowel is not a plain e ("ge-luk" yes, "be-ter" no)
    int stress = 0;
    if (nn >= 2) {
        int lastplain_e = t[nu[nn - 1]].single == 'e';
        int pre = lyr__pre(w, n, 0, "be") || lyr__pre(w, n, 0, "ge") || lyr__pre(w, n, 0, "ver") ||
                  lyr__pre(w, n, 0, "te") || lyr__pre(w, n, 0, "her");
        if (pre && t[nu[0]].single == 'e' && (nn >= 3 || !lastplain_e)) stress = 1;
        if (lyr__pre(w, n, 0, "ont") && t[nu[0]].single == 'o') stress = 1;
    }
    for (int k = 0; k < nn; k++) if (t[nu[k]].acc) stress = k;   // an accent wins (café)
    // function words sung with a schwa
    static const char *const SCHWA_W[] = { "de", "je", "me", "we", "ze", "te", "ge", "een", 0 };
    int fw = 0;
    for (int k = 0; SCHWA_W[k]; k++) if ((int)strlen(SCHWA_W[k]) == n && !memcmp(w, SCHWA_W[k], (size_t)n)) fw = 1;

    for (int k = 0; k < nn && L->n < LYR_MAXSYL; k++) {
        int a = st[k], b = (k + 1 < nn) ? st[k + 1] : m;  // tokens a..b-1
        LyrSyl *S = &L->syl[L->n];
        memset(S, 0, sizeof *S);
        S->word0 = (k == 0);
        int vi = nu[k];
        for (int i = a; i < vi && S->non < LYR_MAXC; i++) S->on[S->non++] = (signed char)t[i].id;
        for (int i = vi + 1; i < b && S->nco < LYR_MAXC; i++) S->co[S->nco++] = (signed char)t[i].id;
        int open = (S->nco == 0);
        int v = t[vi].id, sg = t[vi].single;
        int last = (k == nn - 1);
        if (sg) {
            int closed = !open || t[vi].shortf;
            if (sg == 'e') v = (k != stress || fw) ? LV_SCHWA : closed ? LV_E : LV_EE;
            else if (sg == 'a') v = closed ? LV_A : LV_AA;
            else if (sg == 'o') v = closed ? LV_O : LV_OO;
            else if (sg == 'u') v = closed ? LV_U : LV_UU;
            else if (sg == 'i') v = closed ? LV_I : LV_IE;
            // -ig reduced: "eeuwig", "machtig"
            if (sg == 'i' && last && k > 0 && n >= 2 && w[n - 2] == 'i' && w[n - 1] == 'g') v = LV_SCHWA;
        }
        if (fw && v == LV_EE) v = LV_SCHWA;               // "een" (the article)
        if (last && k > 0 && v == LV_EI && n >= 4 && lyr__pre(w, n, n - 4, "lijk")) v = LV_SCHWA;   // -lijk
        S->v = (unsigned char)v;
        // final devoicing: an obstruent closing a syllable is voiceless ("hond", "heb-ben"…)
        for (int i = 0; i < S->nco; i++) if (lyr__voiced_obs(S->co[i])) S->co[i] = (signed char)lyr__devoice(S->co[i]);
        // the same consonant twice in a coda ("wordt" d+t → t t) once
        for (int i = 1; i < S->nco; i++)
            if (S->co[i] == S->co[i - 1]) { for (int j = i; j + 1 < S->nco; j++) S->co[j] = S->co[j + 1]; S->nco--; i--; }
        // the final n of -en after a schwa, dropped as Dutch sings it (lopen → lope)
        if (!L->keep_en && last && k > 0 && v == LV_SCHWA && S->nco == 1 && S->co[0] == LC_N) S->nco = 0;
        // its letters, for display
        {
            int c0 = t[a].src, c1 = (b < m) ? t[b].src : n, j = 0;
            for (int i = c0; i < c1 && j < (int)sizeof S->txt - 1; i++) if (w[i] != '|' && w[i] != '^') S->txt[j++] = w[i];
            S->txt[j] = 0;
        }
        L->n++;
    }
}

// ───────────────────────────────── reading Mipomi ─────────────────────────────────
// Mipomi (~/Projects/love/mipomi-lang/RULES.md): every syllable is ONE consonant + a vowel, 13
// consonants m b p f k l t j n d s h w, vowels a i o (+ u, "with"). j is the Dutch j, h a breath.
// So the reading is trivial: each vowel opens a syllable, the consonants before it are its onset.
// The aspect vowel "melts" by itself: "mi i mola" gives the syllable "i" with no onset after "mi", and a
// vowel-only syllable on the same vowel is simply that vowel held on the next note ("mii mola").
static void lyr__mp_word(Lyric *L, const char *w, int n) {
    signed char on[LYR_MAXC]; int non = 0, first = 1;
    for (int i = 0; i < n; i++) {
        int c = w[i], v = -1, id = -1;
        switch (c) {
        case 'a': v = LV_AA; break;  case 'i': v = LV_IE; break;  case 'o': v = LV_OP; break;
        case 'u': v = LV_OE; break;  case 'e': v = LV_EE; break;
        case 'm': id = LC_M; break;  case 'b': id = LC_B; break;  case 'p': id = LC_P; break;
        case 'f': id = LC_F; break;  case 'k': id = LC_K; break;  case 'l': id = LC_L; break;
        case 't': id = LC_T; break;  case 'j': id = LC_Y; break;  case 'n': id = LC_N; break;
        case 'd': id = LC_D; break;  case 's': id = LC_S; break;  case 'h': id = LC_H; break;
        case 'w': id = LC_W; break;  case 'g': id = LC_G; break;  case 'r': id = LC_R; break;
        case 'v': id = LC_V; break;  case 'z': id = LC_Z; break;  case 'y': id = LC_Y; break;
        default: break;
        }
        if (id >= 0) { if (non < LYR_MAXC) on[non++] = (signed char)id; continue; }
        if (v < 0 || L->n >= LYR_MAXSYL) continue;
        LyrSyl *S = &L->syl[L->n++];
        memset(S, 0, sizeof *S);
        S->word0 = (unsigned char)first; first = 0;
        S->v = (unsigned char)v;
        for (int k = 0; k < non; k++) S->on[k] = on[k];
        S->non = (unsigned char)non;
        int j = 0, a = i - non;
        for (int k = a; k <= i && j < (int)sizeof S->txt - 1; k++) S->txt[j++] = w[k];
        S->txt[j] = 0;
        non = 0;
    }
    if (non && L->n && !first)                            // a stray final consonant (not Mipomi, but): a coda
        for (int k = 0; k < non && L->syl[L->n - 1].nco < LYR_MAXC; k++)
            L->syl[L->n - 1].co[L->syl[L->n - 1].nco++] = on[k];
}

// ─────────────────────── reading Japanese, Spanish, Swahili ───────────────────────
// Three languages whose spelling is (nearly) phonemic, read by ONE reader: letters → tokens (a vowel,
// or one or more consonants per grapheme) → every vowel opens a syllable, and the consonants between
// two vowels split by the language's rule. Spelling tables are per language; vowels are pure
// (a e i o u, the Japanese u unrounded). Approximations the voice forces: the "j" of jambo / ja is
// d + zh (the engine has no dzh), Spanish rr is three quick d taps (a trill, as near as it gets), the
// single r is the engine's r.
enum { LYR__RT_TRILL = 100, LYR__RT_DZH = 101 };   // composite consonants, expanded when emitted
typedef struct { unsigned char kind, id, acc, a, len, moraic; } LyrRTok;   // a = first letter, len = letters
static int lyr__rt_vowel(int c, int lang) {
    switch (c) {
    case 'a': return LV_AA; case 'e': return LV_EP; case 'i': return LV_IE; case 'o': return LV_OP;
    case 'u': return lang == LYR_JA ? LV_UW : LV_OE;
    }
    return -1;
}
// the consonant(s) the grapheme at w[i] stands for; returns the letters used (*nid = 0: none, skip)
static int lyr__rt_cons(const char *w, int i, int n, int lang, int *ids, int *nid) {
    char c = w[i], d = i + 1 < n ? w[i + 1] : 0, e = i + 2 < n ? w[i + 2] : 0;
    int front = (d == 'e' || d == 'i');
    *nid = 1;
#define LYR__ONE(x, k) do { ids[0] = (x); return (k); } while (0)
#define LYR__TWO(x, y, k) do { ids[0] = (x); ids[1] = (y); *nid = 2; return (k); } while (0)
    if (lang == LYR_ES) {
        if (c == 'c' && d == 'h') LYR__ONE(LC_CH, 2);
        if (c == 'l' && d == 'l') LYR__ONE(LC_Y, 2);
        if (c == 'r' && d == 'r') LYR__ONE(LYR__RT_TRILL, 2);
        if (c == 'r' && i == 0)   LYR__ONE(LYR__RT_TRILL, 1);     // a word-initial r is trilled too
        if (c == 'q' && d == 'u') LYR__ONE(LC_K, 2);              // qu: the u is silent
        if (c == 'g' && d == 'u' && (e == 'e' || e == 'i')) LYR__ONE(LC_G, 2);   // gue gui: silent u
        if (c == 'c') LYR__ONE(front ? LC_S : LC_K, 1);           // ce ci = s (as in the Americas)
        if (c == 'g') LYR__ONE(front ? LC_X : LC_G, 1);           // ge gi = the jota
        if (c == 'j') LYR__ONE(LC_X, 1);
        if (c == 'h') { *nid = 0; return 1; }                     // silent
        if (c == 'v' || c == 'b') LYR__ONE(LC_B, 1);
        if (c == 'z') LYR__ONE(LC_S, 1);
        if (c == '{') LYR__TWO(LC_N, LC_Y, 1);                    // the n-tilde
        if (c == 'y') LYR__ONE(LC_Y, 1);
    } else if (lang == LYR_JA) {
        if (c == 's' && d == 'h') LYR__ONE(LC_SH, 2);
        if (c == 'c' && d == 'h') LYR__ONE(LC_CH, 2);
        if (c == 't' && d == 's') LYR__TWO(LC_T, LC_S, 2);
        if (c == 'r' || c == 'l') LYR__ONE(LC_R, 1);
    } else if (lang == LYR_HAW) {                                 // h k l m n p w (+ loanword letters below)
        char b = i > 0 ? w[i - 1] : 0;
        if (c == 'w') LYR__ONE((b == 'i' || b == 'e') ? LC_V : LC_W, 1);   // w after i/e sounds as v
    } else {                                                      // Swahili
        if (c == 'n' && d == 'g' && e == '\'') LYR__ONE(LC_NG, 3);   // ng' = the ng of "sing"
        if (c == 'n' && d == 'y') LYR__TWO(LC_N, LC_Y, 2);
        if (c == 'c' && d == 'h') LYR__ONE(LC_CH, 2);
        if (c == 's' && d == 'h') LYR__ONE(LC_SH, 2);
        if (c == 'd' && d == 'h') LYR__ONE(LC_DH, 2);
        if (c == 't' && d == 'h') LYR__ONE(LC_TH, 2);
        if (c == 'g' && d == 'h') LYR__ONE(LC_G, 2);
        if (c == 'k' && d == 'h') LYR__ONE(LC_X, 2);
    }
    switch (c) {                                                  // the shared, plain letters
    case 'b': LYR__ONE(LC_B, 1); case 'd': LYR__ONE(LC_D, 1); case 'f': LYR__ONE(LC_F, 1);
    case 'g': LYR__ONE(LC_G, 1); case 'h': LYR__ONE(LC_H, 1); case 'k': LYR__ONE(LC_K, 1);
    case 'l': LYR__ONE(LC_L, 1); case 'm': LYR__ONE(LC_M, 1); case 'n': LYR__ONE(LC_N, 1);
    case 'p': LYR__ONE(LC_P, 1); case 'r': LYR__ONE(LC_R, 1); case 's': LYR__ONE(LC_S, 1);
    case 't': LYR__ONE(LC_T, 1); case 'v': LYR__ONE(LC_V, 1); case 'w': LYR__ONE(LC_W, 1);
    case 'z': LYR__ONE(LC_Z, 1); case 'c': LYR__ONE(LC_K, 1); case 'q': LYR__ONE(LC_K, 1);
    case 'y': LYR__ONE(LC_Y, 1); case 'x': LYR__TWO(LC_K, LC_S, 1); case 'j': LYR__ONE(LYR__RT_DZH, 1);
    }
#undef LYR__ONE
#undef LYR__TWO
    *nid = 0;
    return 1;                                                     // anything else: skipped
}
// can these two consonants start a Spanish syllable together (pr bl tr gr fl ...)?
static int lyr__es_onset2(int a, int b) {
    if (b == LC_W || b == LC_Y) return a != LC_W && a != LC_Y;   // any consonant + a glide (agua, tienda, cuando)
    if (b != LC_R && b != LC_L) return 0;
    if (a == LC_D || a == LC_T) return b == LC_R;                 // dr tr (no dl tl)
    return a == LC_P || a == LC_B || a == LC_K || a == LC_G || a == LC_F;
}
static int lyr__rt_put(signed char *dst, int k, int id) {         // append a consonant (composites expand)
    if (id == LYR__RT_TRILL) { for (int r = 0; r < 3 && k < LYR_MAXC; r++) dst[k++] = LC_D; return k; }
    if (id == LYR__RT_DZH)   { if (k < LYR_MAXC) dst[k++] = LC_D; if (k < LYR_MAXC) dst[k++] = LC_ZH; return k; }
    if (k < LYR_MAXC) dst[k++] = (signed char)id;
    return k;
}
static void lyr__rule_word(Lyric *L, const char *w, int n, int lang) {
    LyrRTok t[48]; int nt = 0, acc = 0, brk = 0;
    for (int i = 0; i < n && nt < 44; ) {
        if (w[i] == '^') { acc = 1; i++; continue; }              // an accent / a macron: the next vowel is a full one
        if (w[i] == '|') { brk = 1; i++; continue; }              // a hiatus / the okina: no diphthong across it
        int v = lyr__rt_vowel(w[i], lang);
        if (lang == LYR_ES && w[i] == 'y' && (i + 1 == n || lyr__rt_vowel(w[i + 1], lang) < 0)) v = LV_IE;   // y not before a vowel = i (muy, hoy, the word "y")
        if (v >= 0) {
            LyrRTok *T = &t[nt++];
            T->kind = LYR__TV; T->id = (unsigned char)v; T->acc = (unsigned char)acc;
            T->a = (unsigned char)i; T->len = 1; T->moraic = (unsigned char)brk;   // (moraic on a vowel = a break before it)
            acc = brk = 0; i++; continue;
        }
        int ids[3], nid, used = lyr__rt_cons(w, i, n, lang, ids, &nid);
        for (int k = 0; k < nid && nt < 44; k++) {
            LyrRTok *T = &t[nt++];
            T->kind = LYR__TC; T->id = (unsigned char)ids[k]; T->acc = 0;
            T->a = (unsigned char)i; T->len = (unsigned char)(k == 0 ? used : 0); T->moraic = 0;
        }
        i += used;
    }
    // Japanese moraic n: an n not followed by a vowel (or y) closes the syllable before it
    if (lang == LYR_JA)
        for (int k = 0; k < nt; k++)
            if (t[k].kind == LYR__TC && t[k].id == LC_N &&
                (k + 1 == nt || (t[k + 1].kind == LYR__TC && t[k + 1].id != LC_Y)))
                t[k].moraic = 1;
    // Spanish diphthongs: an unaccented i/u before another vowel is a glide (bien, agua); after a, e, o
    // (and not itself before a vowel) it melts into a gliding vowel (baile, hoy, causa)
    if (lang == LYR_ES) {
        for (int k = 0; k + 1 < nt; k++) {
            LyrRTok *A = &t[k], *B = &t[k + 1];
            if (A->kind != LYR__TV || B->kind != LYR__TV) continue;
            int ai = (A->id == LV_IE || A->id == LV_OE) && !A->acc;
            int bi = (B->id == LV_IE || B->id == LV_OE) && !B->acc;
            if (ai && B->id != A->id) { A->kind = LYR__TC; A->id = A->id == LV_IE ? LC_Y : LC_W; continue; }
            if (bi && !ai && !(k + 2 < nt && t[k + 2].kind == LYR__TV)) {
                int m = -1;
                if (B->id == LV_IE) m = A->id == LV_AA ? LV_AI : A->id == LV_EP ? LV_EI : A->id == LV_OP ? LV_OI : -1;
                else if (A->id == LV_AA) m = LV_AU;
                if (m >= 0) { A->id = (unsigned char)m; A->len = (unsigned char)(B->a + B->len - A->a); B->kind = 255; }
            }
        }
        int j = 0;
        for (int k = 0; k < nt; k++) if (t[k].kind != 255) t[j++] = t[k];
        nt = j;
    }
    // Hawaiian diphthongs (ai au ei oi ou, sung on one note) unless a macron or the okina keeps them apart
    if (lang == LYR_HAW) {
        for (int k = 0; k + 1 < nt; k++) {
            LyrRTok *A = &t[k], *B = &t[k + 1];
            if (A->kind != LYR__TV || B->kind != LYR__TV || A->acc || B->acc || B->moraic) continue;
            int m = -1;
            if (B->id == LV_IE) m = A->id == LV_AA ? LV_AI : A->id == LV_EP ? LV_EI : A->id == LV_OP ? LV_OI : -1;
            else if (B->id == LV_OE) m = A->id == LV_AA ? LV_AU : A->id == LV_OP ? LV_OO : -1;
            if (m >= 0) { A->id = (unsigned char)m; A->len = (unsigned char)(B->a + B->len - A->a); B->kind = 255; k++; }
        }
        int j = 0;
        for (int k = 0; k < nt; k++) if (t[k].kind != 255) t[j++] = t[k];
        nt = j;
    }
    // syllables: one per vowel; the consonants before it split into the last syllable's coda and its onset
    int first = 1, prevv = -1;
    for (int k = 0; k <= nt; k++) {
        if (k < nt && t[k].kind != LYR__TV) continue;
        int c0 = prevv + 1, c1 = k;                               // the consonant run t[c0..c1)
        int split = c0;                                           // [c0..split) → previous coda, [split..c1) → onset
        if (prevv < 0) split = c0;                                // before the first vowel: all onset
        else if (k == nt) split = c1;                             // after the last vowel: all coda
        else if (lang == LYR_JA) {
            if (c1 > c0 && t[c0].moraic) split = c0 + 1;          // the moraic n closes the syllable before
            else if (c1 - c0 >= 2 && t[c0].id == t[c0 + 1].id) split = c0 + 1;   // kitte: the small tsu
        } else if (lang == LYR_ES) {
            int r = c1 - c0;
            if (r >= 2 && lyr__es_onset2(t[c1 - 2].id, t[c1 - 1].id)) split = c1 - 2;
            else if (r >= 2 && t[c1 - 1].id == LC_Y && t[c1 - 1].len == 0) split = c1 - 2;   // the n-tilde stays together
            else if (r >= 1) split = c1 - 1;
        }                                                         // Swahili, Hawaiian: every syllable open (all onset)
        if (prevv >= 0 && L->n > 0) {                             // codas of the previous syllable
            LyrSyl *P = &L->syl[L->n - 1];
            for (int q = c0; q < split; q++) P->nco = (unsigned char)lyr__rt_put(P->co, P->nco, t[q].id);
        }
        if (k == nt || L->n >= LYR_MAXSYL) break;
        LyrSyl *S = &L->syl[L->n++];
        memset(S, 0, sizeof *S);
        S->word0 = (unsigned char)first; first = 0;
        S->v = t[k].id;
        for (int q = split; q < c1; q++) S->non = (unsigned char)lyr__rt_put(S->on, S->non, t[q].id);
        // its letters, for display: from its first onset letter up to the next syllable's (codas included)
        int a = split < c1 ? t[split].a : t[k].a;
        int b = n;
        for (int q = k + 1; q < nt; q++) if (t[q].kind == LYR__TV) {   // where the next syllable's onset starts
            int nc0 = k + 1, nc1 = q, nsplit = nc1 - 1;
            if (lang == LYR_SW || lang == LYR_HAW || nc1 == nc0) nsplit = nc0;
            else if (lang == LYR_JA) nsplit = (t[nc0].moraic || (nc1 - nc0 >= 2 && t[nc0].id == t[nc0 + 1].id)) ? nc0 + 1 : nc0;
            else if (nc1 - nc0 >= 2 && (lyr__es_onset2(t[nc1 - 2].id, t[nc1 - 1].id) || (t[nc1 - 1].id == LC_Y && t[nc1 - 1].len == 0))) nsplit = nc1 - 2;
            b = nsplit < nc1 ? t[nsplit].a : t[q].a;
            break;
        }
        int j = 0;
        for (int q = a; q < b && j < (int)sizeof S->txt - 1; q++)
            if (w[q] != '^' && w[q] != '|') S->txt[j++] = w[q] == '{' ? 'n' : w[q];
        S->txt[j] = 0;
        prevv = k;
    }
}

// a melisma: the previous syllable's vowel once more; its coda moves to this last one
static void lyr__hold(Lyric *L) {
    if (!L->n || L->n >= LYR_MAXSYL) return;
    LyrSyl *P = &L->syl[L->n - 1], *S = &L->syl[L->n];
    memset(S, 0, sizeof *S);
    S->v = P->v;
    S->hold = 1;
    for (int i = 0; i < P->nco; i++) S->co[i] = P->co[i];
    S->nco = P->nco;
    P->nco = 0;
    S->txt[0] = '-'; S->txt[1] = 0;
    L->n++;
}

// text → syllables. Unknown bytes separate words; UTF-8 é è ê → e, ë ï ö ü → a hiatus mark + the vowel.
static void lyr_set(Lyric *L, int lang, const char *text) {
    int keep = L->keep_en;
    memset(L, 0, sizeof *L);
    L->lang = lang;
    L->keep_en = keep;
    char w[40];
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)text; ; p++) {
        int c = *p, letter = 0;
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c == 0xC3 && p[1]) {                           // UTF-8 Latin-1 accents
            int d = p[1]; p++;
            if (d >= 0x80 && d <= 0x9F) d += 0x20;         // upper → lower
            if (d == 0xB1) { if (n < (int)sizeof w - 1) w[n++] = '{'; continue; }   // the Spanish n-tilde: its own letter
            int hi = (d == 0xAB || d == 0xAF || d == 0xB6 || d == 0xBC);   // ë ï ö ü
            int ac = (d == 0xA9 || d == 0xA1 || d == 0xB3 || d == 0xBA || d == 0xAD);   // é á ó ú í
            c = (d >= 0xA8 && d <= 0xAB) ? 'e' : (d >= 0xAC && d <= 0xAF) ? 'i' :
                (d >= 0xB2 && d <= 0xB6) ? 'o' : (d >= 0xB9 && d <= 0xBC) ? 'u' :
                (d >= 0xA0 && d <= 0xA5) ? 'a' : 0;
            if (hi && n < (int)sizeof w - 1) w[n++] = '|';
            if (ac && n < (int)sizeof w - 1) w[n++] = '^';
        }
        if ((c == 0xC4 || c == 0xC5) && p[1]) {            // UTF-8 macron vowels (Hawaiian, Maori): a long vowel = a full one
            int d = p[1] | (c << 8); p++;
            c = (d == 0xC480 || d == 0xC481) ? 'a' : (d == 0xC492 || d == 0xC493) ? 'e' : (d == 0xC4AA || d == 0xC4AB) ? 'i' :
                (d == 0xC58C || d == 0xC58D) ? 'o' : (d == 0xC5AA || d == 0xC5AB) ? 'u' : 0;
            if (c && n < (int)sizeof w - 1) w[n++] = '^';
        }
        if (c == 0xCA && p[1] == 0xBB) {                   // the Hawaiian okina (a glottal stop): a break between vowels
            p++;
            if (n && n < (int)sizeof w - 1) w[n++] = '|';
            continue;
        }
        if (c >= 'a' && c <= 'z') letter = 1;
        if (letter) { if (n < (int)sizeof w - 1) w[n++] = (char)c; continue; }
        if (c == '\'') {                                   // apostrophes: ignored ("'s", "m'n"), but Swahili's ng' needs its own
            if (lang == LYR_SW && n && n < (int)sizeof w - 1) w[n++] = '\'';
            if (lang == LYR_HAW && n && n < (int)sizeof w - 1) w[n++] = '|';    // a typed okina
            continue;
        }
        if (n) {
            if (lang == LYR_MIPOMI) lyr__mp_word(L, w, n);
            else if (lang >= LYR_JA && lang < LYR_NLANG) lyr__rule_word(L, w, n, lang);
            else lyr__nl_word(L, w, n);
            n = 0;
        }
        if (c == '-') {                                    // a lone "-" (not inside a word): a melisma
            int prev_sep = (p == (const unsigned char *)text) || p[-1] == ' ' || p[-1] == '-';
            int next_sep = p[1] == 0 || p[1] == ' ' || p[1] == '-';
            if (prev_sep && next_sep) lyr__hold(L);
        }
        if (!c) break;
    }
}

// the phonemes as text ("x@.lUk vAn"), for the selfcheck and a debug overlay
static void lyr_phonemes(const Lyric *L, char *out, int max) {
    int j = 0;
#define LYR__P(s) do { for (const char *q = (s); *q && j < max - 1; q++) out[j++] = *q; } while (0)
    for (int i = 0; i < L->n; i++) {
        const LyrSyl *S = &L->syl[i];
        if (i) LYR__P(S->word0 ? " " : ".");
        if (S->hold) { LYR__P("-"); continue; }
        for (int k = 0; k < S->non; k++) LYR__P(lyr__cname[S->on[k]]);
        LYR__P(lyr__vname[S->v]);
        for (int k = 0; k < S->nco; k++) LYR__P(lyr__cname[S->co[k]]);
    }
#undef LYR__P
    out[j] = 0;
}

// ───────────────────────────────── singing ─────────────────────────────────
// where to put vowel v (F1, F2) on INSTR_VOICE: a base vowel on its path + openness/frontness levers
// (note_aux 8/9: ×2^((x-0.5)·3.4) on F1/F2). One base for a whole diphthong, so its glide is smooth.
static int lyr__base_of(float f1, float f2) {
    int best = 0; float bd = 1e9f;
    for (int b = 0; b < 5; b++) {
        float d1 = logf(f1 / lyr__base[b][0]), d2 = logf(f2 / lyr__base[b][1]);
        float d = d1 * d1 + d2 * d2;
        if (d < bd) { bd = d; best = b; }
    }
    return best;
}
static float lyr__lever(float target, float base) {
    float x = 0.5f + log2f(target / base) / 3.4f;
    return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
}
static void lyr__place(int h, int base, float f1, float f2) {
    note_aux(h, 8, lyr__lever(f1, lyr__base[base][0]));
    note_aux(h, 9, lyr__lever(f2, lyr__base[base][1]));
}

typedef struct {
    Lyric *lyr;
    int slot;
    int pos;                     // the next syllable
    float t;                     // the singer's clock (s)
    // the held note
    int h, cur, base;
    int on_k; float on_at;       // the next onset consonant to fire, and when
    // the released note, still singing its tail (one at a time; a third note cuts it short)
    int rh, rsyl, rbase, rstage, rk;
    float rat, rt0;
    int parallel;                // 0 (default) = sing through the Klatt CASCADE formants (note_aux 19), 1 = the parallel model
    float size;                  // vocal-tract SIZE 0 giant .. 1 child (engine idx 1, its own default 0.33); < 0 (default) = leave it. singer_size()
    float whisper;               // 0 sung .. 1 whispered (engine idx 20: the voicing fades, breath alone). singer_whisper()
    float nasal;                 // 0 open .. 1 hummed, through the nose (voice_nasal). singer_nasal()
    float gravel;                // 0 clean .. 1 gravelly (engine idx 21: uneven, doubled cycles + rasp). singer_gravel()
    int scoop;                   // a note after a rest starts this many semitones LOW and slides up into it (0 = lands on it)
    float buzz;                  // EFFORT / BUZZ 0 relaxed, round .. 1 pressed, bright (breath + open quotient + tilt);
                                 // < 0 (the default) = leave the engine's own. Set with singer_buzz().
    // THE SINGER LAYER (after JIANT FM's, ideas only): what makes a voice sound sung, not played.
    float vib;                   // vibrato depth 0..1 (engine idx 5: 1 = ±0.7 semitone); 0 = none. Default 0.3
    float vib_hz;                // vibrato rate (Hz, 3..8). Default 5.5
    float vib_delay;             // the note starts STRAIGHT; vibrato blooms in after this (s), over LYR__VIB_BLOOM. Default 0.25
    float human;                 // per-period jitter + shimmer, 0 = a machine .. 1 = the default dose
    float glide;                 // legato notes slide from the last pitch on an S-curve over this (s) (+ a bit per semitone); 0 = jump. Default 0.07
    int last_midi; float last_end, on_t0, g_from, g_to, g_len; int gliding, vib_done;
} Singer;

static void singer_init(Singer *s, Lyric *L, int slot) {
    memset(s, 0, sizeof *s);
    s->lyr = L;
    s->slot = slot;
    s->cur = -1;
    s->buzz = -1.0f;
    s->size = -1.0f;
    s->vib = 0.3f; s->vib_hz = 5.5f; s->vib_delay = 0.25f;
    s->human = 1.0f;
    s->glide = 0.07f;
    s->last_end = -1.0f;
}
#define LYR__VIB_BLOOM 0.4f      // vibrato grows from 0 to full over this, after vib_delay
#define LYR__LEGATO_S  0.12f     // a note starting within this of the last one's release is legato (it glides)

// the voice's EFFORT on the held note and every next one, poked as raw params (the Singer owns note_aux
// 0, so the EFFORT macro's live path, which also rewrites the vowel, is not used): the same curve as
// sound.h's vox_apply_macros. In the cascade it is the BUZZ: open quotient + presence.
static void singer__buzz_poke(int h, float e) {
    note_aux(h, 2, 0.22f - 0.22f * e);                    // breath
    note_aux(h, 3, 0.85f - 0.65f * e);                    // open quotient
    note_aux(h, 4, 0.70f - 0.65f * e);                    // tilt
}
// WHISPER and NASAL on the held note and every next one (0 = off, the default)
static void singer_whisper(Singer *s, float x) {
    s->whisper = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    if (s->h) note_aux(s->h, 20, s->whisper);
}
static void singer_gravel(Singer *s, float x) {
    s->gravel = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    if (s->h) note_aux(s->h, 21, s->gravel);
}
static void singer_nasal(Singer *s, float x) {
    s->nasal = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    if (s->h) voice_nasal(s->h, s->nasal);
}
// the vocal-tract SIZE on the held note and every next one (0 giant .. 0.33 the voice's own .. 1 child)
static void singer_size(Singer *s, float x) {
    s->size = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    if (s->h) note_aux(s->h, 1, s->size);
}
static void singer_buzz(Singer *s, float e) {
    s->buzz = e < 0.0f ? 0.0f : e > 1.0f ? 1.0f : e;
    if (s->h) singer__buzz_poke(s->h, s->buzz);
}
static void singer_restart(Singer *s) { s->pos = 0; }   // back to the lyric's first syllable

// silence the singer NOW: the held note and a released one still singing its tail both let go. Call it
// before singer_init() on a singer that may be sounding (a song change): init forgets the notes' handles,
// so without this they are never released and hang.
static void singer_stop(Singer *s) {
    if (s->h)  note_off(s->h);
    if (s->rh) note_off(s->rh);
    s->h = s->rh = 0;
    s->cur = -1;
}

// the tail of a released note: 0 = diphthong glide, 1 = codas, 2 = done (note_off)
#define LYR__GLIDE_S 0.09f
static void singer__tail(Singer *s) {
    if (!s->rh) return;
    const LyrSyl *S = &s->lyr->syl[s->rsyl];
    const float *vf = lyr__vf[S->v];
    if (s->rstage == 0) {
        if (vf[2] <= 0.0f) { s->rstage = 1; s->rk = 0; s->rat = s->t; }
        else {
            float k = (s->t - s->rt0) / LYR__GLIDE_S; if (k > 1.0f) k = 1.0f;
            lyr__place(s->rh, s->rbase, vf[0] + (vf[2] - vf[0]) * k, vf[1] + (vf[3] - vf[1]) * k);
            if (k >= 1.0f) { s->rstage = 1; s->rk = 0; s->rat = s->t; }
        }
    }
    if (s->rstage == 1) {
        while (s->rstage == 1 && s->t >= s->rat) {
            if (s->rk < S->nco) {
                int c = S->co[s->rk++];
                voice_coda(s->rh, c);                     // a second one CHAINS from the first (sound.h)
                s->rat = s->t + lyr__cdur[c] + (lyr__cplos[c] ? 0.012f : 0.0f);
            } else {
                note_off(s->rh);                          // the last consonant has sounded: let it go
                s->rh = 0;
                s->rstage = 2;
            }
        }
    }
}

// release the held note: the tail (diphthong, codas) sings on, then the note lets go
static void singer_off(Singer *s) {
    if (!s->h) return;
    if (s->rh) { note_off(s->rh); s->rh = 0; }            // an older tail still going: cut it
    s->rh = s->h; s->rsyl = s->cur; s->rbase = s->base;
    s->rstage = 0; s->rt0 = s->t; s->rat = s->t; s->rk = 0;
    s->h = 0;
    s->last_end = s->t;
    singer__tail(s);
}

// start the next syllable on a new note. Returns the note handle (0 = an empty lyric).
static int singer_on(Singer *s, int midi, int vol) {
    if (!s->lyr || s->lyr->n <= 0) return 0;
    if (s->h) singer_off(s);                              // legato: the old one sings its tail meanwhile
    if (s->pos >= s->lyr->n) s->pos = 0;
    int i = s->pos++;
    const LyrSyl *S = &s->lyr->syl[i];
    const float *vf = lyr__vf[S->v];
    // legato (the last note let go just now, or is being let go by this one): start on ITS pitch and
    // slide to ours, the way a voice moves between notes; a note after a rest just starts
    int from = s->last_midi;
    int slide = s->glide > 0.0f && from > 0 && from != midi && s->last_end >= 0.0f && s->t - s->last_end < LYR__LEGATO_S;
    // a SCOOP (no legato slide to make): start a few semitones under the note and slide up into it,
    // the jazz singer's way in
    int scoop = !slide && s->scoop > 0;
    int start = slide ? from : scoop ? midi - s->scoop : midi;
    int h = note_on(start, s->slot, vol);
    if (h <= 0) return 0;
    s->h = h; s->cur = i;
    s->on_t0 = s->t; s->last_midi = midi;
    s->gliding = slide || scoop;
    if (s->gliding) {
        int d = midi > start ? midi - start : start - midi;
        s->g_from = (float)start; s->g_to = (float)midi;
        s->g_len = slide ? s->glide * (1.0f + 0.06f * (float)d)   // a wider leap takes a little longer
                         : 0.09f + 0.025f * (float)d;             // a scoop: quick, a little longer from further down
        note_glide(h, 16);                                // each frame's pitch step slides over a frame: no staircase
    }
    note_aux(h, 5, 0.0f);                                 // vibrato: none yet (singer_update blooms it)
    note_aux(h, 6, (s->vib_hz - 3.0f) / 5.0f);
    s->vib_done = s->vib <= 0.0f;
    note_aux(h, 11, 0.06f * s->human);                    // jitter: ±0.3 % pitch per period
    note_aux(h, 12, 0.05f * s->human);                    // shimmer: up to 3 % amplitude dips per period
    s->base = lyr__base_of(vf[0], vf[1]);
    note_aux(h, 0, s->base * 0.25f);                      // the base vowel on the engine's path
    note_aux(h, 15, 0.0f);                                // no reduction, no diphthong lever: ours
    note_aux(h, 18, 0.0f);
    note_aux(h, 19, s->parallel ? 0.0f : 1.0f);           // cascade formants: rounder, more present (chosen by ear)
    if (s->buzz >= 0.0f) singer__buzz_poke(h, s->buzz);
    if (s->size >= 0.0f) note_aux(h, 1, s->size);
    if (s->whisper > 0.0f) note_aux(h, 20, s->whisper);
    if (s->gravel > 0.0f) note_aux(h, 21, s->gravel);
    if (s->nasal > 0.0f) voice_nasal(h, s->nasal);
    lyr__place(h, s->base, vf[0], vf[1]);
    voice_coda(h, -1);
    s->on_k = 0; s->on_at = s->t;
    if (S->hold) { voice_consonant(h, -1); s->on_k = LYR_MAXC; }
    else if (S->non) { voice_consonant(h, S->on[0]); s->on_k = 1; s->on_at = s->t + lyr__cdur[S->on[0]] * 0.55f; }
    else voice_consonant(h, -1);
    return h;
}

// call once per frame: fires a cluster's later onset consonants and sings released tails
static void singer_update(Singer *s) {
    s->t += dt();
    if (s->h) {
        float a = s->t - s->on_t0;
        if (s->gliding) {                                 // the S-curve slide (smoothstep in pitch)
            float k = a / s->g_len;
            if (k >= 1.0f) { k = 1.0f; s->gliding = 0; }
            note_pitch(s->h, s->g_from + (s->g_to - s->g_from) * (k * k * (3.0f - 2.0f * k)));
        }
        if (!s->vib_done && a > s->vib_delay) {           // vibrato blooms in (then left alone)
            float k = (a - s->vib_delay) / LYR__VIB_BLOOM;
            if (k >= 1.0f) { k = 1.0f; s->vib_done = 1; }
            note_aux(s->h, 5, s->vib * k);
        }
    }
    if (s->h && s->cur >= 0) {
        const LyrSyl *S = &s->lyr->syl[s->cur];
        while (s->on_k < S->non && s->t >= s->on_at) {
            int c = S->on[s->on_k++];
            voice_consonant(s->h, c);                     // re-fired: the cluster's next consonant
            s->on_at = s->t + lyr__cdur[c] * 0.55f;
        }
    }
    singer__tail(s);
}

// the syllable sounding (or last sung), for display; "" before the first note
static const char *singer_text(const Singer *s) {
    return (s->cur >= 0 && s->lyr && s->cur < s->lyr->n) ? s->lyr->syl[s->cur].txt : "";
}

// ───────────────────────────────── selfcheck ─────────────────────────────────
#ifdef DE_SPEC
#include <stdio.h>
#include "spec.h"
static void lyr__expect_in(int lang, const char *text, const char *want) {
    static Lyric L;
    static char got[256], msg[320];
    L.keep_en = 0;
    lyr_set(&L, lang, text);
    lyr_phonemes(&L, got, sizeof got);
    snprintf(msg, sizeof msg, "lyric %s \"%s\" -> \"%s\" (want \"%s\")", lyr_lang_name[lang],
             text, got, want);
    expect(strcmp(got, want) == 0, msg);
}
#define lyr__expect_word(t, w) lyr__expect_in(LYR_NL, (t), (w))
static void lyr_selfcheck(void) {
    lyr__expect_word("ik hou van jou", "Ik hau vAn jau");
    lyr__expect_word("geluk",     "x@.lUk");        // ge- prefix: schwa, stress moves on
    lyr__expect_word("beter",     "bee.t@r");       // be- is the root here: long ee, -ter schwa
    lyr__expect_word("maken",     "maa.k@");        // open syllable = long; -en drops its n
    lyr__expect_word("mannen",    "mAn.n@");        // closed (double n) = short
    lyr__expect_word("lachen",    "lA.x@");         // ch keeps the a short
    lyr__expect_word("zingen",    "zIng.@");        // ng closes the first syllable
    lyr__expect_word("nacht",     "nAxt");
    lyr__expect_word("hond",      "hOnt");          // final devoicing
    lyr__expect_word("school",    "sxool");         // sch = s + x
    lyr__expect_word("praktisch", "prAk.tIs");      // -isch = s
    lyr__expect_word("huis",      "huis");
    lyr__expect_word("mijn",      "mein");
    lyr__expect_word("liefde",    "lief.d@");
    lyr__expect_word("vrolijk",   "vroo.l@k");      // -lijk reduced
    lyr__expect_word("eeuwig",    "ee.w@x");        // -ig reduced
    lyr__expect_word("de zee",    "d@ zee");
    lyr__expect_word("wereld",    "wee.r@lt");
    lyr__expect_word("vergeten",  "v@r.xee.t@");
    lyr__expect_word("straat",    "straat");
    lyr__expect_word("kunst",     "kUnst");
    lyr__expect_word("zo - ver",  "zoo.- vEr");     // a melisma holds the vowel
    lyr__expect_word("Café",      "kaa.fee");       // é marks the stress: open stressed e = ee
    // Mipomi: CV syllables, pure vowels; the aspect vowel is its own onsetless syllable (it melts)
    lyr__expect_in(LYR_MIPOMI, "mipo, fala a foko", "mie.po faa.laa aa fo.ko");
    lyr__expect_in(LYR_MIPOMI, "mi i mola, jo?",    "mie ie mo.laa jo");
    lyr__expect_in(LYR_MIPOMI, "hihi u wawa",       "hie.hie oe waa.waa");
    // Japanese (romaji): every mora a syllable, the moraic n and the small tsu close one, u unrounded
    lyr__expect_in(LYR_JA, "arigatou",    "aa.rie.gaa.to.u'");
    lyr__expect_in(LYR_JA, "konnichiwa",  "kon.nie.chie.waa");
    lyr__expect_in(LYR_JA, "kitte",       "kiet.te");
    lyr__expect_in(LYR_JA, "shinjitsu",   "shien.dzhie.tsu'");
    // Spanish: glides and diphthongs, rr trilled, ñ, qu/gu, the word "y"
    lyr__expect_in(LYR_ES, "corazón",     "ko.raa.son");
    lyr__expect_in(LYR_ES, "perro",       "pe.dddo");
    lyr__expect_in(LYR_ES, "baile",       "bai.le");
    lyr__expect_in(LYR_ES, "agua",        "aa.gwaa");
    lyr__expect_in(LYR_ES, "niño y guitarra", "nie.njo ie gie.taa.dddaa");
    // Swahili: open syllables, prenasalized mb/nd, ng', ny, every vowel its own
    lyr__expect_in(LYR_SW, "jambo",       "dzhaa.mbo");
    lyr__expect_in(LYR_SW, "ng'ombe",     "ngo.mbe");
    lyr__expect_in(LYR_SW, "nyumba",      "njoe.mbaa");
    lyr__expect_in(LYR_SW, "ndege wanaimba", "nde.ge waa.naa.ie.mbaa");
    // Hawaiian: open syllables, ai/au/ei/oi/ou on one note, the okina and macrons keep vowels apart, w = v after i/e
    lyr__expect_in(LYR_HAW, "aloha",            "aa.lo.haa");
    lyr__expect_in(LYR_HAW, "Hawai\xCA\xBBi",   "haa.wai.ie");
    lyr__expect_in(LYR_HAW, "m\xC4\x81kou",     "maa.koo");
    lyr__expect_in(LYR_HAW, "ewa iwi",          "e.vaa ie.vie");
}
#endif

#endif // LYRIC_H
