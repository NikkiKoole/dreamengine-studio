// runtime/loficity/arranger.h — NOT shelf: the `loficity` cart's private arranger (like lockup/, tenement/).
// A line-for-line port of Lofi Cities' planner, verified bit-identical against the site's own bundle run
// headless in node (see the cart header). Included once, by tools/carts/loficity.c; not a shared header.
#ifndef LOFICITY_ARRANGER_H
#define LOFICITY_ARRANGER_H
#include "words.h"
// ═════════════════════════════════════════════════════════════════════════════
// THE ARRANGER — a line-for-line port of Lofi Cities' planner (js/audio/arrange.js,
// engine.js planBar, rhythm.js, melody.js, harmony.js, styles/*.js). Same RNG, same
// streams, same draw ORDER, so a seed plans the same track the site would: key,
// progression, form, every layer switch, fill, push, bass line and lead phrase, for
// every one of the nine styles. Nothing in this block makes a sound — it turns a seed
// into timed events (seconds from the track's start).
//
// The styles' DATA (energy rows, progression banks, grids, fills, comps, qualities)
// is GENERATED from their live objects (styles_data.h); only each style's three
// functions (plan / voicing / bass) are ported by hand, in the lcs_<style>_* block.
// ═════════════════════════════════════════════════════════════════════════════

// ── core.js: hash + rng, all uint32 so JS's Math.imul maps 1:1 ──
typedef struct { uint32_t s; } Rng;
static double lc_hash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = 2166136261u ^ (a * 374761393u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= b * 668265263u;
    h = (h ^ (h >> 15)) * 2246822519u;
    h ^= c * 3266489917u;
    h = (h ^ (h >> 13)) * 3266489917u;
    h ^= h >> 16;
    return (double)h / 4294967296.0;
}
static double rn(Rng *r) {
    r->s += 1831565813u;
    uint32_t t = r->s;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}
static Rng lc_stream(uint32_t seed, uint32_t k) { Rng r = { (uint32_t)floor(lc_hash(seed, k, 0) * 4294967296.0) }; return r; }
static uint32_t lc_next_seed(uint32_t seed) { return (uint32_t)floor(lc_hash(seed, 7919, 0) * 4294967296.0); }
static int rfloor(Rng *r, int n) { return (int)floor(rn(r) * n); }
// rnd2 = +(a + r()*(b-a)).toFixed(d): printf rounds the exact binary value, as toFixed does
static double rnd2(Rng *r, double a, double b, int d) {
    char buf[64]; snprintf(buf, sizeof buf, "%.*f", d, a + rn(r) * (b - a));
    return strtod(buf, NULL);
}
static int mod12(int n) { return ((n % 12) + 12) % 12; }
static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
static void isort(int *a, int n) { for (int i = 1; i < n; i++) { int v = a[i], j = i - 1; while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; } a[j + 1] = v; } }

// ── the table TYPES the generated styles_data.h is written in ──
typedef struct { int root, q; double beats; } ProgCh;
typedef struct { const char *id; int n; ProgCh c[6]; } Prog;
enum { V_KICK, V_SNARE, V_HAT, V_RIM, V_CLAP, V_SHAKER, V_TOM, V_BLOCK, NDRUMV };
typedef struct { const char *id, *name; const char *lane[NDRUMV]; } Grid;           // lanes K S H R C X T W
typedef struct { const char *id; int hasRoll, nroll; int rollS[8]; double rollV[8]; int stop; int nk; int kicks[4]; int open; int voice; } Fill;
typedef struct { double s, len, v; int npick; int pick[6]; } CompCell;                 // npick -1 = the whole voicing
typedef struct { const char *id; int n; CompCell c[16]; } Comp;
typedef struct { int n; int idx[8]; double w[8]; } GrooveW;                            // grid index + weight, in the JS object's order
typedef struct { int idx; double a, b; } CompW;
typedef struct { int n; double v[6][2]; } XRange;                                      // a style's own energy entry
typedef struct {
    double stretch; int bpm[2]; double swing[2], snareLag[2], drumLevel, kitLp[2], epLp[2], tremDepth[2], strum[2], epVel[2];
    double hold[2], charleston[2], pulse[2], push, legato, lead, softLead, tone[2], wowCents[2], dust[2];
    double introHats; int leadFrom; double hatsFirst, breakDrums, breakLead;
    GrooveW groove[2];                   // [< 74 bpm, >= 74]
    int ncomp; CompW comp[10];           // styles with comps: the comp weights, in E.comp's own order
    XRange x[8];                         // the style's extra entries (see the ST_<STYLE>_X_* enums)
} Energy;
typedef struct {
    const char *id, *name, *desc;
    const Energy *en;
    const Prog *maj; int nmaj; const Prog *min; int nmin;
    const ProgCh *tmaj, *tmin; int ntmaj, ntmin;   // a turnaround per mode, each its own length (medieval: 3 vs 2)
    const Grid *grids; int ngrids; int brk;
    const Fill *fills; int nfills;
    const Comp *comps; int ncomps;
    int bVariation; double keysLevel;
} StyleDef;
#include "styles_data.h"
enum { S_JAZZHOP, S_PIANO, S_AMBIENT, S_BOSSA, S_SYNTH, S_HOUSE, S_GUITAR, S_SAD, S_MEDIEVAL };   // STYLES[] order

