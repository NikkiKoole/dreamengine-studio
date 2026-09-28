// runtime/loficity/style_ambient.h — ported from styles/ambient.js, verified bit-exact against the oracle.
// ── style: ambient (styles/ambient.js) — slow warm pads and long chords, barely a beat ──
// voicing = a 5-note PAD (a bass-ish root/fifth under four open upper tones, chosen by cost);
// bass = one sub note per chord (a 5th in the 2nd bar of a held note, by energy); plan = the pad's
// envelope + chorus/shimmer/wash, the drone odds, no drums when chill, and a SPARSE lead motif.

static int amb_gap_ok(int lo, int hi) { return hi - lo >= (lo < 60 ? 3 : 2) && hi - lo <= 9; }
typedef struct { int v[5]; double mean; int ninth; } AmbCand;
#define AMB_MAXC 8192
// padCands(root, q): every [b, i, j, k, l] with the colour tones the chord needs (pure; theirs is memoised)
static int amb_pad_cands(int root, int q, AmbCand *out) {
    int iv[7], n = 0;
    for (int i = 0; i < QUAL[q].n; i++) { int v = mod12(QUAL[q].iv[i]), d = 0; for (int j = 0; j < n; j++) if (iv[j] == v) d = 1; if (!d) iv[n++] = v; }
#define HAS(x) ({ int h_ = 0; for (int q_ = 0; q_ < n; q_++) if (iv[q_] == (x)) h_ = 1; h_; })
    int pcs[7]; for (int i = 0; i < n; i++) pcs[i] = mod12(root + iv[i]);
    int need[3], nn = 0;
    { static const int A[4] = { 4, 3, 2, 5 }; for (int k = 0; k < 4; k++) if (HAS(A[k])) { need[nn++] = mod12(root + A[k]); break; } }
    { static const int B[3] = { 11, 10, 9 }; for (int k = 0; k < 3; k++) if (HAS(B[k])) { need[nn++] = mod12(root + B[k]); break; } }
    if (HAS(6)) need[nn++] = mod12(root + 6); else if (HAS(3) && HAS(5)) need[nn++] = mod12(root + 5);
    int ninth = HAS(2) ? mod12(root + 2) : -1;
    int b0s[2] = { 0, 7 }, nb0 = HAS(7) ? 2 : 1, nc = 0;
#undef HAS
    for (int bi = 0; bi < nb0; bi++) {
        int b = 47 + mod12(root + b0s[bi] - 47), pool[64], n2 = 0;
        for (int m = b + 5; m <= 84; m++) { int ok = 0; for (int t = 0; t < n; t++) if (pcs[t] == m % 12) ok = 1; if (ok) pool[n2++] = m; }
        for (int i = 0; i < n2 && pool[i] - b <= 12; i++)
            for (int j = i + 1; j < n2 && pool[j] - pool[i] <= 9; j++) {
                if (!amb_gap_ok(pool[i], pool[j])) continue;
                for (int k = j + 1; k < n2 && pool[k] - pool[j] <= 9; k++) {
                    if (!amb_gap_ok(pool[j], pool[k])) continue;
                    for (int l = k + 1; l < n2 && pool[l] <= 84 && pool[l] - pool[k] <= 9; l++) {
                        if (!amb_gap_ok(pool[k], pool[l]) || pool[l] < 67) continue;
                        int v[5] = { b, pool[i], pool[j], pool[k], pool[l] }, pc[5], nd = 0;
                        for (int t = 0; t < 5; t++) { pc[t] = v[t] % 12; int d = 0; for (int u = 0; u < t; u++) if (pc[u] == pc[t]) d = 1; if (!d) nd++; }
                        int all = 1;
                        for (int t = 0; t < nn; t++) { int f = 0; for (int u = 0; u < 5; u++) if (pc[u] == need[t]) f = 1; if (!f) all = 0; }
                        if (nd < 4 || !all) continue;
                        if (nc >= AMB_MAXC) continue;
                        AmbCand *c = &out[nc++]; memcpy(c->v, v, sizeof v);
                        c->mean = (double)(((((0 + v[0]) + v[1]) + v[2]) + v[3]) + v[4]) / 5;
                        int hn = 0; if (ninth >= 0) for (int u = 0; u < 5; u++) if (pc[u] == ninth) hn = 1;
                        c->ninth = hn;
                    }
                }
            }
    }
    return nc;
}
static Voicing amb_voicing(const Spelled *sp, const Voicing *prev, const Chord *c, const Plan *P, BarState *st) {
    (void)st;
    static AmbCand cands[AMB_MAXC];
    int nc = amb_pad_cands(mod12(P->key.tonic + c->root), c->q, cands);
    if (!nc) return lc_voice(sp, prev, P->reg);
    if (prev && prev->n == 5)
        for (int k = 0; k < nc; k++) { int eq = 1; for (int i = 0; i < 5; i++) if (cands[k].v[i] != prev->m[i]) eq = 0; if (eq) return *prev; }
    int r = prev ? 0 : P->reg;
    const int *best = cands[0].v; double bc = INFINITY;
    for (int q = 0; q < nc; q++) {
        const int *v = cands[q].v;
        double k = 0.3 * fabs(cands[q].mean - 64 - r) + 1.5 * fmax(fmax(0, v[4] - 79 - r), 71 + r - v[4]) - (cands[q].ninth ? 1.5 : 0);
        if (prev) {
            int mn = prev->n < 5 ? prev->n : 5;
            for (int i = 0; i < mn; i++) k += abs(v[i] - prev->m[i]);
            k += 2 * fmax(0, abs(v[4] - prev->m[prev->n - 1]) - 3);
        }
        if (k < bc) { bc = k; best = v; }
    }
    Voicing o; o.n = 5; memcpy(o.m, best, sizeof(int) * 5);
    return o;
}
static const double AMB_ATK[3][2] = { { 2.2, 3.2 }, { 1.5, 2.4 }, { 0.9, 1.6 } };
static const double AMB_REL[3][2] = { { 1.2, 1.6 }, { 1, 1.35 }, { 0.75, 1.05 } };
static const double AMB_DRONE[3] = { 0.75, 0.5, 0.3 };
static const double AMB_MOVE[3] = { 0, 0.12, 0.3 };
static const int AMB_SPARSE[6][3] = { { 0, 12 }, { 0, 8, 20 }, { 4, 16 }, { 0, 10, 22 }, { 2, 14, 24 }, { 0, 6, 20 } };
static const int AMB_SPARSEN[6] = { 2, 3, 2, 3, 3, 3 };
static void amb_plan(Plan *P, Rng *r, const Energy *E, int energy) {
    (void)E;
    int e = energy < 0 ? 0 : energy;
    kv_set(P, "ep.pad.atk", rnd2(r, AMB_ATK[e][0], AMB_ATK[e][1], 2));
    kv_set(P, "ep.pad.rel", rnd2(r, AMB_REL[e][0], AMB_REL[e][1], 2));
    kv_set(P, "ep.pad.det", rnd2(r, 5, 12, 1));
    kv_set(P, "ep.pad.shim", rnd2(r, 0.08, 0.16, 2));
    kv_set(P, "ep.pad.lfo", rnd2(r, 0.05, 0.16, 3));
    kv_set(P, "ep.pad.depth", P->ep.tremDepth);
    kv_set(P, "ep.pad.width", rnd2(r, 0.3, 0.55, 2));
    kv_set(P, "ep.pad.wash", rnd2(r, 0.18, 0.3, 2));
    kv_set(P, "ep.pad.drone", rn(r) < AMB_DRONE[e] ? 1 : 0);
    kv_set(P, "bass.move", AMB_MOVE[e]);
    if (energy == EN_CHILL) for (int i = 0; i < P->nsec; i++) P->sec[i].L.drums = DR_NONE;
    if (P->hasLead) {
        Motif *m = &P->motif;
        int si = rfloor(r, 6), n2 = AMB_SPARSEN[si];
        const int *steps = AMB_SPARSE[si];
        for (int i = 0; i < n2; i++) m->steps[i] = steps[i];
        for (int i = 0; i < n2; i++) m->vel[i] = i ? rnd2(r, 0.58, 0.68, 2) : 0.72;
        for (int i = 0; i < n2; i++) m->durs[i] = i < n2 - 1 ? (8 < steps[i + 1] - steps[i] ? 8 : steps[i + 1] - steps[i]) : 8 + rfloor(r, 5);
        m->n = n2;                                   // contour.slice(0, n2): the first n2 are kept as they are
    }
}
static int amb_bass(BassCtx *cx, BassNote *out) {
    const Plan *P = cx->plan; BarState *st = cx->st; int n = 0;
    for (int k = 0; k < cx->nch; k++) {
        const Chord *c = &cx->chords[k];
        int s = (int)(c->start * 4); double len = c->beats * 4;
        int m = bass_note(mod12(P->key.tonic + c->root), st->prevBass);
#define ADD(s_, m_, v_, d_) do { out[n].s = (s_); out[n].midi = (m_); out[n].vel = (v_); out[n].dur = (d_); out[n].slide = 0; n++; } while (0)
        if (cx->b->sec == S_OUTRO && cx->b->j == 2) ADD(s, m, 0.8, 30);
        else if (cx->mode == BS_KICK && !c->onset && len == 16 && rn(&st->rS) < kv(P, "bass.move")) {
            ADD(s, m, 0.74, 8);
            ADD(s + 8, m + 7 <= 50 ? m + 7 : m - 5, 0.7, 8);
        } else ADD(s, m, c->onset ? 0.8 : 0.76, len);
#undef ADD
        st->prevBass = m;
    }
    return n;
}