// ── harmony.js ──
static const char *NOTE_NAMES[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
static const int PENTA[2][5] = { { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 } };   // [major?]

typedef struct { int tonic, major; } Key;
typedef struct { int root, q; double start, beats; int onset, fin; } Chord;
typedef struct { char id[12]; Chord c[14]; int n; double beats; } Expanded;

static Expanded lc_expand(const Prog *p, int stretch, const ProgCh *turn, int nturn) {
    Expanded e; memset(&e, 0, sizeof e); snprintf(e.id, sizeof e.id, "%s", p->id);
    double t = 0;
    for (int i = 0; i < p->n; i++) {
        Chord c = { p->c[i].root, p->c[i].q, t, p->c[i].beats * stretch, 0, 0 };
        e.c[e.n++] = c; t += c.beats;
    }
    if (turn) {
        double cut = t - 4; int k = 0;
        for (int i = 0; i < e.n; i++) if (e.c[i].start < cut) {
            Chord c = e.c[i]; c.beats = fmin(c.beats, cut - c.start); e.c[k++] = c;
        }
        e.n = k; double s = cut;
        for (int i = 0; i < nturn; i++) { Chord c = { turn[i].root, turn[i].q, s, turn[i].beats, 0, 0 }; e.c[e.n++] = c; s += turn[i].beats; }
    }
    e.beats = t;
    return e;
}
static Key related_key(Key prev, Rng *r) {
    double u = rn(r);
    if (u < 0.15) return prev;
    if (u < 0.55) { Key k = { mod12(prev.tonic + (rn(r) < 0.5 ? 5 : 7)), prev.major }; return k; }
    if (u < 0.75) {
        if (prev.major) { Key k = { mod12(prev.tonic + 9), 0 }; return k; }
        Key k = { mod12(prev.tonic + 3), 1 }; return k;
    }
    Key k; k.tonic = rfloor(r, 12); k.major = !(rn(r) < 0.55); return k;
}
static Key random_key(Rng *r) { Key k; k.tonic = rfloor(r, 12); k.major = !(rn(r) < 0.55); return k; }

// spell: the chord's colour tones (root dropped), capped at 4, a 9th added when thin
typedef struct { int root, n, iv[7]; } Spelled;
static Spelled lc_spell(Key key, const Chord *c) {
    Spelled s; s.root = mod12(key.tonic + c->root); s.n = 0;
    int has7 = 0;
    for (int i = 0; i < QUAL[c->q].n; i++) { int v = QUAL[c->q].iv[i]; if (v % 12 != 0) { s.iv[s.n++] = v; if (v == 7) has7 = 1; } }
    if (s.n > 4 && has7) { int k = 0; for (int i = 0; i < s.n; i++) if (s.iv[i] != 7) s.iv[k++] = s.iv[i]; s.n = k; }
    while (s.n > 4) s.n--;
    if (s.n < 4) { int has2 = 0; for (int i = 0; i < s.n; i++) if (s.iv[i] % 12 == 2) has2 = 1; if (!has2) s.iv[s.n++] = 14; }
    return s;
}
typedef struct { int n, m[8]; } Voicing;
static double vmean(const Voicing *v) { double s = 0; for (int i = 0; i < v->n; i++) s += v->m[i]; return s / v->n; }
static double lead_cost(const Voicing *a, const Voicing *prev, int reg) {
    double c = 0.5 * fabs(vmean(a) - 62 - (prev ? 0 : reg));
    if (!prev) return c;
    int n = a->n < prev->n ? a->n : prev->n;
    for (int i = 0; i < n; i++) c += abs(a->m[i] - prev->m[i]);
    return c + 2 * fmax(0, abs(a->m[a->n - 1] - prev->m[prev->n - 1]) - 4);
}
// voice: every close + drop-2 inversion in register, the cheapest by voice-leading cost
static Voicing lc_voice(const Spelled *sp, const Voicing *prev, int reg) {
    int pcs[7], n = 0;
    for (int i = 0; i < sp->n; i++) { int p = sp->iv[i] % 12, dup = 0; for (int j = 0; j < n; j++) if (pcs[j] == p) dup = 1; if (!dup) pcs[n++] = p; }
    isort(pcs, n);
    int shapes[14][7], ns = 0;
    for (int k = 0; k < n; k++) {
        int cl[7], cn = 0;
        for (int j = 0; j < n; j++) { int v = pcs[(k + j) % n]; while (cn && v <= cl[cn - 1]) v += 12; cl[cn++] = v; }
        memcpy(shapes[ns++], cl, sizeof cl);
        if (n >= 3) { int d2[7]; memcpy(d2, cl, sizeof cl); d2[n - 2] -= 12; isort(d2, n); memcpy(shapes[ns++], d2, sizeof d2); }
    }
    Voicing best = { 0 }; double bc = INFINITY; int any = 0;
    for (int s = 0; s < ns; s++) for (int o = 1; o <= 6; o++) {
        Voicing v; v.n = n;
        for (int i = 0; i < n; i++) v.m[i] = sp->root + shapes[s][i] + 12 * o;
        if (v.m[0] < 48 || v.m[0] > 60 || v.m[n - 1] > 76) continue;
        int ok = 1; for (int i = 1; i < n; i++) if (v.m[i] - v.m[i - 1] < 3 && v.m[i - 1] < 52) ok = 0;
        if (!ok) continue;
        double k = lead_cost(&v, prev, reg);
        if (!any || k < bc) { bc = k; best = v; any = 1; }
    }
    if (!any) {
        best.n = sp->n;
        for (int i = 0; i < sp->n; i++) best.m[i] = 48 + mod12(sp->root + sp->iv[i] - 48) + (sp->iv[i] >= 12 ? 12 : 0);
        isort(best.m, best.n);
    }
    return best;
}
static int bass_note(int pc, int prev) {
    int best = 33 + mod12(pc - 33), bd = 1 << 30;
    for (int n = best; n <= 47; n += 12) { int d = abs(n - prev); if (d < bd) { bd = d; best = n; } }
    return best;
}

// ── rhythm.js ──
static const char LANE_CH[NDRUMV] = { 'K', 'S', 'H', 'R', 'C', 'X', 'T', 'W' };
static const double VEL[NDRUMV] = { 0.92, 0.88, 0.8, 0.8, 0.85, 0.7, 0.85, 0.75 };
static const double HAT_ACCENT[4] = { 1, 0.45, 0.7, 0.45 };
typedef struct { int v, s, ghost, open; double acc, vel; } Hit;   // vel < 0 = unset
typedef struct { Hit h[80]; int n; } Hits;
static Hits parse_grid(const Grid *g) {
    Hits o; o.n = 0;
    for (int L = 0; L < NDRUMV; L++) {
        const char *s = g->lane[L]; if (!s) continue;
        for (int i = 0; s[i]; i++) {
            char c = s[i]; if (c == '.') continue;
            Hit h = { L, i, 0, 0, 0, -1 };
            if (L == V_SNARE) h.ghost = c == 'g';
            else if (L == V_HAT) h.open = c == 'o';
            else if (c == 'g') h.ghost = 1;
            if (c == 'a') h.acc = 1.2; else if (c == 's') h.acc = 0.55;
            o.h[o.n++] = h;
        }
    }
    return o;
}
enum { F_NONE = -1 };
enum { COMP_HOLD, COMP_CHARLESTON, COMP_PULSE, NCOMP };            // the jazzhop (default) comps
static const int COMPS[NCOMP][2][2] = { { { 0, 16 }, { -1, 0 } }, { { 0, 3 }, { 6, 10 } }, { { 0, 6 }, { 8, 8 } } };
static const int COMPN[NCOMP] = { 1, 2, 2 };
static double lc_swing(int step, double sd, double pct) { return step % 2 ? ((pct - 50) / 50) * sd : 0; }

// ── melody.js ──
static const int CELLS[8][5] = { { 0, 3, 6, 8 }, { 2, 4, 7 }, { 0, 6, 10, 12, 14 }, { 0, 2, 6, 8 }, { 3, 6, 10 }, { 0, 4, 6, 10 }, { 2, 6, 10, 12 }, { 0, 3, 8, 10 } };
static const int CELLN[8] = { 4, 3, 5, 4, 3, 4, 4, 4 };
static const int TAILS[6][2] = { { 0 }, { 16 }, { 16, 19 }, { 18 }, { 16, 22 }, { 20 } };
static const int TAILN[6] = { 0, 1, 2, 1, 2, 1 };
static const int ANSWERS[6][4] = { { 0, 4, 8 }, { 2, 6, 10 }, { 0, 3, 6, 10 }, { 4, 8 }, { 0, 6, 12 }, { 2, 4, 8, 12 } };
static const int ANSN[6] = { 3, 3, 4, 2, 3, 4 };
#define MEL_LO 67
#define MEL_HI 84
typedef struct { int n, steps[8], durs[8], contour[8]; double vel[8]; } Motif;
static void mk_contour(Rng *r, int n, int *c) {
    c[0] = 0; int dir = rn(r) < 0.5 ? 1 : -1, back = 0;
    for (int i = 1; i < n; i++) {
        int mv;
        if (back) { mv = -back; back = 0; }
        else if (rn(r) < 0.75) { int a = 1 + (int)floor(rn(r) * 2); mv = a * (rn(r) < 0.65 ? dir : -dir); }
        else { mv = (2 + (int)floor(rn(r) * 2)) * dir; back = mv > 0 ? 1 : mv < 0 ? -1 : 0; }
        c[i] = c[i - 1] + mv;
        if (abs(c[i]) > 4) dir = c[i] > 0 ? -1 : 1;
    }
}
static Motif make_motif(Rng *r) {
    Motif m; int ci = rfloor(r, 8), n = 0;
    for (int i = 0; i < CELLN[ci]; i++) m.steps[n++] = CELLS[ci][i];
    int ti = rfloor(r, 6);
    for (int i = 0; i < TAILN[ti]; i++) m.steps[n++] = TAILS[ti][i];
    if (n > 6) n = 6;
    while (n < 3) { m.steps[n] = m.steps[n - 1] + 4; n++; }
    m.n = n;
    int last = 4 + rfloor(r, 5);
    for (int i = 0; i < n; i++) m.durs[i] = i < n - 1 ? (6 < m.steps[i + 1] - m.steps[i] ? 6 : m.steps[i + 1] - m.steps[i]) : last;
    mk_contour(r, n, m.contour);
    for (int i = 0; i < n; i++) m.vel[i] = i == 0 ? 0.78 : 0.62 + 0.1 * rn(r);
    return m;
}
typedef struct { int s, d, midi; double v; } LNote;
typedef struct { LNote n[16]; int cnt; } Phrase;
typedef struct { int n, pc[7]; } Pcs;
typedef Pcs (*ChordAtFn)(void *ctx, int s);
typedef struct { int scale[40], n; } Scale;
static int pcs_has(const Pcs *p, int pc) { for (int i = 0; i < p->n; i++) if (p->pc[i] == pc) return 1; return 0; }
static int s_in(const Scale *S, int i) { return i >= 0 && i < S->n && S->scale[i] >= MEL_LO && S->scale[i] <= MEL_HI; }
static int s_nearest(const Scale *S, double target, const Pcs *pcs) {
    if (!isfinite(target)) target = 74;
    int best = -1; double bd = INFINITY;
    for (int i = 0; i < S->n; i++) {
        if (!s_in(S, i) || (pcs && !pcs_has(pcs, mod12(S->scale[i])))) continue;
        double d = fabs(S->scale[i] - target); if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) return best;
    if (pcs) return s_nearest(S, target, NULL);
    for (int i = 0; i < S->n; i++) if (S->scale[i] >= MEL_LO) return i;
    return 0;
}
static int s_chord_tone(const Scale *S, int i, Pcs pcs) {
    if (i >= 0 && i < S->n && pcs_has(&pcs, mod12(S->scale[i]))) return i;
    static const int D[4] = { 1, -1, 2, -2 };
    for (int k = 0; k < 4; k++) { int j = i + D[k]; if (j >= 0 && j < S->n && s_in(S, j) && pcs_has(&pcs, mod12(S->scale[j]))) return j; }
    return i;
}
static int s_clamp(const Scale *S, double x) {
    int i = (int)floor(x + 0.5);                       // Math.round
    if (i < 0) i = 0; if (i > S->n - 1) i = S->n - 1;
    while (i > 0 && S->scale[i] > MEL_HI) i--;
    while (i < S->n - 1 && S->scale[i] < MEL_LO) i++;
    return i;
}
typedef struct { int statement, inv, shift, hasPrev, prevLast, reg; } Variant;
static Phrase lc_phrase(const Motif *mo, ChordAtFn chordAt, void *ctx, Key key, Rng *r, Variant va) {
    Scale S; S.n = 0;
    for (int m = 55; m <= 96; m++) for (int k = 0; k < 5; k++) if (mod12(key.tonic + PENTA[key.major][k]) == mod12(m)) { S.scale[S.n++] = m; break; }
    struct { int s, d, i; double v; } nt[16]; int nn = 0;
    int shift = va.shift, cont[8];
    for (int i = 0; i < mo->n; i++) cont[i] = va.inv ? -mo->contour[i] : mo->contour[i];
    double tgt = (va.hasPrev ? va.prevLast : 74 + va.reg) + (rn(r) * 4 - 2);
    Pcs p0 = chordAt(ctx, shift);
    int anchor = s_nearest(&S, tgt, &p0);
    for (int i = 0; i < mo->n; i++) {
        int s = mo->steps[i] + shift; if (s > 31) continue;
        int idx = s_clamp(&S, anchor + cont[i]);
        if (s % 16 == 0 || s % 16 == 8) idx = s_chord_tone(&S, idx, chordAt(ctx, s));
        nt[nn].s = s; nt[nn].d = mo->durs[i]; nt[nn].i = idx; nt[nn].v = mo->vel[i]; nn++;
    }
    if (va.statement && va.statement % 3 == 0 && nn > 1) {
        int L = nn - 1;
        nt[L].i = s_clamp(&S, nt[L].i + (rn(r) < 0.5 ? 2 : -1));
        nt[L].d = nt[L].d - 2 > 2 ? nt[L].d - 2 : 2;
        int s2 = nt[L].s + nt[L].d + 1; if (s2 > 31) s2 = 31;
        nt[nn].s = s2; nt[nn].d = 4; nt[nn].i = s_clamp(&S, s_chord_tone(&S, nt[L].i - 1, chordAt(ctx, 30))); nt[nn].v = 0.6; nn++;
    }
    int ai = rfloor(r, 6);
    int cur = nn ? nt[nn - 1].i : anchor;
    for (int k = 0; k < ANSN[ai]; k++) {
        int c = ANSWERS[ai][k], s = 48 + c, mv;
        if (rn(r) < 0.75) { int sg = rn(r) < 0.5 ? -1 : 1; mv = sg * (1 + (int)floor(rn(r) * 2)); }
        else mv = rn(r) < 0.5 ? -3 : 3;
        cur = s_clamp(&S, cur + mv);
        if (k == ANSN[ai] - 1 || s % 16 == 0 || s % 16 == 8) cur = s_chord_tone(&S, cur, chordAt(ctx, s));
        int d;
        if (k < ANSN[ai] - 1) { d = ANSWERS[ai][k + 1] - c; if (d > 6) d = 6; }
        else { int a = 63 - s, b = 4 + rfloor(r, 5); d = a < b ? a : b; }
        nt[nn].s = s; nt[nn].d = d > 1 ? d : 1; nt[nn].i = cur; nt[nn].v = 0.58 + 0.12 * rn(r); nn++;
    }
    int lo = 1 << 30; for (int i = 0; i < nn; i++) if (S.scale[nt[i].i] < lo) lo = S.scale[nt[i].i];
    Phrase ph; ph.cnt = 0;
    for (int i = 0; i < nn; i++) {
        int m = S.scale[nt[i].i];
        while (m > lo + 12) m -= 12;
        int found = 0; for (int j = 0; j < S.n; j++) if (S.scale[j] == m) found = 1;
        if (!found) m = S.scale[s_nearest(&S, m, NULL)];
        LNote x = { nt[i].s, nt[i].d, m, nt[i].v };
        int j = ph.cnt++;                                   // stable insert by s
        while (j > 0 && ph.n[j - 1].s > x.s) { ph.n[j] = ph.n[j - 1]; j--; }
        ph.n[j] = x;
    }
    return ph;
}

// ── arrange.js: vibes, form, sections ──
enum { EN_CHILL, EN_BALANCED, EN_UPBEAT, NENERGY };
static const char *ENERGY_NAME[NENERGY] = { "chill", "balanced", "upbeat" };
enum { BAND_FULL, BAND_NODRUMS, BAND_KEYS, NBAND };
static const char *BAND_NAME[NBAND] = { "full band", "no drums", "chords only" };
enum { S_INTRO, S_A, S_B, S_BREAK, S_OUTRO };
static const char *SEC_NAME[5] = { "intro", "A", "B", "break", "outro" };
static const int FORMS[3][8][2] = {
    { { S_INTRO, 4 }, { S_A, 8 }, { S_A, 8 }, { S_B, 8 }, { S_BREAK, 4 }, { S_A, 8 }, { S_B, 8 }, { S_OUTRO, 4 } },
    { { S_INTRO, 2 }, { S_A, 8 }, { S_B, 8 }, { S_A, 8 }, { S_BREAK, 8 }, { S_B, 8 }, { S_A, 8 }, { S_OUTRO, 4 } },
    { { S_INTRO, 4 }, { S_A, 16 }, { S_BREAK, 4 }, { S_B, 8 }, { S_A, 16 }, { S_OUTRO, 4 }, { -1, 0 } },
};
enum { EP_COMP, EP_INTRO, EP_WHOLE, EP_OUTRO };
enum { BS_KICK, BS_LAST, BS_NONE, BS_WHOLE, BS_OUTRO };
static const char *BS_NAME[5] = { "kick", "last", "none", "whole", "outro" };
enum { DR_PATTERN, DR_HATS2, DR_NONE, DR_P5, DR_OUTRO };
typedef struct { int ep, bass, drums, kit, lead, hatsFirst, dipLast; double level; } Layers;
enum { PR_A, PR_B, PR_FINAL };
typedef struct { int name, bars, prog; Layers L; } Section;

// A style's own plan data — kit parameters, synth/house/medieval settings, the lead's timbre — kept as
// named values exactly as their plan objects hold them ("drums.kit.kickF0", "piano.lh", "lead.timbre"),
// so the port reads like theirs and the oracle can diff every one of them (dump_plan_kv).
#define KV_MAX 96
typedef struct { char k[40]; double v; char s[20]; int isStr; } KV;
#define MAXSEC 16
typedef struct {
    uint32_t seed, nextSeed;
    int energy, band, city, style;
    int bpm; double swing, snareLag;
    Key key;
    Expanded progA, progB; int chordBars;
    int pattern; double drumLevel;
    struct { double lp, tremRate, tremDepth, strum, vel, comp[10], push; int ncomp; int compIdx[10]; } ep;
    int legato; double bassFactor;
    int hasLead; double leadPan; Motif motif;
    double tone, wowCents, dust;
    int reg, form;
    Section sec[MAXSEC]; int nsec, bars;
    double duration;
    char title[40];
    KV kv[KV_MAX]; int nkv;
} Plan;
static KV *kv_find(const Plan *P, const char *k) { for (int i = 0; i < P->nkv; i++) if (!strcmp(P->kv[i].k, k)) return (KV *)&P->kv[i]; return NULL; }
static void kv_set(Plan *P, const char *k, double v) {
    KV *e = kv_find(P, k); if (!e) { if (P->nkv >= KV_MAX) return; e = &P->kv[P->nkv++]; snprintf(e->k, sizeof e->k, "%s", k); }
    e->v = v; e->isStr = 0; e->s[0] = 0;
}
static void kv_sets(Plan *P, const char *k, const char *s) {
    KV *e = kv_find(P, k); if (!e) { if (P->nkv >= KV_MAX) return; e = &P->kv[P->nkv++]; snprintf(e->k, sizeof e->k, "%s", k); }
    e->v = 0; e->isStr = 1; snprintf(e->s, sizeof e->s, "%s", s);
}
static double kv(const Plan *P, const char *k) { KV *e = kv_find(P, k); return e ? e->v : 0; }
static const char *kvs(const Plan *P, const char *k) { KV *e = kv_find(P, k); return e && e->isStr ? e->s : ""; }
static int kv_has(const Plan *P, const char *k) { return kv_find(P, k) != NULL; }
static void kv_del_prefix(Plan *P, const char *pre) {   // `plan3.drums.kit = { … }` replaces the whole object
    int n = 0, L = (int)strlen(pre);
    for (int i = 0; i < P->nkv; i++) if (strncmp(P->kv[i].k, pre, L)) P->kv[n++] = P->kv[i];
    P->nkv = n;
}

// ── the title generator (makeTitle) — the city's words + templates ──
static const char *pick_s(Rng *r, const char **a, int n) { return a[rfloor(r, n)]; }
static void make_title(Rng *r, int city, char *out, int cap) {
    const LcCity *C = &LC_CITY[city];
    int NN = sizeof LC_NOUNS / sizeof *LC_NOUNS, NA = sizeof LC_ADJS / sizeof *LC_ADJS, NT = sizeof LC_TIMES / sizeof *LC_TIMES;
    int NTPL = sizeof LC_TPL / sizeof *LC_TPL; double total = 0; for (int i = 0; i < NTPL; i++) total += LC_TPL[i].w;
    for (int attempt = 0; attempt < 16; attempt++) {
        double u = rn(r) * total; const char *tpl = LC_TPL[0].t;
        for (int i = 0; i < NTPL; i++) if ((u -= LC_TPL[i].w) < 0) { tpl = LC_TPL[i].t; break; }
        char s[128]; int n = 0;
        for (const char *p = tpl; *p && n < 120; p++) {
            char tok[64] = { 0 };
            if (p[0] == '{' && p[1] && p[2] == '}') {
                const char *w;
                switch (p[1]) {
                case 'a': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_ADJS, NA)); break;
                case 'n': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_NOUNS, NN)); break;
                case 't': snprintf(tok, sizeof tok, "%s", pick_s(r, LC_TIMES, NT)); break;
                case 'p': snprintf(tok, sizeof tok, "%s", C->p[rfloor(r, C->np)]); break;
                case 'c': snprintf(tok, sizeof tok, "%s", C->c[rfloor(r, C->nc)]); break;
                case 'w': snprintf(tok, sizeof tok, "%s", C->w[rfloor(r, C->nw)]); break;
                case 'P': case 'Q':
                    w = p[1] == 'P' ? C->p[rfloor(r, C->np)] : C->w[rfloor(r, C->nw)];
                    { const char *a = pick_s(r, LC_ADJS, NA);
                      if (!strncmp(w, "the ", 4)) snprintf(tok, sizeof tok, "the %s %s", a, w + 4);
                      else snprintf(tok, sizeof tok, "%s %s", a, w); }
                    break;
                case 'R': w = C->w[rfloor(r, C->nw)];
                    if (!strncmp(w, "the ", 4)) snprintf(tok, sizeof tok, "%s", w); else snprintf(tok, sizeof tok, "the %s", w);
                    break;
                }
                for (int k = 0; tok[k] && n < 120; k++) s[n++] = tok[k];
                p += 2;
            } else s[n++] = *p;
        }
        s[n] = 0;
        char *ll; while ((ll = strstr(s, "last last "))) memmove(ll, ll + 5, strlen(ll + 5) + 1);
        for (char *q = s; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
        if ((int)strlen(s) <= 32) { snprintf(out, cap, "%s", s); return; }
    }
    snprintf(out, cap, "%.32s", C->p[rfloor(r, C->np)]);
}

// ── the STYLE HOOKS (S.plan / S.voicing / S.bass). NULL = the jazzhop default path. ──
typedef struct BarState BarState;
typedef struct BarInfo BarInfo;
typedef struct { int s, midi, slide; double vel, dur; } BassNote;      // vel < 0 = unset (0.82); dur in steps
typedef struct {
    const Plan *plan; BarState *st; int i; const BarInfo *b; int mode;
    const Chord *chords; int nch; const Chord *nextChords; int nnext;
    const int *kicks; int nkicks; int stop;                            // stop < 0 = none
    Rng *r;
} BassCtx;
typedef struct {
    void (*plan)(Plan *P, Rng *r, const Energy *E, int energy);
    Voicing (*voicing)(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st);
    int (*bass)(BassCtx *cx, BassNote *out);                             // returns the count, or -1 = null (default path)
} StyleHooks;
static const StyleHooks *style_hooks(int style);                        // defined after the style ports
static const StyleDef *style_of(const Plan *P) { return STYLES[P->style]; }

static void pick_w(Rng *r, const GrooveW *g, int *out) {
    double sum = 0; for (int i = 0; i < g->n; i++) sum += g->w[i];
    double u = rn(r) * sum;
    for (int i = 0; i < g->n; i++) if ((u -= g->w[i]) < 0) { *out = g->idx[i]; return; }
    *out = g->idx[0];
}

// planTrack — one seed → the whole track: key, harmony, groove, kit, keys, lead, form
static Plan plan_track(uint32_t seed, const Key *prevKey, int style, int energy, int band, int city) {
    static Plan P; memset(&P, 0, sizeof P);
    P.seed = seed; P.energy = energy; P.band = band; P.city = city; P.style = style;
    const StyleDef *S = STYLES[style];
    const Energy *E = &S->en[energy];
    Rng rH = lc_stream(seed, 1), rR = lc_stream(seed, 2), rM = lc_stream(seed, 3), rA = lc_stream(seed, 4), rT = lc_stream(seed, 5);
    P.key = prevKey ? related_key(*prevKey, &rH) : random_key(&rH);
    const Prog *bank = P.key.major ? S->maj : S->min; int nb = P.key.major ? S->nmaj : S->nmin;
    int idA = rfloor(&rH, nb);
    P.chordBars = rn(&rH) < E->stretch ? 2 : 1;
    int idB = idA, turn = 0;
    if (rn(&rH) < 0.6) {
        int others[12], no = 0; for (int i = 0; i < nb; i++) if (i != idA) others[no++] = i;
        int k = rfloor(&rH, no); idB = k < no ? others[k] : idA;
    } else turn = 1;
    P.progA = lc_expand(&bank[idA], P.chordBars, NULL, 0);
    P.progB = lc_expand(&bank[idB], P.chordBars, turn ? (P.key.major ? S->tmaj : S->tmin) : NULL, P.key.major ? S->ntmaj : S->ntmin);
    if (turn) snprintf(P.progB.id, sizeof P.progB.id, "%s+ii-V", bank[idA].id);
    P.bpm = E->bpm[0] + (int)floor(rn(&rR) * (E->bpm[1] - E->bpm[0] + 1));
    P.swing = rnd2(&rR, E->swing[0], E->swing[1], 1);
    P.snareLag = rnd2(&rR, E->snareLag[0], E->snareLag[1], 4);
    pick_w(&rR, &E->groove[P.bpm < 74 ? 0 : 1], &P.pattern);
    kv_set(&P, "drums.kit.kickF0", rnd2(&rR, 150, 170, 1));
    kv_set(&P, "drums.kit.kickF1", rnd2(&rR, 46, 56, 1));
    kv_set(&P, "drums.kit.kickDecay", rnd2(&rR, 0.11, 0.15, 3));
    kv_set(&P, "drums.kit.snareDecay", rnd2(&rR, 0.05, 0.08, 3));
    kv_set(&P, "drums.kit.hatHP", rnd2(&rR, 6000, 8500, 0));
    kv_set(&P, "drums.kit.hatClosed", rnd2(&rR, 0.012, 0.02, 3));
    kv_set(&P, "drums.kit.lp", rnd2(&rR, E->kitLp[0], E->kitLp[1], 0));
    P.ep.lp = rnd2(&rA, E->epLp[0], E->epLp[1], 0);
    P.ep.tremRate = rnd2(&rA, 3.5, 5.5, 2);
    P.ep.tremDepth = rnd2(&rA, E->tremDepth[0], E->tremDepth[1], 3);
    P.ep.strum = rnd2(&rA, E->strum[0], E->strum[1], 4);
    P.ep.vel = rnd2(&rA, E->epVel[0], E->epVel[1], 3);
    if (S->comps) {       // ep.comp = fromEntries(E.comp → rnd2(rA, …w, 2)), in E.comp's own order
        P.ep.ncomp = E->ncomp;
        for (int i = 0; i < E->ncomp; i++) { P.ep.compIdx[i] = E->comp[i].idx; P.ep.comp[i] = rnd2(&rA, E->comp[i].a, E->comp[i].b, 2); }
    } else {
        P.ep.ncomp = NCOMP;
        P.ep.comp[COMP_HOLD] = rnd2(&rA, E->hold[0], E->hold[1], 2);
        P.ep.comp[COMP_CHARLESTON] = rnd2(&rA, E->charleston[0], E->charleston[1], 2);
        P.ep.comp[COMP_PULSE] = rnd2(&rA, E->pulse[0], E->pulse[1], 2);
        for (int i = 0; i < NCOMP; i++) P.ep.compIdx[i] = i;
    }
    P.ep.push = E->push;
    P.legato = rn(&rA) < E->legato;
    P.bassFactor = P.legato ? 0.92 : 0.55;
    if (rn(&rA) < E->lead) {
        P.hasLead = 1;
        kv_sets(&P, "lead.timbre", rn(&rA) < E->softLead ? "soft" : "vibes");
        P.leadPan = rnd2(&rA, -0.3, 0.3, 2);
        kv_set(&P, "lead.pan", P.leadPan);
        P.motif = make_motif(&rM);
    }
    P.tone = rnd2(&rA, E->tone[0], E->tone[1], 0);
    P.wowCents = rnd2(&rA, E->wowCents[0], E->wowCents[1], 1);
    P.dust = rnd2(&rA, E->dust[0], E->dust[1], 2);
    { Rng r8 = lc_stream(seed, 8); P.reg = (int)floor(rn(&r8) * 11) - 5; }
    double barDur = 240.0 / P.bpm;
    P.form = rfloor(&rA, 3);
    int nm[MAXSEC], nbar[MAXSEC], n = 0;
    for (int i = 0; i < 8 && FORMS[P.form][i][0] >= 0; i++) { nm[n] = FORMS[P.form][i][0]; nbar[n] = FORMS[P.form][i][1]; n++; }
#define TOTAL() ({ int t_ = 0; for (int q_ = 0; q_ < n; q_++) t_ += nbar[q_]; t_; })
#define INSERT() do { nm[n + 1] = nm[n - 1]; nbar[n + 1] = nbar[n - 1]; nm[n - 1] = S_B; nbar[n - 1] = 8; nm[n] = S_A; nbar[n] = 8; n += 2; } while (0)
    while (TOTAL() * barDur < 150 && n + 2 <= MAXSEC) INSERT();
    if (rn(&rA) < 0.35 && (TOTAL() + 16) * barDur <= 240 && n + 2 <= MAXSEC) INSERT();
    int aCount = 0;
    for (int i = 0; i < n; i++) {
        Layers L = { EP_COMP, BS_KICK, DR_PATTERN, 1, 0, 0, 0, 1 };
        int s = nm[i];
        if (s == S_INTRO) {
            L.ep = EP_INTRO;
            L.bass = rn(&rA) < 0.5 ? BS_LAST : BS_NONE;
            L.drums = rn(&rA) < E->introHats ? DR_HATS2 : DR_NONE;
        } else if (s == S_A) { aCount++; L.lead = P.hasLead && aCount >= E->leadFrom; }
        else if (s == S_B) { L.lead = P.hasLead; L.hatsFirst = rn(&rA) < E->hatsFirst; }
        else if (s == S_BREAK) {
            L.ep = EP_WHOLE;
            L.bass = rn(&rA) < 0.5 ? BS_WHOLE : BS_NONE;
            L.drums = rn(&rA) < E->breakDrums ? DR_P5 : DR_NONE;
            L.lead = P.hasLead && rn(&rA) < E->breakLead;
        } else { L.ep = EP_OUTRO; L.bass = BS_OUTRO; L.drums = DR_OUTRO; }
        if ((s == S_A || s == S_B) && i < n - 1) L.dipLast = rn(&rA) < 0.25;
        P.sec[i].name = s; P.sec[i].bars = nbar[i];
        P.sec[i].prog = s == S_B ? PR_B : s == S_OUTRO ? PR_FINAL : PR_A;
        P.sec[i].L = L;
    }
    P.nsec = n; P.bars = TOTAL();
#undef TOTAL
#undef INSERT
    P.duration = P.bars * barDur;
    P.nextSeed = lc_next_seed(seed);
    P.drumLevel = E->drumLevel;
    make_title(&rT, city, P.title, sizeof P.title);
    const StyleHooks *H = style_hooks(style);
    if (H && H->plan) { Rng r7 = lc_stream(seed, 7); H->plan(&P, &r7, E, energy); }
    return P;
}
// bandLayers — full band / no drums / chords only (keys lifted by the style's keysLevel)
static Layers band_layers(Layers L, int band, double keysLevel) {
    if (band == BAND_NODRUMS) L.kit = 0;
    else if (band == BAND_KEYS) { L.kit = 0; L.bass = BS_NONE; L.lead = 0; L.level = keysLevel; }
    return L;
}

// ── engine.js: the per-bar planner ──
enum { K_FX, K_EP, K_BASS, K_KICK, K_SNARE, K_HAT, K_RIM, K_CLAP, K_SHAKER, K_TOM, K_BLOCK, K_LEAD };
static const char *K_NAME[] = { "fx", "ep", "bass", "kick", "snare", "hat", "rim", "clap", "shaker", "tom", "block", "lead" };
enum { FX_TONE_, FX_VINYL_, FX_DUST_, FX_WOW_, FX_LEVEL_ };
static const char *FXN[] = { "tone", "vinyl", "dust", "wow", "level" };
typedef struct {
    double t; int k, bar;
    int notes[8], nn; double vel, dur, strum, rel;
    int midi, slide, glide, open, ghost;
    int fx; double v, tau;
} Ev;
struct BarInfo { int si, sec, j, n, prog, prev; Layers L; };
#define MAXBARS 160              // house tracks reach 102 bars
struct BarState {
    BarInfo map[MAXBARS]; int nbars;
    Rng rH, rR, rM, rU, rS;
    Voicing prevVoicing; int hasPrevVoicing;
    int prevBass, pushed, nextComp;
    Phrase phrases[MAXSEC][4]; unsigned char hasPhrase[MAXSEC][4];
    int statement, lastLead, hasLastLead;
    double sx[16]; int si[16];               // a style's own bar-state scratch (their st.* extras)
    int lastFill, lastComp, lastPush;        // for the display (not part of the port)
};
static const double VINYL_BOOST = 1.585;

static void bar_state(const Plan *P, BarState *st) {
    memset(st, 0, sizeof *st);
    double lift = style_of(P)->keysLevel;
    for (int si = 0; si < P->nsec; si++) {
        Layers L = band_layers(P->sec[si].L, P->band, lift);
        for (int j = 0; j < P->sec[si].bars && st->nbars < MAXBARS; j++) {
            BarInfo b = { si, P->sec[si].name, j, P->sec[si].bars, P->sec[si].prog, si ? P->sec[si - 1].name : -1, L };
            st->map[st->nbars++] = b;
        }
    }
    st->rH = lc_stream(P->seed, 11); st->rR = lc_stream(P->seed, 12); st->rM = lc_stream(P->seed, 13);
    st->rU = lc_stream(P->seed, 16); st->rS = lc_stream(P->seed, 17);
    st->prevBass = 40; st->pushed = -1; st->nextComp = -1; st->lastFill = F_NONE;
}
static int chords_in(const Expanded *pr, int j, Chord *out) {
    double b0 = fmod(j * 4.0, pr->beats); int n = 0;
    for (int i = 0; i < pr->n; i++) {
        const Chord *c = &pr->c[i];
        double s = fmax(c->start, b0), e = fmin(c->start + c->beats, b0 + 4);
        if (e > s) { Chord o = { c->root, c->q, s - b0, e - s, c->start >= b0, 0 }; out[n++] = o; }
    }
    return n;
}
static int bar_chords(const Plan *P, const BarState *st, int i, Chord *out) {
    const BarInfo *b = &st->map[i < st->nbars - 1 ? i : st->nbars - 1];
    if (b->prog == PR_FINAL) {
        if (b->j < 2) return chords_in(&P->progA, b->j, out);
        int q = !P->key.major ? Q_M9 : (P->seed % 2 ? Q_MAJ9 : Q_N69);
        Chord c = { 0, q, 0, 4, b->j == 2, 1 }; out[0] = c; return 1;
    }
    return chords_in(b->prog == PR_B ? &P->progB : &P->progA, b->j, out);
}
static Pcs chord_pcs(const Plan *P, const Chord *c) {
    Pcs p; p.n = QUAL[c->q].n;
    for (int i = 0; i < p.n; i++) p.pc[i] = mod12(P->key.tonic + c->root + QUAL[c->q].iv[i]);
    return p;
}
static int same_chord(const Chord *a, const Chord *b) { return a && b && a->root == b->root && a->q == b->q; }
// pickComp over ep.comp (ids in their own order) → the COMP index it names
static int pick_comp(const Plan *P, Rng *r) {
    double sum = 0; for (int i = 0; i < P->ep.ncomp; i++) sum += P->ep.comp[i];
    double u = rn(r) * sum;
    for (int i = 0; i < P->ep.ncomp; i++) if ((u -= P->ep.comp[i]) < 0) return P->ep.compIdx[i];
    return P->ep.compIdx[P->ep.ncomp - 1];
}
// pickNotes: the voicing's notes named by index (negative = from the top)
static int pick_notes(const Voicing *v, const CompCell *cc, int *out) {
    int n = 0;
    for (int k = 0; k < cc->npick; k++) { int i = cc->pick[k] < 0 ? v->n + cc->pick[k] : cc->pick[k]; if (i >= 0 && i < v->n) out[n++] = v->m[i]; }
    return n;
}

// barHits — the drum grid for one bar, with its B-section variations and fills
typedef struct { Hit h[96]; int n, stop, fill; } BarHits;
static void hits_filter_not(BarHits *o, int (*keep)(const Hit *, int), int arg) {
    int k = 0; for (int i = 0; i < o->n; i++) if (keep(&o->h[i], arg)) o->h[k++] = o->h[i]; o->n = k;
}
static int keep_hatshaker(const Hit *h, int a) { (void)a; return h->v == V_HAT || h->v == V_SHAKER; }
static int keep_nokick(const Hit *h, int a) { (void)a; return h->v != V_KICK; }
static int keep_not_hat15(const Hit *h, int a) { (void)a; return !(h->v == V_HAT && h->s == 15); }
static int keep_not_v_ge12(const Hit *h, int v) { return !(h->v == v && h->s >= 12); }
static int keep_before(const Hit *h, int s) { return h->s < s; }
static int keep_not_hat_ge(const Hit *h, int s) { return !(h->v == V_HAT && h->s >= s); }
static BarHits bar_hits(const Plan *P, const BarInfo *b, int drums, int hatsFirst, Rng *r) {
    const StyleDef *S = style_of(P);
    BarHits o; o.n = 0; o.stop = -1; o.fill = F_NONE;
    int mode = drums;
    if (mode == DR_NONE) return o;
    if (mode == DR_HATS2 && b->j < b->n - 2) return o;
    if (mode == DR_OUTRO && b->j >= 2) return o;
    // mode P5 = the style's `break` grid, else the default P5 (rim); otherwise the planned pattern
    const Grid *grid = mode == DR_P5 ? (S->brk >= 0 ? &S->grids[S->brk] : &ST_JAZZHOP_GRIDS[4]) : &S->grids[P->pattern];
    Hits g = parse_grid(grid);
    double scale = (mode == DR_P5 ? 0.6 : 1) * P->drumLevel;
    for (int i = 0; i < g.n; i++) o.h[o.n++] = g.h[i];
    if (mode == DR_HATS2 || (hatsFirst && b->j == 0)) hits_filter_not(&o, keep_hatshaker, 0);
    if (mode == DR_OUTRO) hits_filter_not(&o, keep_nokick, 0);
    int groove = mode == DR_PATTERN || mode == DR_P5;
    if (groove && b->j > 0 && rn(r) < 0.08) hits_filter_not(&o, keep_nokick, 0);
    if (mode == DR_PATTERN && b->sec == S_B && S->bVariation) {
        if (b->j % 2 == 1) {
            int f = -1; for (int i = 0; i < o.n; i++) if (o.h[i].v == V_HAT && o.h[i].s == 14) { f = i; break; }
            if (f >= 0) o.h[f].open = 1;
            else { Hit h = { V_HAT, 14, 0, 1, 0, -1 }; o.h[o.n++] = h; }
            hits_filter_not(&o, keep_not_hat15, 0);
        }
        static const int SS[2] = { 7, 15 };
        for (int q = 0; q < 2; q++) {
            if (rn(r) < 0.3) {
                int has = 0; for (int i = 0; i < o.n; i++) if (o.h[i].v == V_SNARE && o.h[i].s == SS[q]) has = 1;
                if (!has) { Hit h = { V_SNARE, SS[q], 1, 0, 0, -1 }; o.h[o.n++] = h; }
            }
        }
    }
    if (groove && b->j < b->n) {
        int pos = b->j + 1; double p = pos % 8 == 0 ? 0.5 : pos % 4 == 0 ? 0.2 : 0;
        if (p && rn(r) < p) {
            int id = rfloor(r, S->nfills); o.fill = id;
            const Fill *f = &S->fills[id];
            if (f->hasRoll) {          // `if (f.roll)`: an EMPTY roll is still truthy in JS (house K1 = { roll: [], voice: "kick" })
                int rv = f->voice >= 0 ? f->voice : V_SNARE;
                hits_filter_not(&o, keep_not_v_ge12, rv);
                for (int k = 0; k < f->nroll; k++) { Hit h = { rv, f->rollS[k], 0, 0, 0, f->rollV[k] }; o.h[o.n++] = h; }
            }
            if (f->stop >= 0) { hits_filter_not(&o, keep_before, f->stop); o.stop = f->stop; }
            if (f->nk) {
                int kvv = f->voice >= 0 ? f->voice : V_KICK;
                for (int k = 0; k < f->nk; k++) {
                    int has = 0; for (int i = 0; i < o.n; i++) if (o.h[i].v == kvv && o.h[i].s == f->kicks[k]) has = 1;
                    if (!has) { Hit h = { kvv, f->kicks[k], 0, 0, 0, 0.7 }; o.h[o.n++] = h; }
                }
            }
            if (f->open >= 0) { hits_filter_not(&o, keep_not_hat_ge, f->open); Hit h = { V_HAT, f->open, 0, 1, 0, -1 }; o.h[o.n++] = h; }
        }
    }
    for (int i = 0; i < o.n; i++) {
        Hit *h = &o.h[i];
        double v = h->vel >= 0 ? h->vel : VEL[h->v];
        if (h->v == V_HAT) v *= h->open ? 0.95 : HAT_ACCENT[h->s % 4];
        if (h->v == V_KICK && h->s != 0 && h->vel < 0) v *= 0.9;
        if (h->acc) v *= h->acc;
        if (h->ghost) v = 0.25 + 0.1 * rn(r);
        h->vel = v * scale;
    }
    for (int i = 1; i < o.n; i++) { Hit x = o.h[i]; int j = i - 1; while (j >= 0 && o.h[j].s > x.s) { o.h[j + 1] = o.h[j]; j--; } o.h[j + 1] = x; }
    return o;
}

typedef struct { const Plan *P; const BarState *st; int startBar; } LeadCtx;
static Pcs lead_pcs_at(void *vctx, int s) {
    LeadCtx *c = vctx; Chord cs[14];
    int n = bar_chords(c->P, c->st, c->startBar + s / 16, cs);
    double beat = (s % 16) / 4.0; int k = 0;
    for (int i = 0; i < n; i++) if (beat >= cs[i].start && beat < cs[i].start + cs[i].beats) { k = i; break; }
    return chord_pcs(c->P, &cs[k]);
}

static const double SIG[NDRUMV] = { 3e-3, 4e-3, 6e-3, 4e-3, 5e-3, 5e-3, 5e-3, 5e-3 };
static int LAGGED(int v) { return v == V_SNARE || v == V_RIM || v == V_CLAP; }

typedef struct { Ev e[200]; int n; } Evs;
static Ev *push_ev(Evs *E, double t, int k, int bar) { Ev *x = &E->e[E->n++]; memset(x, 0, sizeof *x); x->t = t; x->k = k; x->bar = bar; return x; }
typedef struct { const Plan *P; BarState *st; double bar0, sd; } BarCtx;
static double gauss_(BarState *st) { return (rn(&st->rU) + rn(&st->rU) + rn(&st->rU) - 1.5) * 2; }
static double at_(BarCtx *c, double s, double sigma, double lag) {
    return fmax(0, c->bar0 + s * c->sd + lc_swing((int)s, c->sd, c->P->swing) + lag + gauss_(c->st) * sigma);
}
static double vh_(BarState *st, double v) { return clampd(v * (1 + (rn(&st->rU) * 2 - 1) * 0.08), 0.05, 1); }

// planBar — every event of bar i: fx automation, keys comping (+ the push), drums, bass, lead
static void plan_bar(const Plan *P, int i, BarState *st, Evs *out) {
    out->n = 0;
    const StyleDef *S = style_of(P); const StyleHooks *H = style_hooks(P->style);
    const BarInfo *b = &st->map[i]; const Layers *L = &b->L;
    double sd = 60.0 / P->bpm / 4, bar0 = i * 16 * sd, barDur = 16 * sd;
    BarCtx cx = { P, st, bar0, sd };
    Chord chords[14]; int nch = bar_chords(P, st, i, chords);
    Chord nextC[14]; int nnext = i + 1 < st->nbars ? bar_chords(P, st, i + 1, nextC) : 0;
#define FX(type, val, tau_, dt_) do { Ev *x_ = push_ev(out, bar0 + (dt_), K_FX, i); x_->fx = type; x_->v = val; x_->tau = tau_; } while (0)
    if (i == 0) { FX(FX_DUST_, P->dust, 1, 0); FX(FX_WOW_, 1, 1, 0); FX(FX_VINYL_, VINYL_BOOST, 0.5, 0); FX(FX_TONE_, 900, 0.02, 0); }
    else if (L->level != st->map[i - 1].L.level) FX(FX_LEVEL_, L->level, 0.4, 0);
#define DRIFT() (P->tone * (1 + (rn(&st->rH) * 2 - 1) * 0.12))
    if (b->sec == S_INTRO) FX(FX_TONE_, 900 * pow(P->tone / 900, (b->j + 1) / (double)b->n), barDur / 3, i == 0 ? 0.12 : 0);
    else if (b->j == 0) {
        if (b->sec == S_BREAK) FX(FX_TONE_, 1800, 0.4, 0);
        else if (b->sec != S_OUTRO) { double d = DRIFT(); FX(FX_TONE_, d, b->prev == S_BREAK ? 0.8 : 0.3, 0); }
        if (b->prev == S_INTRO) FX(FX_VINYL_, 1, 2, 0);
    } else if (b->j % 4 == 0 && (b->sec == S_A || b->sec == S_B)) { double d = DRIFT(); FX(FX_TONE_, d, barDur, 0); }
    if (b->j % 2 == 0) { double w = 0.6 + rn(&st->rH) * 0.8; FX(FX_WOW_, w, 1, 0); }
    if (L->dipLast && b->j == b->n - 1) FX(FX_TONE_, 500, 0.25, 0);
    if (b->sec == S_OUTRO && b->j == b->n - 2) { FX(FX_TONE_, 700, barDur * 0.6, 0); FX(FX_VINYL_, VINYL_BOOST, 1.5, 0); }
#undef DRIFT
#undef FX
    // ── keys: the comp hits [step, len, chord, vel-scale, release, pick] ──
    struct { double s, len; Chord c; double vs, rel; const CompCell *pick; } hits[24]; int nh = 0;
#define HIT(s_, len_, c_, vs_, rel_, pk_) do { hits[nh].s = s_; hits[nh].len = len_; hits[nh].c = c_; hits[nh].vs = vs_; hits[nh].rel = rel_; hits[nh].pick = pk_; nh++; } while (0)
    double vel = P->ep.vel;
    st->lastComp = -1; st->lastPush = 0;
    if (L->ep == EP_INTRO || L->ep == EP_WHOLE) {
        for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], L->ep == EP_INTRO ? 0.85 : 0.8, 0.12, NULL);
    } else if (L->ep == EP_OUTRO) {
        if (b->j < 2) for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], 0.9, 0.1, NULL);
        else if (b->j == 2) HIT(0, (b->n - 2) * 16, chords[0], 0.85, 0.9, NULL);
    } else if (S->comps) {
        int ci = st->nextComp >= 0 ? st->nextComp : pick_comp(P, &st->rH); st->lastComp = ci;
        const Comp *cells = &S->comps[ci];
        int pushed = st->pushed == i;
        st->nextComp = -1;
        if (pushed) st->pushed = -1;
        for (int k = 0; k < nch; k++) {
            double c0 = chords[k].start * 4, c1 = c0 + chords[k].beats * 4;
            int own = 0;
            for (int q = 0; q < cells->n; q++) { double s = cells->c[q].s; if (s >= c0 && s < c1 && !(pushed && s == 0)) own++; }
            if (!own && !(pushed && c0 == 0)) HIT(c0, c1 - c0, chords[k], 1, 0.08, NULL);
            for (int q = 0; q < cells->n; q++) {
                const CompCell *cc = &cells->c[q]; double s = cc->s;
                if (!(s >= c0 && s < c1 && !(pushed && s == 0))) continue;
                HIT(s, fmin(cc->len, c1 - s), chords[k], cc->v, 0.08, cc->npick >= 0 ? cc : NULL);
            }
        }
    } else if (nch > 1 || st->pushed == i) {
        for (int k = 0; k < nch; k++) HIT(chords[k].start * 4, chords[k].beats * 4, chords[k], 1, 0.08, NULL);
    } else {
        int ci = pick_comp(P, &st->rH); st->lastComp = ci;
        for (int k = 0; k < COMPN[ci]; k++) {
            int s = COMPS[ci][k][0], len = COMPS[ci][k][1];
            HIT(s, k == COMPN[ci] - 1 ? 16 - s : len, chords[0], k ? 0.9 : 1, 0.08, NULL);
        }
    }
    if (st->pushed == i) { if (nh) { memmove(&hits[0], &hits[1], sizeof hits[0] * (nh - 1)); nh--; } st->pushed = -1; }
    const Chord *last = &chords[nch - 1];
    if (L->ep == EP_COMP && nnext && st->map[i + 1].L.ep == EP_COMP && !same_chord(last, &nextC[0]) && rn(&st->rH) < P->ep.push) {
        if (nh && hits[nh - 1].s < 14) {
            for (int k = 0; k < nh; k++) hits[k].len = fmin(hits[k].len, 14 - hits[k].s);
            double len = 2 + nextC[0].beats * 4;
            if (S->comps) {
                st->nextComp = pick_comp(P, &st->rH);
                const Comp *nc = &S->comps[st->nextComp];
                for (int q = 0; q < nc->n; q++) if (nc->c[q].s > 0 && nc->c[q].s < nextC[0].beats * 4) { len = 2 + nc->c[q].s; break; }
            }
            HIT(14, len, nextC[0], 0.95, 0.08, NULL);
            st->pushed = i + 1; st->lastPush = 1;
        }
    }
#undef HIT
    for (int k = 0; k < nh; k++) {
        Spelled sp = lc_spell(P->key, &hits[k].c);
        const Voicing *pv = st->hasPrevVoicing ? &st->prevVoicing : NULL;
        Voicing v = H && H->voicing ? H->voicing(&sp, pv, &hits[k].c, P, st) : lc_voice(&sp, pv, P->reg);
        st->prevVoicing = v; st->hasPrevVoicing = 1;
        int notes[8], nn;
        if (hits[k].pick) nn = pick_notes(&v, hits[k].pick, notes);
        else { nn = v.n; memcpy(notes, v.m, sizeof(int) * v.n); }
        if (!nn) continue;
        double t = at_(&cx, hits[k].s, 4e-3, 0);
        Ev *x = push_ev(out, t, K_EP, i);
        x->nn = nn; memcpy(x->notes, notes, sizeof(int) * nn);
        x->vel = vh_(st, vel * hits[k].vs);
        x->dur = fmax(0.1, hits[k].len * sd - 0.03);
        x->strum = P->ep.strum; x->rel = hits[k].rel;
    }
    // ── drums ──
    BarHits dh = bar_hits(P, b, L->drums, L->hatsFirst, &st->rR);
    st->lastFill = dh.fill;
    for (int k = 0; k < dh.n; k++) {
        const Hit *h = &dh.h[k];
        double lag = LAGGED(h->v) ? P->snareLag : 0;
        double t = at_(&cx, h->s, SIG[h->v], lag);
        double vv = vh_(st, h->vel);
        if (L->kit) { Ev *x = push_ev(out, t, K_KICK + h->v, i); x->vel = vv; x->open = h->open; x->ghost = h->ghost; }
    }
    // ── bass: the style's own line, else on the kicks + the changes with an approach ──
    int bm = L->bass;
    int bassOn = bm == BS_KICK || bm == BS_WHOLE || (bm == BS_LAST && b->j == b->n - 1) || (bm == BS_OUTRO && b->j < 3);
    int kicks[40], nk = 0;
    for (int k = 0; k < dh.n; k++) if (dh.h[k].v == V_KICK) kicks[nk++] = dh.h[k].s;
    int styled = -1; BassNote line[48];
    if (bassOn && H && H->bass) {
        BassCtx bc = { P, st, i, b, bm, chords, nch, nnext ? nextC : NULL, nnext, kicks, nk, dh.stop, &st->rR };
        styled = H->bass(&bc, line);
    }
    if (styled >= 0) {
        for (int k = 0; k < styled; k++) {
            double t = at_(&cx, line[k].s, 4e-3, 3e-3);
            Ev *x = push_ev(out, t, K_BASS, i);
            x->midi = line[k].midi; x->vel = vh_(st, line[k].vel >= 0 ? line[k].vel : 0.82);
            x->dur = fmax(0.25, line[k].dur) * sd; x->slide = line[k].slide;
        }
    } else if (bassOn) {
        int steps[48], ns = 0, changes[14], ncg = 0;
        if (bm == BS_KICK) for (int k = 0; k < nk; k++) steps[ns++] = kicks[k];
        for (int k = 0; k < nch; k++) changes[ncg++] = (int)(chords[k].start * 4);
        for (int k = 0; k < ncg; k++) steps[ns++] = changes[k];
        { int u[48], nu = 0; for (int k = 0; k < ns; k++) { int d = 0; for (int q = 0; q < nu; q++) if (u[q] == steps[k]) d = 1; if (!d) u[nu++] = steps[k]; }
          isort(u, nu); memcpy(steps, u, sizeof(int) * nu); ns = nu; }
        int approach = 0;
        if (bm == BS_KICK && nnext && nextC[0].root != last->root && rn(&st->rR) < 0.5) {
            int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < 14) steps[k2++] = steps[k]; ns = k2;
            steps[ns++] = 14; approach = 1;
        }
        if (dh.stop >= 0) { int k2 = 0; for (int k = 0; k < ns; k++) if (steps[k] < dh.stop) steps[k2++] = steps[k]; ns = k2; }
        int endStep = dh.stop >= 0 ? dh.stop : 16;
        for (int k = 0; k < ns; k++) {
            int s = steps[k];
            const Chord *c = &chords[nch - 1];
            for (int q = 0; q < nch; q++) if (s >= chords[q].start * 4 && s < (chords[q].start + chords[q].beats) * 4) { c = &chords[q]; break; }
            int rootPc = mod12(P->key.tonic + c->root);
            int root = bass_note(rootPc, st->prevBass), m = root;
            int isChange = 0; for (int q = 0; q < ncg; q++) if (changes[q] == s) isChange = 1;
            if (approach && s == 14) {
                int target = bass_note(mod12(P->key.tonic + nextC[0].root), root);
                m = rn(&st->rR) < 0.6 ? target - 1 : target + 1;
            } else if (!isChange && s % 8 != 0 && rn(&st->rR) < 0.25) {
                m = rn(&st->rR) < 0.6 ? (root + 7 <= 50 ? root + 7 : root - 5) : (root + 12 <= 52 ? root + 12 : root);
            }
            int nextS = k + 1 < ns ? steps[k + 1] : endStep;
            double dur = fmax(1, nextS - s) * sd * P->bassFactor;
            double t = at_(&cx, s, 4e-3, 3e-3);
            Ev *x = push_ev(out, t, K_BASS, i);
            x->midi = m; x->vel = vh_(st, s == 0 ? 0.92 : 0.82); x->dur = dur;
            x->slide = rn(&st->rR) < 0.15;
            st->prevBass = root;
        }
    }
    // ── lead: one motif, stated per 4-bar group, inverted + shifted in B, answered ──
    if (P->hasLead && L->lead) {
        int g = b->j / 4;
        if (!st->hasPhrase[b->si][g]) {
            int startBar = i - (b->j % 4);
            int rest = g % 2 == 1 && rn(&st->rM) < 0.3;
            Phrase ph; ph.cnt = 0;
            if (!rest) {
                LeadCtx lc = { P, st, startBar };
                Variant va = { ++st->statement, b->sec == S_B, b->sec == S_B ? 2 : 0, st->hasLastLead, st->lastLead, P->reg };
                ph = lc_phrase(&P->motif, lead_pcs_at, &lc, P->key, &st->rM, va);
            }
            if (ph.cnt) { st->lastLead = ph.n[ph.cnt - 1].midi; st->hasLastLead = 1; }
            st->phrases[b->si][g] = ph; st->hasPhrase[b->si][g] = 1;
        }
        const Phrase *ph = &st->phrases[b->si][g]; int bj = b->j % 4;
        for (int k = 0; k < ph->cnt; k++) {
            const LNote *n = &ph->n[k];
            if (n->s / 16 != bj) continue;
            const LNote *p = k ? &ph->n[k - 1] : NULL;
            int glide = p && n->s - (p->s + p->d) <= 1;
            double t = at_(&cx, n->s % 16, 8e-3, 0);
            Ev *x = push_ev(out, t, K_LEAD, i);
            x->midi = n->midi; x->vel = vh_(st, n->v); x->dur = n->d * sd * 0.95; x->glide = glide;
        }
    }
    // stable sort by time (JS Array.sort is stable)
    for (int a = 1; a < out->n; a++) { Ev x = out->e[a]; int j = a - 1; while (j >= 0 && out->e[j].t > x.t) { out->e[j + 1] = out->e[j]; j--; } out->e[j + 1] = x; }
}

// ═══ the style ports: each style's plan / voicing / bass, line for line ═══
#include "styles.h"
#endif // LOFICITY_ARRANGER_H
